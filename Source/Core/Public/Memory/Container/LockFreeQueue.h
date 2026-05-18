#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "../../Diagnostics/DebugCheck.h"
#include "../../Logging/Logger.h"
#include "ContainerPolicies.h"

// Lock-free bounded queues for thread-to-thread communication.
// This file contains:
// - Core::SPSCQueue<T, Capacity> : Single Producer / Single Consumer
// - Core::MPSCQueue<T, Capacity> : Multiple Producers / Single Consumer
// - Core::SPMCQueue<T, Capacity> : Single Producer / Multiple Consumers
//
// These queues are different from Core::Queue / Core::FixedQueue.
// Core::Queue is a single-thread container.
// The queues in this file are synchronization primitives.

namespace Core
{
	namespace LockFreeQueuePrivate
	{
#if defined(__cpp_lib_hardware_interference_size)
		// Keep producer/consumer hot indices on separate cache lines.
		inline constexpr size_t CacheLineSize = std::hardware_destructive_interference_size;
#else
		inline constexpr size_t CacheLineSize = 64;
#endif

		inline constexpr size_t HalfSequenceRange =
			static_cast<size_t>(1) << ((sizeof(size_t) * 8) - 1);

		inline bool IsSequenceBefore(size_t InputSequence, size_t InputTargetSequence)
		{
			const size_t Distance = InputTargetSequence - InputSequence;

			return 0 < Distance && Distance < HalfSequenceRange;
		}

		template<typename T, typename... TArgs>
		inline void ConstructValue(T* InputValue, TArgs&&... InputArgs)
		{
			std::construct_at(InputValue, std::forward<TArgs>(InputArgs)...);
		}

		template<typename T>
		inline void DestroyValue(T* InputValue)
		{
			if constexpr (false == std::is_trivially_destructible_v<T>)
			{
				std::destroy_at(InputValue);
			}
		}

		template<typename T>
		struct Slot final
		{
		private:
			using StorageType = std::aligned_storage_t<sizeof(T), alignof(T)>;

		public:
			std::atomic<size_t> Sequence = 0;
			StorageType         Storage;

		public:
			Slot() = default;

			explicit Slot(size_t InputSequence)
				: Sequence(InputSequence)
			{
			}

			Slot(const Slot& ) = delete;
			Slot(      Slot&&) = delete;

			Slot& operator=(const Slot& ) = delete;
			Slot& operator=(      Slot&&) = delete;

		public:
			T* GetValueStorage()
			{
				return reinterpret_cast<T*>(&Storage);
			}

			const T* GetValueStorage() const
			{
				return reinterpret_cast<const T*>(&Storage);
			}

			T* GetValue()
			{
				return std::launder(reinterpret_cast<T*>(&Storage));
			}

			const T* GetValue() const
			{
				return std::launder(reinterpret_cast<const T*>(&Storage));
			}
		};
	} // namespace LockFreeQueuePrivate

	// ============================================================
	// SPSCQueue
	// Single Producer / Single Consumer
	// ============================================================

	template<typename T, size_t Capacity>
	class SPSCQueue final
	{
	private:
		static_assert(0 < Capacity,
			"Core::SPSCQueue capacity must be greater than 0.");

		static_assert(Capacity < static_cast<size_t>(-1),
			"Core::SPSCQueue capacity is too large.");

		static_assert(std::is_destructible_v<T>,
			"Core::SPSCQueue requires T to be destructible.");

	private:
		static constexpr size_t BufferCapacity = Capacity + 1;

		using AllocatorType = InlineAllocator<T, BufferCapacity>;

	private:
		AllocatorType m_Allocator;
		T*            m_Values = nullptr;

		alignas(LockFreeQueuePrivate::CacheLineSize) std::atomic<size_t> m_HeadIndex = 0;
		alignas(LockFreeQueuePrivate::CacheLineSize) std::atomic<size_t> m_TailIndex = 0;

	public:
		SPSCQueue()
		{
			m_Values = static_cast<T*>(m_Allocator.Allocate(BufferCapacity));
			CHECKF(nullptr != m_Values, L"Core::SPSCQueue failed to allocate storage.", 1);
		}

