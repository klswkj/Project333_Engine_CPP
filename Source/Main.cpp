#include "Core/Public/Memory/Memory.h"
#include "Engine/Public/Application/Application.h"
#include "Core/Public/Memory/Allocator/HeapAllocator.h"

int main()
{
    int Result = 0;

    Core::HeapAllocator MainHeapAllocator(8000000 * sizeof(int));

    Result = Core::Memory::Initialize();

    if (0 != Result)
    {
        return Result;
    }

    {
        Application Project333Application;

        Result = Project333Application.Run();
    }

    Core::Memory::Shutdown();

    return Result;
}