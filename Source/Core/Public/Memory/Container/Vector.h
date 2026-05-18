#pragma once

#include "../Allocator/IAllocator.h"
#include "ContainerPolicies.h"

#include <type_traits> // For type traits like std::is_move_constructible, std::is_copy_constructible, etc.
#include <new>     // For placement new
#include <utility> // For std::move, std::forward
#include <cstddef> // For size_t, ptrdiff_t
#include <cassert>

// NewCapacity means element count, not byte size.

namespace Core
{
	template <typename ElementType, typename AllocatorType = DefaultContainerAllocator<ElementType>>
	// requires std::is_nothrow_move_constructible_v<T> 
	// || std::is_copy_constructible_v<T> 
	// || std::is_move_constructible_v<T> 
	// || std::is_trivially_copyable_v<T> 
	// || std::is_trivially_move_constructible_v<T> 
	// || std::is_destructible_v<T>
	requires ContainerAllocatorPolicy<AllocatorType>
	class Vector final
	{
	private:
		AllocatorType m_Allocator;

		ElementType* m_Data;
		size_t m_Size;
		size_t m_Capacity;

	private:
		static constexpr size_t sDefaultInitialCapacity = 8;
		static constexpr size_t sGrowthFactor = 2;

	private:
		static_assert(std::is_destructible_v<ElementType>, 
			"Vector<T> requires T to be destructible.");

		static_assert(std::is_move_constructible_v<ElementType> || std::is_copy_constructible_v<ElementType>,
			"Vector<T> requires T to be move or copy constructible.");

		// private end
	public:

		Vector()
			: m_Allocator()
			, m_Data(nullptr)
			, m_Size(0)
			, m_Capacity(0)
		{
		}

		~Vector()
		{
			Clear();

			if (nullptr != m_Data)
			{
				m_Allocator.Deallocate(m_Data);
				m_Data = nullptr;
			}

			m_Capacity = 0;
		}

		void Clear()
		{
			DestroyRange(m_Data, m_Size);

			m_Size = 0;
		}

		// @todo : 나중에 데이터 소유권(deallocation 책임) 때문에
		// 일단은 delete로 처리해놓고, 나중에 구현
		Vector           (const Vector& )          = delete;
		Vector& operator=(const Vector& )          = delete;
		Vector& operator=(      Vector&&) noexcept = delete;

		Vector(Vector&& InputOther) noexcept
			: m_Allocator(std::move(InputOther.m_Allocator))
			, m_Data(InputOther.m_Data)
			, m_Size(InputOther.m_Size)
			, m_Capacity(InputOther.m_Capacity)
		{
			InputOther.m_Data     = nullptr;
			InputOther.m_Size     = 0;
			InputOther.m_Capacity = 0;
		}

		// public end
	public:
		[[nodiscard]]
		bool Reserve(size_t InputCapacity)
		{
			if (InputCapacity <= m_Capacity)
			{
				return true;
			}

			ElementType* NewData = static_cast<ElementType*>
				(
					m_Allocator.Allocate
					(
						InputCapacity
					)
				);

			if (nullptr == NewData)
			{
				return false;
			}

			ConstructMoveOrCopy(NewData, m_Data, m_Size);
			DestroyRange                (m_Data, m_Size);

			if (nullptr != m_Data)
			{
				m_Allocator.Deallocate(m_Data);
			}

			m_Data     = NewData;
			m_Capacity = InputCapacity;

			return true;
		}

		[[nodiscard]]
		bool Push_back(const ElementType& InputValue)
		{
			static_assert(std::is_copy_constructible_v<ElementType>,
				"push_back(const T&) requires T to be copy constructible.");

			if (false == GrowIfNeeded())
			{
				return false;
			}

			new (m_Data + m_Size) ElementType(InputValue);
			++m_Size;

			return true;
		}

		[[nodiscard]]
		bool Push_back(ElementType&& InputValue)
		{
			static_assert(std::is_move_constructible_v<ElementType>,
				"push_back(T&&) requires T to be move constructible.");

			if (!GrowIfNeeded())
			{
				return false;
			}

			new (m_Data + m_Size) ElementType(std::move(InputValue));

			++m_Size;

			return true;
		}