		~SPSCQueue()
		{
			ClearUnsafe();

			m_Allocator.Deallocate(m_Values);
			m_Values = nullptr;
		}

		SPSCQueue(const SPSCQueue& ) = delete;
		SPSCQueue(      SPSCQueue&&) = delete;

		SPSCQueue& operator=(const SPSCQueue& ) = delete;
		SPSCQueue& operator=(      SPSCQueue&&) = delete;

	public:
		[[nodiscard]]
		bool Enqueue(const T& InputValue)
		{
			static_assert(std::is_copy_constructible_v<T>,
				"Core::SPSCQueue::Enqueue(const T&) requires T to be copy constructible.");

			const size_t TailIndex     = m_TailIndex.load(std::memory_order_relaxed);
			const size_t NextTailIndex = GetNextIndex(TailIndex);

			if (NextTailIndex == m_HeadIndex.load(std::memory_order_acquire))
			{
				return false;
			}

			LockFreeQueuePrivate::ConstructValue(GetSlot(TailIndex), InputValue);

			m_TailIndex.store(NextTailIndex, std::memory_order_release);

			return true;
		}

		[[nodiscard]]
		bool Enqueue(T&& InputValue)
		{
			static_assert(std::is_move_constructible_v<T>,
				"Core::SPSCQueue::Enqueue(T&&) requires T to be move constructible.");

			const size_t TailIndex     = m_TailIndex.load(std::memory_order_relaxed);
			const size_t NextTailIndex = GetNextIndex(TailIndex);

			if (NextTailIndex == m_HeadIndex.load(std::memory_order_acquire))
			{
				return false;
			}

			LockFreeQueuePrivate::ConstructValue(GetSlot(TailIndex), std::move(InputValue));

			m_TailIndex.store(NextTailIndex, std::memory_order_release);

			return true;
		}

		template<typename... TArgs>
		[[nodiscard]]
		bool Emplace(TArgs&&... InputArgs)
		{
			static_assert(std::is_constructible_v<T, TArgs&&...>,
				"Core::SPSCQueue::Emplace requires T to be constructible from the input arguments.");

			const size_t TailIndex     = m_TailIndex.load(std::memory_order_relaxed);
			const size_t NextTailIndex = GetNextIndex(TailIndex);

			if (NextTailIndex == m_HeadIndex.load(std::memory_order_acquire))
			{
				return false;
			}

			LockFreeQueuePrivate::ConstructValue(GetSlot(TailIndex), std::forward<TArgs>(InputArgs)...);

			m_TailIndex.store(NextTailIndex, std::memory_order_release);

			return true;
		}

		[[nodiscard]]
		bool Dequeue(T& OutValue)
		{
			static_assert(std::is_move_assignable_v<T>,
				"Core::SPSCQueue::Dequeue requires T to be move assignable.");

			const size_t HeadIndex = m_HeadIndex.load(std::memory_order_relaxed);

			if (HeadIndex == m_TailIndex.load(std::memory_order_acquire))
			{
				return false;
			}

			T* HeadSlot = GetSlot(HeadIndex);

			OutValue = std::move(*HeadSlot);

			LockFreeQueuePrivate::DestroyValue(HeadSlot);

			m_HeadIndex.store(GetNextIndex(HeadIndex), std::memory_order_release);

			return true;
		}

		[[nodiscard]]
		bool IsEmpty() const
		{
			const size_t HeadIndex = m_HeadIndex.load(std::memory_order_acquire);
			const size_t TailIndex = m_TailIndex.load(std::memory_order_acquire);

			return HeadIndex == TailIndex;
		}

		[[nodiscard]]
		bool IsFull() const
		{
			const size_t TailIndex     = m_TailIndex.load(std::memory_order_acquire);
			const size_t NextTailIndex = GetNextIndex(TailIndex);
			const size_t HeadIndex     = m_HeadIndex.load(std::memory_order_acquire);

			return NextTailIndex == HeadIndex;
		}

