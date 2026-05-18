#pragma once

#include "../Memory.h"

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <cstring> // For std::memcpy

// -----------------------------------------------
// 1. DefaultContainerAllocator
// 2. InlineAllocator
// -----------------------------------------------

namespace Core
{
	template <typename ElementType>
	class DefaultContainerAllocator final
	{
	public:

		void* Allocate(size_t InputElementCount)
		{
			return Memory::Allocate
			(
				sizeof(ElementType) * InputElementCount,
				alignof(ElementType)
			);
		}

		void* Reallocate(void* InputPtr, size_t InputElementCount)
		{
			return Memory::Reallocate
			(
				InputPtr, 
				sizeof(ElementType) * InputElementCount,
				alignof(ElementType)
			);
		}

		void Deallocate(void* InputPtr)
		{
			Memory::Deallocate(InputPtr);
		}
	};


	template
	<
		typename ElementType,
		size_t   InlineElementCount,
		typename SecondaryAllocatorType = DefaultContainerAllocator<ElementType>
	>
		class InlineAllocator final
	{
	private:
		using StorageType = std::aligned_storage_t<sizeof(ElementType), alignof(ElementType)>;
	
		// private end
	private:
		StorageType            m_InlineStorage[InlineElementCount];
		// alignas(ElementType) std::byte m_InlineStorage[sizeof(ElementType) * InlineElementCount];

		SecondaryAllocatorType m_SecondaryAllocator;

	public:

		void* Allocate(size_t InputElementCount)
		{
			if (InputElementCount <= InlineElementCount)
			{
				return static_cast<void*>(m_InlineStorage);
			}

			return m_SecondaryAllocator.Allocate(InputElementCount);
		}

		void Deallocate(void* InputPtr)
		{
			if (nullptr == InputPtr)
			{
				return;
			}

			if (InputPtr == static_cast<void*>(m_InlineStorage))
			{
				return;
			}

			m_SecondaryAllocator.Deallocate(InputPtr);
		}

		void* Reallocate(void* InputPtr, size_t InputElementCount)
		{
			if (nullptr == InputPtr)
			{
				return Allocate(InputElementCount);
			}

			if (0 == InputElementCount)
			{
				Deallocate(InputPtr);
				return nullptr;
			}

			const bool bInputIsInlineStorage = IsInlineStorage(InputPtr);

			// 입력받은 Element 수가 Inline크기보다 작으면 그대로 InlineStorage를 사용
			if (InputElementCount <= InlineElementCount)
			{
				return static_cast<void*>(m_InlineStorage);
			}

			// 기존 포인터가 inline storage였고, 
			// 새 크기가 inline capacity를 넘으면 Secondary Allocator로 옮김
			if (bInputIsInlineStorage)
			{
				void* NewPtr = m_SecondaryAllocator.Allocate(InputElementCount);

				if (nullptr == NewPtr)
				{
					return nullptr;
				}

				std::memcpy
				(
					NewPtr,
					static_cast<void*>(m_InlineStorage),
					sizeof(ElementType) * InlineElementCount
				);

				return NewPtr;
			}

			// 기존 포인터가 Secondary Allocator가 할당한거였으면, 
			// 걔한테 맡김.
			return m_SecondaryAllocator.Reallocate(InputPtr, InputElementCount);
		}

		bool IsInlineStorage(void* InputPtr) const
		{
			return InputPtr == static_cast<const void*>(m_InlineStorage);
		}
	};

	template <typename AllocatorType>
	concept ContainerAllocatorPolicy = 
		requires(AllocatorType Allocator, void* Ptr, size_t ElementCount)
	{
		{ Allocator.Allocate(       ElementCount) } -> std::same_as<void*>;
		{ Allocator.Reallocate(Ptr, ElementCount) } -> std::same_as<void*>;
		{ Allocator.Deallocate(Ptr              ) } -> std::same_as<void>;
	};
}