		template <typename... TArgs>
		[[nodiscard]] 
		bool Emplace_back(TArgs&&... InputArgs)
		{
			if (false == GrowIfNeeded())
			{
				return false;
			}

			new (m_Data + m_Size) ElementType(std::forward<TArgs>(InputArgs)...);

			++m_Size;

			return true;
		}

		void Pop_back()
		{
			assert(0 < m_Size);

			--m_Size;

			DestroyRange(m_Data + m_Size);
		}

		[[nodiscard]]
		bool Resize(size_t InputSize)
		{
			if (InputSize < m_Size)
			{
				DestroyRange(m_Data + InputSize, m_Size - InputSize);

				m_Size = InputSize;

				return true;
			}

			if (m_Size < InputSize)
			{
				static_assert(std::is_default_constructible_v<ElementType>,
					"resize(size_t) requires T to be default constructible.");

				if (m_Capacity < InputSize)
				{
					if (false == Reserve(InputSize))
					{
						return false;
					}
				}

				for (size_t i = m_Size; i < InputSize; ++i)
				{
					new (m_Data + i) ElementType();
				}

				m_Size = InputSize;
			}

			return true;
		}

		void Erase(size_t InputIndex)
		{
			static_assert(std::is_move_assignable_v<ElementType>,
				"Vector::Erase requires ElementType to be move assignable.");

			assert(InputIndex < m_Size);

			// Erase 대상 InputIndex가 마지막 원소가 아니면
			// 뒤의 원소들을 앞으로 한칸씩 당겨야함
			if (InputIndex + 1 < m_Size)
			{
				DestroyRange(m_Data + InputIndex);

				// std::vector처럼 erase에서 move assignment를 요구함

				// InputIndex칸으로
				// move가 noexcept면 예외가 안나니까 안전하게 move가능,
				//                   copy보다 효율적.
				// copy가 불가능하면	move밖에 없으니까 move해야함.
				if constexpr (std::is_nothrow_move_constructible_v<ElementType>
					|| !std::is_copy_constructible_v<ElementType>)
				{
					new (m_Data + InputIndex) ElementType(std::move(m_Data[InputIndex + 1]));
				}
				else
				{
					new (m_Data + InputIndex) ElementType(m_Data[InputIndex + 1]);
				}

				for (size_t i = InputIndex + 1; i + 1 < m_Size; ++i)
				{
					m_Data[i] = std::move(m_Data[i + 1]);
				}

				DestroyRange(m_Data + m_Size - 1);
			}
			else
			{
				DestroyRange(m_Data + InputIndex);
			}

			--m_Size;
		}


		[[nodiscard]]
		bool Insert(size_t InputIndex, const ElementType& InputValue)
		{
			static_assert(std::is_copy_constructible_v<ElementType>,
				"Vector::Insert(size_t, const ElementType&) requires ElementType to be copy_constructible.");

			assert(InputIndex <= m_Size);

			return InsertImpl(InputIndex, InputValue);
		}

