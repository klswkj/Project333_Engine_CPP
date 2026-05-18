#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "Vector.h"
#include "ContainerPolicies.h"

#include "../../Diagnostics/DebugCheck.h"
#include "../../Logging/Logger.h"

namespace Core
{
	// This file defines the generic Queue template.
	// Convenience aliases for common usages, such as Core::DynamicQueue
	// and Core::FixedQueue, are declared at the bottom of this file.
	//
	// Use Core::FixedQueue<T, Capacity> for fixed-size ring queues.
	// For example:
	// Core::FixedQueue<D3D12Payload*, 1024> PendingSubmission;
	// Core::FixedQueue<D3D12Payload*, 1024> PendingInterrupt;
	//
	// Convenience aliases such as Core::DynamicQueue and Core::FixedQueue
	// are declared at the bottom of this file.
	//
	// template<typename T, typename AllocatorType = DefaultContainerAllocator<T>>
	// using DynamicQueue = Queue<T, 0, AllocatorTtype, false>;
	//
	// template<typename T, size_t Capacity>
	// using FixedQueue = Queue<T, Capacity, InlineAllocator<T, Capacity>, true>;


	template
	<
		typename T,
		size_t Capacity,
		typename AllocatorType = DefaultContainerAllocator<T>,
		bool bFixed = false
	>
	class Queue;


	// Dynamic-Capacity Queue
	template<typename T, size_t Capacity, typename AllocatorType>
	class Queue<T, Capacity, AllocatorType, false>
	{
	private:
		Vector<T, AllocatorType> m_Values;
		size_t                   m_HeadIndex = 0;

	public:
		Queue()  = default;
		~Queue() = default;

		Queue(const Queue& ) = delete;
		Queue(      Queue&&) = delete;

		Queue& operator=(const Queue& ) = delete;
		Queue& operator=(      Queue&&) = delete;

	public:
		[[nodiscard]]
		bool Enqueue(const T& InputValue)
		{
			return m_Values.Push_back(InputValue);
		}

		[[nodiscard]]
		bool Enqueue(T&& InputValue)
		{
			return m_Values.Push_back(std::move(InputValue));
		}

		template<typename... TArgs>
		[[nodiscard]]
		bool Emplace(TArgs&&... InputArgs)
		{
			return m_Values.Emplace_back(std::forward<TArgs>(InputArgs)...);
		}

		[[nodiscard]]
		bool Dequeue(T& OutValue)
		{
			static_assert(std::is_move_assignable_v<T>,
				"Core::Queue::Dequeue requires T to be move assignable.");

			if (true == IsEmpty())
			{
				return false;
			}

			OutValue = std::move(m_Values[m_HeadIndex]);

			++m_HeadIndex;
			ClearIfConsumed();

			return true;
		}

		[[nodiscard]]
		T* Peek()
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return &m_Values[m_HeadIndex];
		}

