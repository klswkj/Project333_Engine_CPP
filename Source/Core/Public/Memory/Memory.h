#pragma once

#include <cstddef> // for size_t

namespace Core
{
	class IMallocBase;

	namespace Memory
	{
		int Initialize();
		int Shutdown();

		void SetMainAllocator(IMallocBase* InputMalloc);
		IMallocBase* GetMainAllocator();

		void* Allocate(size_t InputSize, size_t InputAlignment);
		void* Reallocate(void* InputPtr, size_t InputNewSize, size_t InputNewAlignment);
		void Deallocate(void* InputPtr);
	}
}