		[[nodiscard]]
		bool Insert(size_t InputIndex, ElementType&& InputValue)
		{
			static_assert(std::is_move_constructible_v<ElementType>,
				"Vector::Insert(size_t, const ElementType&&) requires ElementType to be move_constructible.");

			assert(InputIndex <= m_Size);
			
			return InsertImpl(InputIndex, std::move(InputValue));
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
		size_t GetSize()     const { return m_Size; }
		size_t GetCapacity() const { return m_Capacity; }
		bool   IsEmpty()     const { return 0 == m_Size; }

		      AllocatorType& GetAllocator()       { return m_Allocator; }
		const AllocatorType& GetAllocator() const { return m_Allocator; }

			  ElementType* GetData()       { return m_Data; }
		const ElementType* GetData() const { return m_Data; }

			  ElementType* begin()        { return m_Data; }
		const ElementType* begin()  const { return m_Data; }
		const ElementType* cbegin() const { return m_Data; }

			  ElementType* end()        { return m_Data + m_Size; }
		const ElementType* end()  const { return m_Data + m_Size; }
		const ElementType* cend() const { return m_Data + m_Size; }

		// public end
	private:
		
		// 객체생성 없이 Raw-Memory만 확보
		ElementType* AllocateElementStorage(size_t InputCapacity)
		{
            if (0 == InputCapacity)
            {
                return nullptr;
            }

			return static_cast<ElementType*>
				(
					m_Allocator.Allocate
					(
						InputCapacity
					)
				);
		}

		bool GrowIfNeeded()
		{
			if (m_Size < m_Capacity)
			{
				return true;
			}

			const size_t NewCapacity = (0 == m_Capacity) 
				? sDefaultInitialCapacity 
				: m_Capacity * sGrowthFactor;

			return Reserve(NewCapacity);
		}

		template <typename ValueType>
        [[nodiscard]]
        bool InsertImpl(size_t InputIndex, ValueType&& InputValue)
        {
            static_assert(std::is_constructible_v<ElementType, ValueType&&>,
				"Vector::InsertImpl requires ElementType to be constructible from InsertValueType.");

            const size_t NewSize    = m_Size + 1;
			const size_t NewCapacity = (m_Capacity < NewSize)
            ? ((0 == m_Capacity) ? sDefaultInitialCapacity : m_Capacity * sGrowthFactor)
            : m_Capacity;

            ElementType* NewData = AllocateElementStorage(NewCapacity);

            if (nullptr == NewData)
            {
                return false;
            }

			ConstructMoveOrCopy(NewData, m_Data, InputIndex);

			new (NewData + InputIndex) ElementType(std::forward<ValueType>(InputValue));

			ConstructMoveOrCopy
			(
				NewData + InputIndex + 1,
				m_Data  + InputIndex,
				m_Size  - InputIndex
			);

			DestroyRange(m_Data, m_Size);

			if (nullptr != m_Data)
			{
				m_Allocator.Deallocate(m_Data);
			}

			m_Size     = NewSize;
            m_Data     = NewData;
            m_Capacity = NewCapacity;

            return true;
        }

		// !! Source를 Destroy 하지 않음 !!
		// Reserve()나 InsertImpl()에서 범위 Construct가 끝난 뒤
		// DestroyRange()로 소멸자 호출할 것
		static void ConstructMoveOrCopy(ElementType* InputDest, ElementType* InputSrc, size_t InputCnt = 1)
		{
            static_assert(std::is_move_constructible_v<ElementType>
                || std::is_copy_constructible_v<ElementType>,
                "Vector requires ElementType to be move or copy constructible.");

			if (nullptr == InputDest || nullptr == InputSrc || 0 == InputCnt)
			{
				return;
			}

            if constexpr (std::is_nothrow_move_constructible_v<ElementType> 
                || !std::is_copy_constructible_v<ElementType>)
            {
                for (size_t i = 0; i < InputCnt; ++i)
                {
                    new (InputDest + i) ElementType(std::move(InputSrc[i]));
                }
            }
            else
            {
                for (size_t i = 0; i < InputCnt; ++i)
                {
                    new (InputDest + i) ElementType(InputSrc[i]);
                }
            }
		}

		static void MoveOrCopyAndDestroy(ElementType* Dest, ElementType* Src, size_t Count = 1)
		{
			static_assert(std::is_move_constructible_v<ElementType>
				|| std::is_copy_constructible_v<ElementType>,
				"T must be move or copy constructible.");

			if constexpr (std::is_nothrow_move_constructible_v<ElementType>
				|| !std::is_copy_constructible_v<ElementType>)
			{
				for (size_t i = 0; i < Count; ++i)
				{
					new (Dest + i) ElementType(std::move(Src[i]));

					if constexpr (!std::is_trivially_destructible_v<ElementType>)
					{
						Src[i].~ElementType();
					}
				}
			}
			else
			{
				for (size_t i = 0; i < Count; ++i)
				{
					new (Dest + i) ElementType(Src[i]);

					if constexpr (!std::is_trivially_destructible_v<ElementType>)
					{
						Src[i].~ElementType();
					}
				}
			}
		}

		// 새 버퍼 만들어서 삽입결과배열을 구성
		// InsertImpl

		// 원소 여러개 소멸자 호출 (메모리 해제는 아님)
        static void DestroyRange(ElementType* InputData, size_t InputCount = 1)
        {
			if (nullptr == InputData)
			{
				return;
			}

            if constexpr (false == std::is_trivially_destructible_v<ElementType>)
            {
                for (size_t i = 0; i < InputCount; ++i)
                {
                    InputData[i].~ElementType();
                }
            }
        }

		// private end
	};
}