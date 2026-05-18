#pragma once

#include <type_traits>

#define DEFINE_ENUM_CLASS_INT_COMPARISON(EnumType)                                      \
    static_assert(std::is_enum_v<EnumType>, "EnumType must be an enum type.");          \
                                                                                        \
    constexpr bool operator==(EnumType Left, std::underlying_type_t<EnumType> Right)    \
    {                                                                                   \
        return static_cast<std::underlying_type_t<EnumType>>(Left) == Right;            \
    }                                                                                   \
                                                                                        \
    constexpr bool operator==(std::underlying_type_t<EnumType> Left, EnumType Right)    \
    {                                                                                   \
        return Left == static_cast<std::underlying_type_t<EnumType>>(Right);            \
    }                                                                                   \
                                                                                        \
    constexpr bool operator!=(EnumType Left, std::underlying_type_t<EnumType> Right)    \
    {                                                                                   \
        return false == (Left == Right);                                                \
    }                                                                                   \
                                                                                        \
    constexpr bool operator!=(std::underlying_type_t<EnumType> Left, EnumType Right)    \
    {                                                                                   \
        return false == (Left == Right);                                                \
    }