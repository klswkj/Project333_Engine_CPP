#include "../../Public/Memory/Memory.h"
#include "../../Public/Memory/MallocBase.h"
#include "../../Public/Memory/SystemMalloc.h"

#include "../../Public/Logging/Logger.h"

namespace Core
{
	namespace
	{
		IMallocBase* GMainMalloc = nullptr;
		SystemMalloc GDefaultSystemMalloc;
	}

	namespace Memory
	{
		int Initialize()
		{
			if (nullptr == GMainMalloc)
			{
				GMainMalloc = &GDefaultSystemMalloc;
			}

			return 0;
		}

		int Shutdown()
		{
			GMainMalloc = nullptr;

			return 0;
		}

		void SetMainAllocator(IMallocBase* InputMalloc)
		{
			GMainMalloc = InputMalloc;
		}

		IMallocBase* GetMainAllocator()
		{
			return GMainMalloc;
		}

		void* Allocate(size_t InputSize, size_t InputAlignment)
		{
			if (nullptr == GMainMalloc)
			{
				LOG_ERROR(L"Memory::Allocate called before Memory system initialization.", 1);
				return nullptr;
			}
			return GMainMalloc->Allocate(InputSize, InputAlignment);
		}

		void* Reallocate
		(
			void* InputPtr,
			size_t InputNewSize,
			size_t InputNewAlignment
		)
		{
			if (nullptr == GMainMalloc)
			{
				LOG_ERROR(L"Memory::Reallocate called before Memory system initialization.", 1);
				return nullptr;
			}

			return GMainMalloc->Reallocate(InputPtr, InputNewSize, InputNewAlignment);
		}

		void Deallocate(void* InputPtr)
		{
			if (nullptr == InputPtr)
			{
				LOG_ERROR(L"Memory::Deallocate called with nullptr.", 1);
				return;
			}

			if (nullptr == GMainMalloc)
			{
				LOG_ERROR(L"Memory::Deallocate called before Memory system initialization.", 1);
				return;
			}

			GMainMalloc->Deallocate(InputPtr);
		}
	}
}