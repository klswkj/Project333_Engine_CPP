#include "../../Public/Memory/SystemMalloc.h"
#include "../../Public/Memory/MemoryAlignmentPolicy.h"

#include <malloc.h> // for _aligned_malloc and _aligned_realloc

namespace MAP = Core::MemoryAlignmentPolicy;

namespace Core
{
	// SystemMalloc implementation

	void* SystemMalloc::Allocate(size_t InputSize, size_t InputAlignment)
	{
		if (0 == InputAlignment)
		{
			return nullptr;
		}

		const size_t Alignment = (MAP::sMinAlignment < InputAlignment) ? MAP::sDefaultAlignment : MAP::sMinAlignment;

#if defined(_WIN32) || defined(_WIN64)
		return _aligned_malloc(InputSize, InputAlignment);
#else
		void* Result = nullptr;

		if (0 != posix_memalign(&Result, InputAlignment, InputSize))
		{
			return nullptr;
		}

		return Result;
#endif
	}

	void* SystemMalloc::Reallocate
	(
		void* InputPtr,
		size_t InputNewSize,
		size_t InputNewAlignment
	)
	{
		if (nullptr == InputPtr)
		{
			return Allocate(InputNewSize, InputNewAlignment);
		}

		if (0 == InputNewSize)
		{
			Deallocate(InputPtr);
			return nullptr;
		}

		const size_t Alignment = MAP::AdjustAlignment(InputNewAlignment);

#if defined(_WIN32) || defined(_WIN64)

		return _aligned_realloc(InputPtr, InputNewSize, Alignment);

#else

		void* NewPtr = nullptr;

		if (0 != posix_memalign(&NewPtr, Alignment, InputNewSize))
		{
			return nullptr;
		}

		const size_t CopySize = (InputNewSize < _msize(InputPtr)) ? InputNewSize : _msize(InputPtr);

		// Copy the data from the old pointer to the new pointer
		// Note: This is a simple implementation and does not handle all edge cases (e.g., if the new size is smaller than the old size)
		memcpy(NewPtr, InputPtr, CopySize);

		// Deallocate the old pointer
		Free(InputPtr);

		return NewPtr;

#endif
	}

	void SystemMalloc::Deallocate(void* InputPtr)
	{
		if (nullptr == InputPtr)
		{
			return;
		}

#if defined(_WIN32) || defined(_WIN64)

		_aligned_free(InputPtr);

#else

		free(InputPtr);

#endif
	}

}