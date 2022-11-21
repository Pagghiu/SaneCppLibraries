// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Compiler.h"
namespace SC
{
template <typename T, int N>
struct ConstexprArray
{
    T   values[N] = {};
    int size      = 0;

    [[nodiscard]] constexpr bool contains(T value, int* outIndex = nullptr) const
    {
        for (int i = 0; i < size; ++i)
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

    template <int N2>
    [[nodiscard]] constexpr bool append(const ConstexprArray<T, N2>& other)
    {
        if (size + other.size >= N)
            return false;
        for (int i = 0; i < other.size; ++i)
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

struct ConstexprStringView
{
    const char* data;
    int         length;
    constexpr ConstexprStringView() : data(nullptr), length(0) {}
    template <int N>
    constexpr ConstexprStringView(const char (&data)[N]) : data(data), length(N)
    {}
    constexpr ConstexprStringView(const char* data, int length) : data(data), length(length) {}
};
// These are short names because they end up in symbol table (as we're storing their stringized signature)
struct Nm
{
    const char* data;
    int         length;
    constexpr Nm(const char* data, int length) : data(data), length(length) {}
};

template <typename T>
static constexpr Nm ClNm()
{
    // clang-format off
#if SC_MSVC
    const char*    name            = __FUNCSIG__;
    constexpr char separating_char = '<';
    constexpr int  skip_chars      = 8;
    constexpr int  trim_chars      = 7;
#else
    const char*    name            = __PRETTY_FUNCTION__;
    constexpr char separating_char = '=';
    constexpr int  skip_chars      = 2;
    constexpr int  trim_chars      = 1;
#endif
    // clang-format on
    int         length = 0;
    const char* it     = name;
    while (*it != separating_char)
        it++;
    it += skip_chars;
    while (it[length] != 0)
        length++;
    return Nm(it, length - trim_chars);
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
        ConstexprArray<char, className.length> trimmedName;
        for (int i = 0; i < className.length; ++i)
        {
            trimmedName.values[i] = className.data[i];
        }
        trimmedName.size = className.length;
        return trimmedName;
    }

    // Inline static constexpr requires C++17
    static inline constexpr auto value = TrimClassName();

  public:
    [[nodiscard]] static constexpr ConstexprStringView get() { return ConstexprStringView(value.values, value.size); }
#else
    [[nodiscard]] static constexpr ConstexprStringView get()
    {
        auto className = ClNm<T>();
        return ConstexprStringView(className.data, className.length);
    }
#endif
};

} // namespace SC
