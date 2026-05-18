#pragma once

#include "../Diagnostics/DebugCheck.h"
#include "../Logging/Logger.h"

#include <cstddef> // for size_t


// namespace MAP = Core::MemoryAlignmentPolicy;

namespace Core
{
    namespace MemoryAlignmentPolicy
    {
        static constexpr size_t sDefaultAlignment = 16; // Default allocator alignment (can be used for SIMD types)
        static constexpr size_t sMinAlignment = 8;      // Minimum allocator alignment
        
        static constexpr size_t sSizeTBitCount = sizeof(size_t) * 8;

        inline constexpr bool IsPowerOfTwo(size_t InputValue)
        {
            return (0 != InputValue) && (0 == (InputValue & (InputValue - 1)));
        }

        inline constexpr size_t CeilPowerOfTwo(size_t InputValue)
        {
            if (InputValue <= 1)
            {
                return 1;
            }

            --InputValue;


            for (size_t Shift = 1; Shift < sSizeTBitCount; Shift <<= 1)
            {
                InputValue |= InputValue >> Shift;
            }

            return InputValue + 1;
        }

        inline constexpr size_t AdjustAlignment(size_t InputAlignment)
        {
            if (InputAlignment < sMinAlignment)
            {
                return sMinAlignment;
            }

            if (InputAlignment <= sDefaultAlignment)
            {
                return sDefaultAlignment;
            }

            #if _DEBUG
                CHECKF(false, L"HeapAllocator only supports up to 16-byte alignment.", 1);
            #endif

            if (false == IsPowerOfTwo(InputAlignment))
            {
                return CeilPowerOfTwo(InputAlignment);
            }

            return InputAlignment;
        }

        inline constexpr size_t AlignUp(size_t InputValue, size_t InputAlignment)
        {
            const size_t Alignment = AdjustAlignment(InputAlignment);

            return (InputValue + Alignment - 1) & ~(Alignment - 1);
        }

    }
}