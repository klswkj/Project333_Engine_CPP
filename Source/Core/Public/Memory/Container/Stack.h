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
	// Convenience aliases such as 
	// Core::DynamicStack and Core::FixedStack 
	// are declared at the bottom of this file.
	//
	// template<typename T, typename AllocatorType = DefaultContainerAllocator<T>>
	// using DynamicStack = Stack<T, 0, AllocatorType, false>;
	//
	// template<typename T, size_t Capacity>
	// using FixedStack = Stack<T, Capacity, InlineAllocator<T, Capacity>, true>;

	template
	<
		typename T,
		size_t Capacity,
		typename AllocatorType = DefaultContainerAllocator<T>,
		bool bFixed = false
	>
	class Stack;

	// Dynamic-Capacity Stack
	template<typename T, size_t Capacity, typename AllocatorType>
	class Stack<T, Capacity, AllocatorType, false>
	{
	private:
		Vector<T, AllocatorType> m_Values;

	public:
		Stack()  = default;
		~Stack() = default;

		Stack(const Stack& ) = delete;
		Stack(      Stack&&) = delete;

		Stack& operator=(const Stack& ) = delete;
		Stack& operator=(      Stack&&) = delete;

	public:
		[[nodiscard]]
		bool Push(const T& InputValue)
		{
			return m_Values.Push_back(InputValue);
		}

		[[nodiscard]]
		bool Push(T&& InputValue)
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
		bool Pop(T& OutValue)
		{
			static_assert(std::is_move_assignable_v<T>,
				"Core::Stack::Pop requires T to be move assignable.");

			if (true == IsEmpty())
			{
				return false;
			}

			const size_t TopIndex = m_Values.GetSize() - 1;

			OutValue = std::move(m_Values[TopIndex]);
			m_Values.Pop_back();

			return true;
		}

		void Pop()
		{
			if (true == IsEmpty())
			{
				return;
			}

			m_Values.Pop_back();
		}

		[[nodiscard]]
		T* Peek()
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return &m_Values[m_Values.GetSize() - 1];
		}

		[[nodiscard]]
		const T* Peek() const
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return &m_Values[m_Values.GetSize() - 1];
		}

		void Clear()
		{
			m_Values.Clear();
		}

		[[nodiscard]]
		bool IsEmpty() const
		{
			return 0 == m_Values.GetSize();
		}

		[[nodiscard]]
		size_t GetSize() const
		{
			return m_Values.GetSize();
		}
	};

	// Fixed-Capacity Stack
	template<typename T, size_t Capacity>
	class Stack<T, Capacity, InlineAllocator<T, Capacity>, true>
	{
	private:
		using AllocatorType = InlineAllocator<T, Capacity>;

		static_assert(0 < Capacity,
			"Core::Stack fixed capacity must be greater than 0.");

		static_assert(std::is_destructible_v<T>,
			"Core::Stack fixed mode requires T to be destructible.");

	private:
		Vector<T, AllocatorType> m_Values;

		size_t m_Count = 0;

	public:
		Stack()
		{
			const bool bReserved = m_Values.Reserve(Capacity);
			CHECKF(true == bReserved, L"Core::Stack fixed mode failed to reserve storage.", 1);
		}

		~Stack()
		{
			Clear();
		}

		Stack(const Stack& ) = delete;
		Stack(      Stack&&) = delete;

		Stack& operator=(const Stack& ) = delete;
		Stack& operator=(      Stack&&) = delete;

	public:
		[[nodiscard]]
		bool Push(const T& InputValue)
		{
			static_assert(std::is_copy_constructible_v<T>,
				"Core::Stack::Push(const T&) requires T to be copy constructible.");

			if (true == IsFull())
			{
				return false;
			}

			std::construct_at(GetSlot(m_Count), InputValue);

			++m_Count;

			return true;
		}

		[[nodiscard]]
		bool Push(T&& InputValue)
		{
			static_assert(std::is_move_constructible_v<T>,
				"Core::Stack::Push(T&&) requires T to be move constructible.");

			if (true == IsFull())
			{
				return false;
			}

			std::construct_at(GetSlot(m_Count), std::move(InputValue));

			++m_Count;

			return true;
		}

		template<typename... TArgs>
		[[nodiscard]]
		bool Emplace(TArgs&&... InputArgs)
		{
			static_assert(std::is_constructible_v<T, TArgs&&...>,
				"Core::Stack::Emplace requires T to be constructible from the input arguments.");

			if (true == IsFull())
			{
				return false;
			}

			std::construct_at(GetSlot(m_Count), std::forward<TArgs>(InputArgs)...);

			++m_Count;

			return true;
		}

		[[nodiscard]]
		bool Pop(T& OutValue)
		{
			static_assert(std::is_move_assignable_v<T>,
				"Core::Stack::Pop requires T to be move assignable.");

			if (true == IsEmpty())
			{
				return false;
			}

			--m_Count;

			T* TopSlot = GetSlot(m_Count);
			OutValue = std::move(*TopSlot);
			DestroySlot(TopSlot);

			return true;
		}

		void Pop()
		{
			if (true == IsEmpty())
			{
				return;
			}

			--m_Count;
			DestroySlot(GetSlot(m_Count));
		}

		[[nodiscard]]
		T* Peek()
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return GetSlot(m_Count - 1);
		}

		[[nodiscard]]
		const T* Peek() const
		{
			if (true == IsEmpty())
			{
				return nullptr;
			}

			return GetSlot(m_Count - 1);
		}

		void Clear()
		{
			while (false == IsEmpty())
			{
				Pop();
			}

			m_Count = 0;
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
	inline constexpr bool StackAlwaysFalse = false;

	template<typename T, size_t Capacity, typename AllocatorType>
	class Stack<T, Capacity, AllocatorType, true>
	{
		static_assert
		(
			StackAlwaysFalse<AllocatorType>,
			"Core::Stack fixed mode requires Core::InlineAllocator<T, Capacity>."
		);
	};

	
	template<typename T, typename AllocatorType = DefaultContainerAllocator<T>>
	using DynamicStack = Stack<T, 0, AllocatorType, false>;

	template<typename T, size_t Capacity>
	using FixedStack = Stack<T, Capacity, InlineAllocator<T, Capacity>, true>;
}
