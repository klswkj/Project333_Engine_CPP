#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

#include "../Source/Core/Public/Memory/Container/LockFreeQueue.h"
#include "../Source/Core/Public/Memory/Memory.h"

class LockFreeQueueTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		Core::Memory::Initialize();
	}

	void TearDown() override
	{
		Core::Memory::Shutdown();
	}
};

TEST_F(LockFreeQueueTest, MPSCStressMultipleProducersSingleConsumer)
{
	constexpr size_t ProducerCount = 4;
	constexpr size_t ValuesPerProducer = 5000;
	constexpr size_t TotalValueCount = ProducerCount * ValuesPerProducer;
	constexpr size_t QueueCapacity = 128;

	Core::MPSCQueue<size_t, QueueCapacity> Queue;

	std::atomic<bool> bStart = false;
	std::atomic<bool> bStop = false;
	std::atomic<size_t> ProducersDone = 0;
	std::atomic<size_t> DuplicateCount = 0;
	std::atomic<size_t> InvalidCount = 0;
	std::atomic<size_t> OrderViolationCount = 0;
	std::atomic<size_t> ConsumedCount = 0;

	std::vector<std::thread> Producers;
	Producers.reserve(ProducerCount);

	for (size_t ProducerIndex = 0; ProducerIndex < ProducerCount; ++ProducerIndex)
	{
		Producers.emplace_back
		(
			[&Queue, &bStart, &bStop, &ProducersDone, ProducerIndex]()
			{
				while (false == bStart.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}

				for (size_t ValueIndex = 0; ValueIndex < ValuesPerProducer; ++ValueIndex)
				{
					const size_t Value = ProducerIndex * ValuesPerProducer + ValueIndex + 1;

					while (false == bStop.load(std::memory_order_acquire)
						&& false == Queue.Enqueue(Value))
					{
						std::this_thread::yield();
					}
				}

				ProducersDone.fetch_add(1, std::memory_order_release);
			}
		);
	}

	std::thread Consumer
	(
		[&]()
		{
			std::vector<bool> SeenValues(TotalValueCount + 1, false);
			constexpr size_t InvalidValueIndex = static_cast<size_t>(-1);

			std::vector<size_t> LastValueIndexByProducer(ProducerCount, InvalidValueIndex);

			while (false == bStart.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}

			while (ConsumedCount.load(std::memory_order_acquire) < TotalValueCount
				&& false == bStop.load(std::memory_order_acquire))
			{
				size_t Value = 0;

				if (true == Queue.Dequeue(Value))
				{
					if (0 == Value || TotalValueCount < Value)
					{
						InvalidCount.fetch_add(1, std::memory_order_relaxed);
					}
					else
					{
						if (true == SeenValues[Value])
						{
							DuplicateCount.fetch_add(1, std::memory_order_relaxed);
						}

						SeenValues[Value] = true;

						const size_t ProducerIndex = (Value - 1) / ValuesPerProducer;
						const size_t ValueIndex = (Value - 1) % ValuesPerProducer;

						if (InvalidValueIndex != LastValueIndexByProducer[ProducerIndex]
							&& ValueIndex <= LastValueIndexByProducer[ProducerIndex])
						{
							OrderViolationCount.fetch_add(1, std::memory_order_relaxed);
						}

						LastValueIndexByProducer[ProducerIndex] = ValueIndex;
					}

					ConsumedCount.fetch_add(1, std::memory_order_release);
				}
				else
				{
					std::this_thread::yield();
				}
			}

			size_t SeenCount = 0;

			for (size_t Value = 1; Value <= TotalValueCount; ++Value)
			{
				if (true == SeenValues[Value])
				{
					++SeenCount;
				}
			}

			EXPECT_EQ(TotalValueCount, SeenCount);
		}
	);

	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	bStart.store(true, std::memory_order_release);

	while (ConsumedCount.load(std::memory_order_acquire) < TotalValueCount
		&& std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	const bool bTimedOut = ConsumedCount.load(std::memory_order_acquire) < TotalValueCount;
	bStop.store(true, std::memory_order_release);

	for (std::thread& Producer : Producers)
	{
		Producer.join();
	}

	Consumer.join();

	EXPECT_FALSE(bTimedOut);
	EXPECT_EQ(ProducerCount, ProducersDone.load(std::memory_order_acquire));
	EXPECT_EQ(TotalValueCount, ConsumedCount.load(std::memory_order_acquire));
	EXPECT_EQ(0, DuplicateCount.load(std::memory_order_acquire));
	EXPECT_EQ(0, InvalidCount.load(std::memory_order_acquire));
	EXPECT_EQ(0, OrderViolationCount.load(std::memory_order_acquire));
}

TEST_F(LockFreeQueueTest, SPMCStressSingleProducerMultipleConsumers)
{
	constexpr size_t ConsumerCount = 4;
	constexpr size_t TotalValueCount = 20000;
	constexpr size_t QueueCapacity = 128;

	Core::SPMCQueue<size_t, QueueCapacity> Queue;

	std::atomic<bool> bStart = false;
	std::atomic<bool> bStop = false;
	std::atomic<bool> bProducerDone = false;
	std::atomic<size_t> ConsumedCount = 0;
	std::atomic<size_t> DuplicateCount = 0;
	std::atomic<size_t> InvalidCount = 0;

	std::unique_ptr<std::atomic<unsigned char>[]> SeenValues =
		std::make_unique<std::atomic<unsigned char>[]>(TotalValueCount + 1);

	for (size_t Index = 0; Index <= TotalValueCount; ++Index)
	{
		SeenValues[Index].store(0, std::memory_order_relaxed);
	}

	std::thread Producer
	(
		[&]()
		{
			while (false == bStart.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}

			for (size_t Value = 1; Value <= TotalValueCount; ++Value)
			{
				while (false == bStop.load(std::memory_order_acquire)
					&& false == Queue.Enqueue(Value))
				{
					std::this_thread::yield();
				}
			}

			bProducerDone.store(true, std::memory_order_release);
		}
	);

	std::vector<std::thread> Consumers;
	Consumers.reserve(ConsumerCount);

	for (size_t ConsumerIndex = 0; ConsumerIndex < ConsumerCount; ++ConsumerIndex)
	{
		Consumers.emplace_back
		(
			[&]()
			{
				while (false == bStart.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}

				while (ConsumedCount.load(std::memory_order_acquire) < TotalValueCount
					&& false == bStop.load(std::memory_order_acquire))
				{
					size_t Value = 0;

					if (true == Queue.Dequeue(Value))
					{
						if (0 == Value || TotalValueCount < Value)
						{
							InvalidCount.fetch_add(1, std::memory_order_relaxed);
						}
						else
						{
							unsigned char Expected = 0;

							if (false == SeenValues[Value].compare_exchange_strong
							(
								Expected,
								1,
								std::memory_order_acq_rel,
								std::memory_order_relaxed
							))
							{
								DuplicateCount.fetch_add(1, std::memory_order_relaxed);
							}
						}

						ConsumedCount.fetch_add(1, std::memory_order_release);
					}
					else
					{
						if (true == bProducerDone.load(std::memory_order_acquire)
							&& TotalValueCount <= ConsumedCount.load(std::memory_order_acquire))
						{
							break;
						}

						std::this_thread::yield();
					}
				}
			}
		);
	}

	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	bStart.store(true, std::memory_order_release);

	while (ConsumedCount.load(std::memory_order_acquire) < TotalValueCount
		&& std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	const bool bTimedOut = ConsumedCount.load(std::memory_order_acquire) < TotalValueCount;
	bStop.store(true, std::memory_order_release);

	Producer.join();

	for (std::thread& Consumer : Consumers)
	{
		Consumer.join();
	}

	size_t SeenCount = 0;

	for (size_t Value = 1; Value <= TotalValueCount; ++Value)
	{
		if (0 != SeenValues[Value].load(std::memory_order_acquire))
		{
			++SeenCount;
		}
	}

	EXPECT_FALSE(bTimedOut);
	EXPECT_EQ(TotalValueCount, ConsumedCount.load(std::memory_order_acquire));
	EXPECT_EQ(TotalValueCount, SeenCount);
	EXPECT_EQ(0, DuplicateCount.load(std::memory_order_acquire));
	EXPECT_EQ(0, InvalidCount.load(std::memory_order_acquire));
}