		[[nodiscard]]
		constexpr size_t GetCapacity() const
		{
			return Capacity;
		}

	private:
		// Not thread-safe. Call only after producer and consumer have stopped.
		void ClearUnsafe()
		{
			size_t HeadIndex = m_HeadIndex.load(std::memory_order_relaxed);
			const size_t TailIndex = m_TailIndex.load(std::memory_order_acquire);

			while (HeadIndex != TailIndex)
			{
				LockFreeQueuePrivate::DestroyValue(GetSlot(HeadIndex));
				HeadIndex = GetNextIndex(HeadIndex);
			}

			m_HeadIndex.store(HeadIndex, std::memory_order_relaxed);
		}

		size_t GetNextIndex(size_t InputIndex) const
		{
			++InputIndex;

			if (BufferCapacity == InputIndex)
			{
				return 0;
			}

			return InputIndex;
		}

		T* GetSlot(size_t InputIndex)
		{
			return m_Values + InputIndex;
		}

		const T* GetSlot(size_t InputIndex) const
		{
			return m_Values + InputIndex;
		}
	};

	// ============================================================
	// MPSCQueue
	// Multiple Producers / Single Consumer
	// ============================================================

	template<typename T, size_t Capacity>
	class MPSCQueue final
	{
	private:
		static_assert(0 < Capacity,
			"Core::MPSCQueue capacity must be greater than 0.");

		static_assert(Capacity < LockFreeQueuePrivate::HalfSequenceRange,
			"Core::MPSCQueue capacity must be less than half of size_t range.");

		static_assert(std::is_destructible_v<T>,
			"Core::MPSCQueue requires T to be destructible.");

	private:
		using SlotType      = LockFreeQueuePrivate::Slot<T>;
		using AllocatorType = InlineAllocator<SlotType, Capacity>;

	private:
		AllocatorType m_Allocator;
		SlotType*     m_Slots = nullptr;

		alignas(LockFreeQueuePrivate::CacheLineSize) size_t              m_HeadIndex = 0;
		alignas(LockFreeQueuePrivate::CacheLineSize) std::atomic<size_t> m_TailIndex = 0;

	public:
		MPSCQueue()
		{
			m_Slots = static_cast<SlotType*>(m_Allocator.Allocate(Capacity));
			CHECKF(nullptr != m_Slots, L"Core::MPSCQueue failed to allocate slots.", 1);

			for (size_t Index = 0; Index < Capacity; ++Index)
			{
				std::construct_at(m_Slots + Index, Index);
			}
		}

		~MPSCQueue()
		{
			ClearUnsafe();

			for (size_t Index = 0; Index < Capacity; ++Index)
			{
				std::destroy_at(m_Slots + Index);
			}

			m_Allocator.Deallocate(m_Slots);
			m_Slots = nullptr;
		}

		MPSCQueue(const MPSCQueue& ) = delete;
		MPSCQueue(      MPSCQueue&&) = delete;

		MPSCQueue& operator=(const MPSCQueue& ) = delete;
		MPSCQueue& operator=(      MPSCQueue&&) = delete;

	public:
		[[nodiscard]]
		bool Enqueue(const T& InputValue)
		{
			static_assert(std::is_copy_constructible_v<T>,
				"Core::MPSCQueue::Enqueue(const T&) requires T to be copy constructible.");

			size_t TailIndex = m_TailIndex.load(std::memory_order_relaxed);

			while (true)
			{
				SlotType* CurrentSlot = GetSlot(TailIndex % Capacity);

				const size_t Sequence = CurrentSlot->Sequence.load(std::memory_order_acquire);

				// Slot state relative to the local TailIndex:
				//
				// Sequence == TailIndex
				//     The slot is free. Try to reserve it.
				//
				// IsSequenceBefore(Sequence, TailIndex)
				//     The slot has not been released yet.
				//     The queue is full, or this slot cannot be reused yet.
				//
				// Otherwise
				//     The local TailIndex is stale.
				//     Another producer has advanced the tail.
				//     Reload TailIndex and retry.
				if (Sequence == TailIndex)
				{
					if (true == m_TailIndex.compare_exchange_weak
					(
						TailIndex,
						TailIndex + 1,
						std::memory_order_acq_rel,
						std::memory_order_relaxed
					))
					{
						LockFreeQueuePrivate::ConstructValue(CurrentSlot->GetValueStorage(), InputValue);
						CurrentSlot->Sequence.store(TailIndex + 1, std::memory_order_release);

						return true;
					}

					continue;
				}

				if (true == LockFreeQueuePrivate::IsSequenceBefore(Sequence, TailIndex))
				{
					return false;
				}

				TailIndex = m_TailIndex.load(std::memory_order_relaxed);
			}
		}

