#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "ContainerPolicies.h"

#include "../../Diagnostics/DebugCheck.h"
#include "../../Logging/Logger.h"

namespace Core
{
	template<typename T, size_t Capacity>
	class FixedSlotPool final
	{
	private:
		using AllocatorType = InlineAllocator<T, Capacity>;

		static_assert(0 < Capacity,
			"Core::FixedSlotPool capacity must be greater than 0.");

		static_assert(Capacity < UINT32_MAX,
			"Core::FixedSlotPool capacity must fit in a 32-bit free-list index.");

		static_assert(std::is_destructible_v<T>,
			"Core::FixedSlotPool requires T to be destructible.");

	private:
		enum class ESlotState : uint8_t
		{
			Free,      // Slot is in the free-index stack and does not contain a live object.
			Acquiring, // Slot was popped from the free-index stack and object construction is in progress.
			Occupied,  // Slot contains a live object and may be returned by Get().
			Releasing, // Slot was claimed for Release() and object destruction is in progress.
		};

		class FreeIndexStack final
		{
		private:
			using IndexType = uint32_t;

			static constexpr IndexType InvalidIndex = UINT32_MAX;
			static constexpr uint64_t  IndexMask    = 0x00000000FFFFFFFFULL;

		private:
			// Tagged head: high 32 bits are an ABA-mitigation generation, low 32 bits are the top index.
			std::atomic<uint64_t>  m_Head = 0;
			std::atomic<IndexType> m_NextIndices[Capacity] = {};

		public:
			FreeIndexStack()
			{
				for (IndexType Index = 0; Index < static_cast<IndexType>(Capacity); ++Index)
				{
					const IndexType NextIndex =
						(Index + 1 < static_cast<IndexType>(Capacity))
						? Index + 1
						: InvalidIndex;

					m_NextIndices[Index].store(NextIndex, std::memory_order_relaxed);
				}

				m_Head.store(Pack(0, 0), std::memory_order_relaxed);
			}

			~FreeIndexStack() = default;

			FreeIndexStack(const FreeIndexStack& ) = delete;
			FreeIndexStack(      FreeIndexStack&&) = delete;

			FreeIndexStack& operator=(const FreeIndexStack& ) = delete;
			FreeIndexStack& operator=(      FreeIndexStack&&) = delete;

		public:
			[[nodiscard]]
			bool Pop(size_t& OutIndex)
			{
				uint64_t CurrentHead = m_Head.load(std::memory_order_acquire);

				while (true)
				{
					const IndexType CurrentIndex = UnpackIndex(CurrentHead);

					if (InvalidIndex == CurrentIndex)
					{
						return false;
					}

					const uint32_t CurrentGeneration = UnpackGeneration(CurrentHead);
					const IndexType NextIndex = m_NextIndices[CurrentIndex].load(std::memory_order_acquire);
					const uint64_t NewHead = Pack(CurrentGeneration + 1, NextIndex);

					if (true == m_Head.compare_exchange_weak
					(
						CurrentHead,
						NewHead,
						std::memory_order_acq_rel,
						std::memory_order_acquire
					))
					{
						OutIndex = static_cast<size_t>(CurrentIndex);

						return true;
					}
				}
			}

			void Push(size_t InputIndex)
			{
				RELEASE_CHECKF(InputIndex < Capacity, L"Core::FixedSlotPool free stack received an invalid index.", 1);

				const IndexType Index = static_cast<IndexType>(InputIndex);
				uint64_t CurrentHead = m_Head.load(std::memory_order_acquire);

				while (true)
				{
					const IndexType CurrentIndex = UnpackIndex(CurrentHead);
					const uint32_t CurrentGeneration = UnpackGeneration(CurrentHead);

					m_NextIndices[Index].store(CurrentIndex, std::memory_order_release);

					const uint64_t NewHead = Pack(CurrentGeneration + 1, Index);

					if (true == m_Head.compare_exchange_weak
					(
						CurrentHead,
						NewHead,
						std::memory_order_acq_rel,
						std::memory_order_acquire
					))
					{
						return;
					}
				}
			}

		private:
			static uint64_t Pack(uint32_t InputGeneration, IndexType InputIndex)
			{
				return (static_cast<uint64_t>(InputGeneration) << 32)
					| static_cast<uint64_t>(InputIndex);
			}

			static uint32_t UnpackGeneration(uint64_t InputPackedHead)
			{
				return static_cast<uint32_t>(InputPackedHead >> 32);
			}

			static IndexType UnpackIndex(uint64_t InputPackedHead)
			{
				return static_cast<IndexType>(InputPackedHead & IndexMask);
			}
		};

	private:
		AllocatorType                  m_Allocator;
		T*                             m_Values = nullptr;
		FreeIndexStack                 m_FreeIndices;

		std::atomic<size_t>            m_OccupiedCount = 0; // For WaitUntilEmpty synchronization.
		std::atomic<ESlotState>        m_SlotStates[Capacity] = {};

	public:
		FixedSlotPool()
		{
			m_Values = static_cast<T*>(m_Allocator.Allocate(Capacity));

			CHECKF(nullptr != m_Values, L"Core::FixedSlotPool failed to allocate storage.", 1);

			for (size_t Index = 0; Index < Capacity; ++Index)
			{
				m_SlotStates[Index].store(ESlotState::Free, std::memory_order_relaxed);
			}
		}

