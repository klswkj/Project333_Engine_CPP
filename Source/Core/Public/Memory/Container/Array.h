#pragma once

#include "ContainerPolicies.h"
#include "../../Diagnostics/DebugCheck.h"
#include "../../Logging/Logger.h"

#include <type_traits> // For type traits like std::is_move_constructible, std::is_copy_constructible, etc.
#include <new>     // For placement new
#include <utility> // For std::move, std::forward
#include <cstddef> // For size_t, ptrdiff_t
#include <cassert>

// NewCapacity means element count, not byte size.

namespace Core
{
	template <typename ElementType, typename AllocatorType = DefaultContainerAllocator<ElementType>>
	requires ContainerAllocatorPolicy<AllocatorType>
	class Array final
	{
	private:
		static_assert(std::is_destructible_v<ElementType>, 
			"Array<ElementType> requires T to be destructible.");

		static_assert(std::is_default_constructible_v<ElementType>,
			"Array<ElementType> requires T to be default constructible.");

        // private end
	private:
		AllocatorType m_Allocator;

		ElementType* m_Data;
		size_t       m_Size;

		// private end
	public:
		Array() = delete;

		explicit Array(size_t InputSize)
			: m_Allocator()
			, m_Data(nullptr)
			, m_Size(0)
		{
			const bool bInit = Initialize(InputSize);

			CHECKF(bInit, L"Array constructor failed to allocate or initialize element storage.", 1);

			m_Size = InputSize;
		}

		~Array()
		{
			DestroyElements();

            if (nullptr != m_Data)
            {
                m_Allocator.Deallocate(m_Data);
                m_Data = nullptr;
            }

			m_Size = 0;
		}

        Array(const Array&) = delete;

        Array(Array&& InputOther) noexcept
            : m_Allocator(InputOther.m_Allocator)
            , m_Data(InputOther.m_Data)
            , m_Size(InputOther.m_Size)
        {
            InputOther.m_Data = nullptr;
            InputOther.m_Size = 0;
        }

        Array& operator=(const Array& )          = delete;
        Array& operator=(      Array&&) noexcept = delete;
	
		// public end
	private:
		[[nodiscard]]
		bool Initialize(size_t InputSize)
		{
			assert(0 < InputSize);

			ElementType* NewData = AllocateElementStorage(InputSize);

			if (nullptr == NewData)
			{
				m_Size = 0;
				m_Data = nullptr;
				return false;
			}

			if constexpr (!std::is_trivially_default_constructible_v<ElementType>)
			{
				for (size_t i = 0; i < InputSize; ++i)
				{
					new (NewData + i) ElementType();
				}
			}

			m_Data = NewData;
			m_Size = InputSize;

			return true;
		}

		// private end
	public:
		void Fill(const ElementType& InputValue)
		{
            for (size_t i = 0; i < m_Size; ++i)
            {
                m_Data[i] = InputValue;
            }
		}

	// public end
	public:
              ElementType& operator[](size_t InputIndex)       { assert(InputIndex < m_Size); return m_Data[InputIndex]; }
        const ElementType& operator[](size_t InputIndex) const { assert(InputIndex < m_Size); return m_Data[InputIndex]; }

			  ElementType& Front()       { assert(0 < m_Size); return m_Data[0]; }
        const ElementType& Front() const { assert(0 < m_Size); return m_Data[0]; }

              ElementType& Back()       { assert(0 < m_Size); return m_Data[m_Size - 1]; }
        const ElementType& Back() const { assert(0 < m_Size); return m_Data[m_Size - 1]; }

		// public end
	public:
        size_t Size() const { return m_Size; }
        bool Empty() const { return 0 == m_Size; }

              ElementType* Data()       { return m_Data; }
        const ElementType* Data() const { return m_Data; }

		      AllocatorType& GetAllocator()       { return m_Allocator; }
		const AllocatorType& GetAllocator() const { return m_Allocator; }

              ElementType* Begin()        { return m_Data; }
        const ElementType* Begin()  const { return m_Data; }
        const ElementType* cBegin() const { return m_Data; }

              ElementType* End()        { return m_Data + m_Size; }
        const ElementType* End()  const { return m_Data + m_Size; }
        const ElementType* cEnd() const { return m_Data + m_Size; }
        

		// public end
	private:

		ElementType* AllocateElementStorage(size_t InputSize)
		{
			if (0 == InputSize)
			{
				return nullptr;
			}

			return static_cast<ElementType*>
				(
					m_Allocator.Allocate
					(
						InputSize
					)
				);
		}

        void DestroyElements()
        {
			if (nullptr == m_Data)
			{
				return;
			}

			if constexpr (!std::is_trivially_destructible_v<ElementType>)
			{
				for (size_t i = 0; i < m_Size; ++i)
				{
					m_Data[i].~ElementType();
				}
			}
        }
	};
}