		[[nodiscard]]
		bool Enqueue(T&& InputValue)
		{
			static_assert(std::is_move_constructible_v<T>,
				"Core::MPSCQueue::Enqueue(T&&) requires T to be move constructible.");

			size_t TailIndex = m_TailIndex.load(std::memory_order_relaxed);

			while (true)
			{
				SlotType* CurrentSlot = GetSlot(TailIndex % Capacity);

				const size_t Sequence = CurrentSlot->Sequence.load(std::memory_order_acquire);

				if (Sequence == TailIndex)
				{
					if (true == m_TailIndex.compare_exchange_weak
					(
						TailIndex,
						TailIndex + 1,
						std::memory_order_acq_rel,
						std::memory_order_relaxed
					))
					{
						LockFreeQueuePrivate::ConstructValue(CurrentSlot->GetValueStorage(), std::move(InputValue));
						CurrentSlot->Sequence.store(TailIndex + 1, std::memory_order_release);

						return true;
					}

					continue;
				}

				if (true == LockFreeQueuePrivate::IsSequenceBefore(Sequence, TailIndex))
				{
					return false;
				}

				TailIndex = m_TailIndex.load(std::memory_order_relaxed);
			}
		}

		template<typename... TArgs>
		[[nodiscard]]
		bool Emplace(TArgs&&... InputArgs)
		{
			static_assert(std::is_constructible_v<T, TArgs&&...>,
				"Core::MPSCQueue::Emplace requires T to be constructible from the input arguments.");

			size_t TailIndex = m_TailIndex.load(std::memory_order_relaxed);

			while (true)
			{
				SlotType* CurrentSlot = GetSlot(TailIndex % Capacity);

				const size_t Sequence = CurrentSlot->Sequence.load(std::memory_order_acquire);

				if (Sequence == TailIndex)
				{
					if (true == m_TailIndex.compare_exchange_weak
					(
						TailIndex,
						TailIndex + 1,
						std::memory_order_acq_rel,
						std::memory_order_relaxed
					))
					{
						LockFreeQueuePrivate::ConstructValue(CurrentSlot->GetValueStorage(), std::forward<TArgs>(InputArgs)...);
						CurrentSlot->Sequence.store(TailIndex + 1, std::memory_order_release);

						return true;
					}

					continue;
				}

				if (true == LockFreeQueuePrivate::IsSequenceBefore(Sequence, TailIndex))
				{
					return false;
				}

				TailIndex = m_TailIndex.load(std::memory_order_relaxed);
			}
		}

		[[nodiscard]]
		bool Dequeue(T& OutValue)
		{
			static_assert(std::is_move_assignable_v<T>,
				"Core::MPSCQueue::Dequeue requires T to be move assignable.");

			SlotType* CurrentSlot = GetSlot(m_HeadIndex % Capacity);

			const size_t TargetSequence = m_HeadIndex + 1;
			const size_t Sequence       = CurrentSlot->Sequence.load(std::memory_order_acquire);

			if (Sequence != TargetSequence)
			{
				return false;
			}

			T* Value = CurrentSlot->GetValue();

			OutValue = std::move(*Value);

			LockFreeQueuePrivate::DestroyValue(Value);

			CurrentSlot->Sequence.store(m_HeadIndex + Capacity, std::memory_order_release);

			++m_HeadIndex;

			return true;
		}

		[[nodiscard]]
		constexpr size_t GetCapacity() const
		{
			return Capacity;
		}

