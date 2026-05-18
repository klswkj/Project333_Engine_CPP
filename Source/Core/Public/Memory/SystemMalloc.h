#include "MallocBase.h"

namespace Core
{
    class SystemMalloc final : public IMallocBase
    {
    public: // IMallocBase interface override
        virtual void* Allocate(size_t InputSize, size_t InputAlignment) override;
        virtual void* Reallocate(void* InputPtr, size_t InputNewSize, size_t InputNewAlignment) override;
        virtual void Deallocate(void* InputPtr) override;
    };
}