		[[nodiscard]]
		const T* Peek() const
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return &m_Values[m_HeadIndex];
		}

		void Pop()
		{
			if (true == IsEmpty())
			{
				return;
			}

			++m_HeadIndex;
			ClearIfConsumed();
		}

		void Clear()
		{
			m_Values.Clear();
			m_HeadIndex = 0;
		}

		[[nodiscard]]
		bool IsEmpty() const
		{
			return m_Values.GetSize() <= m_HeadIndex;
		}

		[[nodiscard]]
		size_t GetSize() const
		{
			if (m_Values.GetSize() <= m_HeadIndex)
			{
				return 0;
			}

			return m_Values.GetSize() - m_HeadIndex;
		}

	private:
		void ClearIfConsumed()
		{
			if (m_Values.GetSize() == m_HeadIndex)
			{
				Clear();
			}
		}
	};

	// Fixed-Capacity Ring Queue
	template<typename T, size_t Capacity>
	class Queue<T, Capacity, InlineAllocator<T, Capacity>, true>
	{
	private:
		using AllocatorType = InlineAllocator<T, Capacity>;

		static_assert(0 < Capacity,
			"Core::Queue fixed capacity must be greater than 0.");

		static_assert(std::is_destructible_v<T>,
			"Core::Queue fixed mode requires T to be destructible.");

	private:
		Vector<T, AllocatorType> m_Values;

		size_t m_HeadIndex = 0;
		size_t m_TailIndex = 0;
		size_t m_Count     = 0;

	public:
		Queue()
		{
			const bool bReserved = m_Values.Reserve(Capacity);
			CHECKF(true == bReserved, L"Core::Queue fixed mode failed to reserve storage.", 1);
		}

		~Queue()
		{
			Clear();
		}

		Queue(const Queue& ) = delete;
		Queue(      Queue&&) = delete;

		Queue& operator=(const Queue& ) = delete;
		Queue& operator=(      Queue&&) = delete;

	public:
		[[nodiscard]]
		bool Enqueue(const T& InputValue)
		{
			static_assert(std::is_copy_constructible_v<T>,
				"Core::Queue::Enqueue(const T&) requires T to be copy constructible.");

			if (true == IsFull())
			{
				return false;
			}

			std::construct_at(GetSlot(m_TailIndex), InputValue);

			m_TailIndex = GetNextIndex(m_TailIndex);
			++m_Count;

			return true;
		}

		[[nodiscard]]
		bool Enqueue(T&& InputValue)
		{
			static_assert(std::is_move_constructible_v<T>,
				"Core::Queue::Enqueue(T&&) requires T to be move constructible.");

			if (true == IsFull())
			{
				return false;
			}

			std::construct_at(GetSlot(m_TailIndex), std::move(InputValue));

			m_TailIndex = GetNextIndex(m_TailIndex);
			++m_Count;

			return true;
		}

		template<typename... TArgs>
		[[nodiscard]]
		bool Emplace(TArgs&&... InputArgs)
		{
			static_assert(std::is_constructible_v<T, TArgs&&...>,
				"Core::Queue::Emplace requires T to be constructible from the input arguments.");

			if (true == IsFull())
			{
				return false;
			}

			std::construct_at(GetSlot(m_TailIndex), std::forward<TArgs>(InputArgs)...);

			m_TailIndex = GetNextIndex(m_TailIndex);
			++m_Count;

			return true;
		}

		[[nodiscard]]
		bool Dequeue(T& OutValue)
		{
			static_assert(std::is_move_assignable_v<T>,
				"Core::Queue::Dequeue requires T to be move assignable.");

			if (true == IsEmpty())
			{
				return false;
			}

			T* FrontSlot = GetSlot(m_HeadIndex);
			OutValue = std::move(*FrontSlot);
			DestroySlot(FrontSlot);

			m_HeadIndex = GetNextIndex(m_HeadIndex);
			--m_Count;

			ResetIndicesIfEmpty();

			return true;
		}

		[[nodiscard]]
		T* Peek()
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return GetSlot(m_HeadIndex);
		}

		[[nodiscard]]
		const T* Peek() const
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return GetSlot(m_HeadIndex);
		}

		void Pop()
		{
			if (true == IsEmpty())
			{
				return;
			}

			DestroySlot(GetSlot(m_HeadIndex));

			m_HeadIndex = GetNextIndex(m_HeadIndex);
			--m_Count;

			ResetIndicesIfEmpty();
		}

		void Clear()
		{
			while (false == IsEmpty())
			{
				Pop();
			}

			m_HeadIndex = 0;
			m_TailIndex = 0;
			m_Count     = 0;
		}

		[[nodiscard]]
		bool IsEmpty() const
		{
			return 0 == m_Count;
		}

		[[nodiscard]]
		bool IsFull() const
		{
			return Capacity == m_Count;
		}

		[[nodiscard]]
		size_t GetSize() const
		{
			return m_Count;
		}

		[[nodiscard]]
		constexpr size_t GetCapacity() const
		{
			return Capacity;
		}

	private:
		size_t GetNextIndex(size_t InputIndex) const
		{
			++InputIndex;

			if (Capacity == InputIndex)
			{
				return 0;
			}

			return InputIndex;
		}

		void ResetIndicesIfEmpty()
		{
			if (0 == m_Count)
			{
				m_HeadIndex = 0;
				m_TailIndex = 0;
			}
		}

		T* GetSlot(size_t InputIndex)
		{
			return m_Values.GetData() + InputIndex;
		}

		const T* GetSlot(size_t InputIndex) const
		{
			return m_Values.GetData() + InputIndex;
		}

		static void DestroySlot(T* InputSlot)
		{
			if constexpr (false == std::is_trivially_destructible_v<T>)
			{
				std::destroy_at(InputSlot);
			}
		}
	};

	template<typename>
	inline constexpr bool AlwaysFalse = false;

	template<typename T, size_t Capacity, typename AllocatorType>
	class Queue<T, Capacity, AllocatorType, true>
	{
		static_assert
		(
			AlwaysFalse<AllocatorType>,
			"Core::Queue fixed mode requires Core::InlineAllocator<T, Capacity>."
		);
	};

	template<typename T, typename AllocatorType = DefaultContainerAllocator<T>>
	using DynamicQueue = Queue<T, 0, AllocatorType, false>;

	template<typename T, size_t Capacity>
	using FixedQueue = Queue<T, Capacity, InlineAllocator<T, Capacity>, true>;
}