	private:
		// Not thread-safe. Call only after all producers and the consumer have stopped.
		void ClearUnsafe()
		{
			while (true)
			{
				SlotType* CurrentSlot = GetSlot(m_HeadIndex % Capacity);

				const size_t TargetSequence = m_HeadIndex + 1;
				const size_t Sequence       = CurrentSlot->Sequence.load(std::memory_order_acquire);

				if (Sequence != TargetSequence)
				{
					break;
				}

				LockFreeQueuePrivate::DestroyValue(CurrentSlot->GetValue());

				CurrentSlot->Sequence.store(m_HeadIndex + Capacity, std::memory_order_release);

				++m_HeadIndex;
			}
		}

		SlotType* GetSlot(size_t InputIndex)
		{
			return m_Slots + InputIndex;
		}

		const SlotType* GetSlot(size_t InputIndex) const
		{
			return m_Slots + InputIndex;
		}
	};

	// ============================================================
	// SPMCQueue
	// Single Producer / Multiple Consumers
	// ============================================================

	template<typename T, size_t Capacity>
	class SPMCQueue final
	{
	private:
		static_assert(0 < Capacity,
			"Core::SPMCQueue capacity must be greater than 0.");

		static_assert(Capacity < LockFreeQueuePrivate::HalfSequenceRange,
			"Core::SPMCQueue capacity must be less than half of size_t range.");

		static_assert(std::is_destructible_v<T>,
			"Core::SPMCQueue requires T to be destructible.");

	private:
		using SlotType      = LockFreeQueuePrivate::Slot<T>;
		using AllocatorType = InlineAllocator<SlotType, Capacity>;

	private:
		AllocatorType m_Allocator;
		SlotType*     m_Slots = nullptr;

		alignas(LockFreeQueuePrivate::CacheLineSize) std::atomic<size_t> m_HeadIndex = 0;
		alignas(LockFreeQueuePrivate::CacheLineSize) size_t              m_TailIndex = 0;

	public:
		SPMCQueue()
		{
			m_Slots = static_cast<SlotType*>(m_Allocator.Allocate(Capacity));
			CHECKF(nullptr != m_Slots, L"Core::SPMCQueue failed to allocate slots.", 1);

			for (size_t Index = 0; Index < Capacity; ++Index)
			{
				std::construct_at(m_Slots + Index, Index);
			}
		}

		~SPMCQueue()
		{
			ClearUnsafe();

			for (size_t Index = 0; Index < Capacity; ++Index)
			{
				std::destroy_at(m_Slots + Index);
			}

			m_Allocator.Deallocate(m_Slots);
			m_Slots = nullptr;
		}

		SPMCQueue(const SPMCQueue& ) = delete;
		SPMCQueue(      SPMCQueue&&) = delete;

		SPMCQueue& operator=(const SPMCQueue& ) = delete;
		SPMCQueue& operator=(      SPMCQueue&&) = delete;

	public:
		[[nodiscard]]
		bool Enqueue(const T& InputValue)
		{
			static_assert(std::is_copy_constructible_v<T>,
				"Core::SPMCQueue::Enqueue(const T&) requires T to be copy constructible.");

			SlotType* CurrentSlot = GetSlot(m_TailIndex % Capacity);

			const size_t Sequence = CurrentSlot->Sequence.load(std::memory_order_acquire);

			if (Sequence != m_TailIndex)
			{
				return false;
			}

			LockFreeQueuePrivate::ConstructValue(CurrentSlot->GetValueStorage(), InputValue);
			CurrentSlot->Sequence.store(m_TailIndex + 1, std::memory_order_release);

			++m_TailIndex;

			return true;
		}

		[[nodiscard]]
		bool Enqueue(T&& InputValue)
		{
			static_assert(std::is_move_constructible_v<T>,
				"Core::SPMCQueue::Enqueue(T&&) requires T to be move constructible.");

			SlotType* CurrentSlot = GetSlot(m_TailIndex % Capacity);

			const size_t Sequence = CurrentSlot->Sequence.load(std::memory_order_acquire);

			if (Sequence != m_TailIndex)
			{
				return false;
			}

			LockFreeQueuePrivate::ConstructValue(CurrentSlot->GetValueStorage(), std::move(InputValue));
			CurrentSlot->Sequence.store(m_TailIndex + 1, std::memory_order_release);

			++m_TailIndex;

			return true;
		}

