#pragma once

#include <cstddef> // for size_t

namespace Core
{
    // Dereived Classes :
    // SystemMallloc.h
    class IMallocBase
    {
    public:
        virtual ~IMallocBase() = default;

        virtual void* Allocate(size_t InputSize, size_t InputAlignment) = 0;
        virtual void* Reallocate(void* InputPtr, size_t InputNewSize, size_t InputAlignment) = 0;

        virtual void Deallocate(void* InputPtr) = 0;
    };
}