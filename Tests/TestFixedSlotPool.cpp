#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "../Source/Core/Public/Memory/Container/FixedSlotPool.h"
#include "../Source/Core/Public/Memory/Memory.h"

class FixedSlotPoolTest : public ::testing::Test
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

struct FPoolNonMovable
{
	static int ConstructCount;
	static int DestructCount;

	int Value = 0;

	FPoolNonMovable() = delete;

	explicit FPoolNonMovable(int InputValue)
		: Value(InputValue)
	{
		++ConstructCount;
	}

	~FPoolNonMovable()
	{
		++DestructCount;
	}

	FPoolNonMovable(const FPoolNonMovable& ) = delete;
	FPoolNonMovable(      FPoolNonMovable&&) = delete;

	FPoolNonMovable& operator=(const FPoolNonMovable& ) = delete;
	FPoolNonMovable& operator=(      FPoolNonMovable&&) = delete;

	static void Reset()
	{
		ConstructCount = 0;
		DestructCount  = 0;
	}
};

int FPoolNonMovable::ConstructCount = 0;
int FPoolNonMovable::DestructCount  = 0;

TEST_F(FixedSlotPoolTest, AcquiresLowestAvailableSlotsFirst)
{
	Core::FixedSlotPool<int, 3> Pool;

	EXPECT_TRUE(Pool.IsEmpty());

	size_t FirstIndex = 0;
	size_t SecondIndex = 0;

	int* FirstValue = Pool.Acquire(FirstIndex, 10);
	int* SecondValue = Pool.Acquire(SecondIndex, 20);

	ASSERT_NE(nullptr, FirstValue);
	ASSERT_NE(nullptr, SecondValue);

	EXPECT_EQ(0, FirstIndex);
	EXPECT_EQ(1, SecondIndex);
	EXPECT_EQ(10, *Pool.Get(FirstIndex));
	EXPECT_EQ(20, *Pool.Get(SecondIndex));
	EXPECT_EQ(1, Pool.GetFreeCount());
	EXPECT_FALSE(Pool.IsEmpty());

	Pool.Release(SecondIndex);
	Pool.Release(FirstIndex);

	EXPECT_TRUE(Pool.IsEmpty());
}

TEST_F(FixedSlotPoolTest, ReturnsNullWhenFullAndReusesReleasedSlot)
{
	Core::FixedSlotPool<int, 2> Pool;

	size_t FirstIndex = 0;
	size_t SecondIndex = 0;
	size_t ThirdIndex = 0;

	int* FirstValue = Pool.Acquire(FirstIndex, 10);
	int* SecondValue = Pool.Acquire(SecondIndex, 20);
	int* ThirdValue = Pool.Acquire(ThirdIndex, 30);

	ASSERT_NE(nullptr, FirstValue);
	ASSERT_NE(nullptr, SecondValue);
	EXPECT_EQ(nullptr, ThirdValue);
	EXPECT_EQ(0, Pool.GetFreeCount());

	Pool.Release(FirstIndex);
	EXPECT_EQ(1, Pool.GetFreeCount());

	int* ReusedValue = Pool.Acquire(ThirdIndex, 30);

	ASSERT_NE(nullptr, ReusedValue);
	EXPECT_EQ(FirstIndex, ThirdIndex);
	EXPECT_EQ(30, *Pool.Get(ThirdIndex));

	Pool.Release(SecondIndex);
	Pool.Release(ThirdIndex);
}

TEST_F(FixedSlotPoolTest, SupportsNonDefaultConstructibleNonMovableTypes)
{
	FPoolNonMovable::Reset();

	{
		Core::FixedSlotPool<FPoolNonMovable, 2> Pool;

		size_t Index = 0;
		FPoolNonMovable* Value = Pool.Acquire(Index, 42);

		ASSERT_NE(nullptr, Value);
		EXPECT_EQ(42, Value->Value);
		EXPECT_EQ(Value, Pool.Get(Index));
		EXPECT_EQ(1, FPoolNonMovable::ConstructCount);
		EXPECT_EQ(0, FPoolNonMovable::DestructCount);

		Pool.Release(Index);

		EXPECT_EQ(1, FPoolNonMovable::ConstructCount);
		EXPECT_EQ(1, FPoolNonMovable::DestructCount);
	}

	EXPECT_EQ(FPoolNonMovable::ConstructCount, FPoolNonMovable::DestructCount);
}

TEST_F(FixedSlotPoolTest, WaitUntilEmptyReturnsAfterOutstandingSlotIsReleased)
{
	Core::FixedSlotPool<int, 1> Pool;

	size_t Index = 0;
	int* Value = Pool.Acquire(Index, 10);

	ASSERT_NE(nullptr, Value);
	EXPECT_FALSE(Pool.IsEmpty());

	std::atomic<bool> bReleaseStarted = false;

	std::thread ReleaseThread
	(
		[&]()
		{
			bReleaseStarted.store(true, std::memory_order_release);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			Pool.Release(Index);
		}
	);

	while (false == bReleaseStarted.load(std::memory_order_acquire))
	{
		std::this_thread::yield();
	}

	Pool.WaitUntilEmpty();

	ReleaseThread.join();

	EXPECT_TRUE(Pool.IsEmpty());
	EXPECT_EQ(1, Pool.GetFreeCount());
}

TEST_F(FixedSlotPoolTest, SupportsConcurrentAcquireAndRelease)
{
	constexpr size_t PoolCapacity = 64;
	constexpr size_t ThreadCount = 8;
	constexpr size_t IterationsPerThread = 5000;

	Core::FixedSlotPool<int, PoolCapacity> Pool;

	std::atomic<bool> bStart = false;
	std::atomic<size_t> AcquireCount = 0;
	std::atomic<size_t> ReleaseCount = 0;
	std::atomic<size_t> NullGetCount = 0;

	std::vector<std::thread> Threads;
	Threads.reserve(ThreadCount);

	for (size_t ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
	{
		Threads.emplace_back
		(
			[&Pool, &bStart, &AcquireCount, &ReleaseCount, &NullGetCount, ThreadIndex]()
			{
				while (false == bStart.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}

				for (size_t Iteration = 0; Iteration < IterationsPerThread; ++Iteration)
				{
					size_t SlotIndex = 0;
					int* Value = nullptr;

					while (nullptr == Value)
					{
						Value = Pool.Acquire
						(
							SlotIndex,
							static_cast<int>((ThreadIndex + 1) * 100000 + Iteration)
						);

						if (nullptr == Value)
						{
							std::this_thread::yield();
						}
					}

					AcquireCount.fetch_add(1, std::memory_order_relaxed);

					if (nullptr == Pool.Get(SlotIndex))
					{
						NullGetCount.fetch_add(1, std::memory_order_relaxed);
					}

					Pool.Release(SlotIndex);
					ReleaseCount.fetch_add(1, std::memory_order_relaxed);
				}
			}
		);
	}

	bStart.store(true, std::memory_order_release);

	for (std::thread& Thread : Threads)
	{
		Thread.join();
	}

	Pool.WaitUntilEmpty();

	EXPECT_TRUE(Pool.IsEmpty());
	EXPECT_EQ(PoolCapacity, Pool.GetFreeCount());
	EXPECT_EQ(ThreadCount * IterationsPerThread, AcquireCount.load(std::memory_order_acquire));
	EXPECT_EQ(ThreadCount * IterationsPerThread, ReleaseCount.load(std::memory_order_acquire));
	EXPECT_EQ(0, NullGetCount.load(std::memory_order_acquire));
}