		template<typename... TArgs>
		[[nodiscard]]
		bool Emplace(TArgs&&... InputArgs)
		{
			static_assert(std::is_constructible_v<T, TArgs&&...>,
				"Core::SPMCQueue::Emplace requires T to be constructible from the input arguments.");

			SlotType* CurrentSlot = GetSlot(m_TailIndex % Capacity);

			const size_t Sequence = CurrentSlot->Sequence.load(std::memory_order_acquire);

			if (Sequence != m_TailIndex)
			{
				return false;
			}

			LockFreeQueuePrivate::ConstructValue(CurrentSlot->GetValueStorage(), std::forward<TArgs>(InputArgs)...);
			CurrentSlot->Sequence.store(m_TailIndex + 1, std::memory_order_release);

			++m_TailIndex;

			return true;
		}

		[[nodiscard]]
		bool Dequeue(T& OutValue)
		{
			static_assert(std::is_move_assignable_v<T>,
				"Core::SPMCQueue::Dequeue requires T to be move assignable.");

			size_t HeadIndex = m_HeadIndex.load(std::memory_order_relaxed);

			while (true)
			{
				SlotType* CurrentSlot = GetSlot(HeadIndex % Capacity);

				const size_t TargetSequence = HeadIndex + 1;
				const size_t Sequence       = CurrentSlot->Sequence.load(std::memory_order_acquire);

				// Slot state relative to the local HeadIndex:
				//
				// Sequence == TargetSequence
				//     The slot is ready to consume.
				//
				// IsSequenceBefore(Sequence, TargetSequence)
				//     The slot is not ready yet.
				//     The queue is empty for this local HeadIndex.
				//
				// Otherwise
				//     The local HeadIndex is stale.
				//     Another consumer has advanced the head.
				//     Reload HeadIndex and retry.
				if (Sequence == TargetSequence)
				{
					if (true == m_HeadIndex.compare_exchange_weak
					(
						HeadIndex,
						HeadIndex + 1,
						std::memory_order_acq_rel,
						std::memory_order_relaxed
					))
					{
						T* Value = CurrentSlot->GetValue();

						OutValue = std::move(*Value);

						LockFreeQueuePrivate::DestroyValue(Value);

						CurrentSlot->Sequence.store(HeadIndex + Capacity, std::memory_order_release);

						return true;
					}

					continue;
				}

				if (true == LockFreeQueuePrivate::IsSequenceBefore(Sequence, TargetSequence))
				{
					return false;
				}

				HeadIndex = m_HeadIndex.load(std::memory_order_relaxed);
			}
		}

		[[nodiscard]]
		constexpr size_t GetCapacity() const
		{
			return Capacity;
		}

	private:
		// Not thread-safe. Call only after the producer and all consumers have stopped.
		void ClearUnsafe()
		{
			size_t HeadIndex = m_HeadIndex.load(std::memory_order_relaxed);

			while (true)
			{
				SlotType* CurrentSlot = GetSlot(HeadIndex % Capacity);

				const size_t TargetSequence = HeadIndex + 1;
				const size_t Sequence       = CurrentSlot->Sequence.load(std::memory_order_acquire);

				if (Sequence != TargetSequence)
				{
					break;
				}

				LockFreeQueuePrivate::DestroyValue(CurrentSlot->GetValue());

				CurrentSlot->Sequence.store(HeadIndex + Capacity, std::memory_order_release);

				++HeadIndex;
			}

			m_HeadIndex.store(HeadIndex, std::memory_order_relaxed);
		}

		SlotType* GetSlot(size_t InputIndex)
		{
			return m_Slots + InputIndex;
		}

		const SlotType* GetSlot(size_t InputIndex) const
		{
			return m_Slots + InputIndex;
		}
	};
}
