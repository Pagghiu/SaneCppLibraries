// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Base/Compiler.h"
namespace SC
{
namespace Reflection
{
template <typename T, uint32_t N>
struct SizedArray
{
    T        values[N] = {};
    uint32_t size      = 0;

    [[nodiscard]] constexpr bool contains(T value, uint32_t* outIndex = nullptr) const
    {
        for (uint32_t i = 0; i < size; ++i)
        {
            if (values[i] == value)
            {
                if (outIndex)
                    *outIndex = i;
                return true;
            }
        }
        return false;
    }

    template <uint32_t N2>
    [[nodiscard]] constexpr bool append(const SizedArray<T, N2>& other)
    {
        if (size + other.size >= N)
            return false;
        for (uint32_t i = 0; i < other.size; ++i)
        {
            values[size++] = other.values[i];
        }
        return true;
    }

    [[nodiscard]] constexpr bool push_back(const T& value)
    {
        if (size < N)
        {
            values[size++] = value;
            return true;
        }
        return false;
    }
};

struct SymbolStringView
{
    const char* data;
    uint32_t    length;
    constexpr SymbolStringView() : data(nullptr), length(0) {}
    template <uint32_t N>
    constexpr SymbolStringView(const char (&data)[N]) : data(data), length(N)
    {}
    constexpr SymbolStringView(const char* data, uint32_t length) : data(data), length(length) {}
};
// These are short names because they end up in symbol table (as we're storing their stringized signature)
struct Nm
{
    const char* data;
    uint32_t    length;
    constexpr Nm(const char* data, uint32_t length) : data(data), length(length) {}
};

template <typename T>
static constexpr Nm ClNm()
{
    // clang-format off
#if SC_COMPILER_CLANG || SC_COMPILER_GCC
    const char* name = __PRETTY_FUNCTION__;
    constexpr char separating_char = '=';
    constexpr uint32_t  skip_chars = 2;
    constexpr uint32_t  trim_chars = 1;
    uint32_t         length = 0;
    const char* it = name;
    while (*it != separating_char)
        it++;
    it += skip_chars;
    while (it[length] != 0)
        length++;
    return Nm(it, length - trim_chars);
#else
    const char* name = __FUNCSIG__;
    constexpr char separating_char = '<';
    constexpr char ending_char = '>';
    const char* it = name;
    while (*it != separating_char)
        it++;
    auto itStart = it + 1;
    while (*it != ending_char)
    {
        if (*it == ' ')
            itStart = it + 1;
        it++;
    }
    return Nm(itStart, static_cast<int>(it - itStart));
#endif
}

template <typename T>
struct TypeToString
{
#if SC_CPP_AT_LEAST_17
private:
    // In C++ 17 we trim the long string producted by ClassName<T> to reduce executable size
    [[nodiscard]] static constexpr auto TrimClassName()
    {
        constexpr auto                         className = ClNm<T>();
        SizedArray<char, className.length> trimmedName;
        for (uint32_t i = 0; i < className.length; ++i)
        {
            trimmedName.values[i] = className.data[i];
        }
        trimmedName.size = className.length;
        return trimmedName;
    }
    
    // Inline static constexpr requires C++17
    static inline constexpr auto value = TrimClassName();
    
public:
    [[nodiscard]] static constexpr SymbolStringView get() { return SymbolStringView(value.values, value.size); }
#else
    [[nodiscard]] static constexpr SymbolStringView get()
    {
        auto className = ClNm<T>();
        return SymbolStringView(className.data, className.length);
    }
#endif
};
}
} // namespace SC