		~FixedSlotPool()
		{
			for (size_t Index = 0; Index < Capacity; ++Index)
			{
				const ESlotState SlotState = m_SlotStates[Index].load(std::memory_order_acquire);
				RELEASE_CHECKF(ESlotState::Free == SlotState, L"Core::FixedSlotPool destroyed with occupied slots.", 1);
			}

			m_Allocator.Deallocate(m_Values);
			m_Values = nullptr;
		}

		FixedSlotPool(const FixedSlotPool& ) = delete;
		FixedSlotPool(      FixedSlotPool&&) = delete;

		FixedSlotPool& operator=(const FixedSlotPool& ) = delete;
		FixedSlotPool& operator=(      FixedSlotPool&&) = delete;

	public:
		template<typename... TArgs>
		[[nodiscard]]
		T* Acquire(size_t& OutIndex, TArgs&&... InputArgs)
		{
			static_assert(std::is_constructible_v<T, TArgs&&...>,
				"Core::FixedSlotPool::Acquire requires T to be constructible from the input arguments.");

			size_t FreeIndex = 0;

			if (false == m_FreeIndices.Pop(FreeIndex))
			{
				return nullptr;
			}

			RELEASE_CHECKF(FreeIndex < Capacity, L"Core::FixedSlotPool acquired invalid slot index.", 1);

			ESlotState ExpectedState = ESlotState::Free;
			const bool bReserved = m_SlotStates[FreeIndex].compare_exchange_strong
			(
				ExpectedState,
				ESlotState::Acquiring,
				std::memory_order_acq_rel,
				std::memory_order_acquire
			);

			RELEASE_CHECKF(true == bReserved, L"Core::FixedSlotPool acquired a non-free slot.", 1);

			T* Slot = GetSlot(FreeIndex);

			try
			{
				std::construct_at(Slot, std::forward<TArgs>(InputArgs)...);
			}
			catch (...)
			{
				m_SlotStates[FreeIndex].store(ESlotState::Free, std::memory_order_release);
				m_FreeIndices.Push(FreeIndex);

				throw;
			}

			m_SlotStates[FreeIndex].store(ESlotState::Occupied, std::memory_order_release);
			m_OccupiedCount.fetch_add(1, std::memory_order_release);

			OutIndex = FreeIndex;

			return Slot;
		}

		void Release(size_t InputIndex)
		{
			RELEASE_CHECKF(InputIndex < Capacity, L"Core::FixedSlotPool::Release received an invalid index.", 1);

			ESlotState ExpectedState = ESlotState::Occupied;
			const bool bReleaseClaimed = m_SlotStates[InputIndex].compare_exchange_strong
			(
				ExpectedState,
				ESlotState::Releasing,
				std::memory_order_acq_rel,
				std::memory_order_acquire
			);

			RELEASE_CHECKF(true == bReleaseClaimed, L"Core::FixedSlotPool detected double release or release of an empty slot.", 1);

			DestroySlot(GetSlot(InputIndex));

			m_SlotStates[InputIndex].store(ESlotState::Free, std::memory_order_release);
			m_FreeIndices.Push(InputIndex);

			const size_t PreviousOccupiedCount =
				m_OccupiedCount.fetch_sub(1, std::memory_order_acq_rel);

			RELEASE_CHECKF(0 < PreviousOccupiedCount, L"Core::FixedSlotPool occupied count underflow.", 1);

			if (1 == PreviousOccupiedCount)
			{
				m_OccupiedCount.notify_all();
			}
		}

		[[nodiscard]]
		T* Get(size_t InputIndex)
		{
			RELEASE_CHECKF(InputIndex < Capacity, L"Core::FixedSlotPool::Get received an invalid index.", 1);
			RELEASE_CHECKF(ESlotState::Occupied == m_SlotStates[InputIndex].load(std::memory_order_acquire),
				L"Core::FixedSlotPool::Get requested a non-occupied slot.", 1);

			return GetSlot(InputIndex);
		}

		[[nodiscard]]
		const T* Get(size_t InputIndex) const
		{
			RELEASE_CHECKF(InputIndex < Capacity, L"Core::FixedSlotPool::Get received an invalid index.", 1);
			RELEASE_CHECKF(ESlotState::Occupied == m_SlotStates[InputIndex].load(std::memory_order_acquire),
				L"Core::FixedSlotPool::Get requested a non-occupied slot.", 1);

			return GetSlot(InputIndex);
		}

		[[nodiscard]]
		size_t GetFreeCount() const
		{
			return Capacity - m_OccupiedCount.load(std::memory_order_acquire);
		}

		[[nodiscard]]
		bool IsEmpty() const
		{
			return 0 == m_OccupiedCount.load(std::memory_order_acquire);
		}

		void WaitUntilEmpty() const
		{
			size_t OccupiedCount = m_OccupiedCount.load(std::memory_order_acquire);

			while (0 != OccupiedCount)
			{
				m_OccupiedCount.wait(OccupiedCount, std::memory_order_acquire);
				OccupiedCount = m_OccupiedCount.load(std::memory_order_acquire);
			}
		}

		[[nodiscard]]
		constexpr size_t GetCapacity() const
		{
			return Capacity;
		}

	private:
		T* GetSlot(size_t InputIndex)
		{
			return m_Values + InputIndex;
		}

		const T* GetSlot(size_t InputIndex) const
		{
			return m_Values + InputIndex;
		}

		static void DestroySlot(T* InputSlot)
		{
			if constexpr (false == std::is_trivially_destructible_v<T>)
			{
				std::destroy_at(InputSlot);
			}
		}
	};
}
