// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Compiler.h"
#include "../Foundation/Span.h"
namespace SC
{
namespace Reflection
{
//! @addtogroup group_reflection
//! @{

/// @brief A constexpr array
/// @tparam T Item types
/// @tparam N Maximum number of items
template <typename T, uint32_t N>
struct ArrayWithSize
{
    T        values[N] = {};
    uint32_t size      = 0;

    operator Span<const T>() const { return {values, size}; }
    /// @brief Check if array contains given value, and retrieve index where such item exists
    /// @param value Value to look for
    /// @param outIndex if not `nullptr` will receive the index where item has been found
    /// @return `true` if ArrayWithSize contains given value
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

    /// @brief Appends another sized array to this one (assuming enough space)
    /// @tparam N2 Maximum size of other array
    /// @param other The array to append to this one
    /// @return `true` if there was enough space to append the other array to this one
    template <uint32_t N2>
    [[nodiscard]] constexpr bool append(const ArrayWithSize<T, N2>& other)
    {
        if (size + other.size >= N)
            return false;
        for (uint32_t i = 0; i < other.size; ++i)
        {
            values[size++] = other.values[i];
        }
        return true;
    }

    /// @brief Append a single item to this array
    /// @param value Value to append
    /// @return `true` if there was enough space for the value
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

/// @brief A writable span of objects
template <typename Type>
struct WritableRange
{
    Type* iterator;
    Type* iteratorEnd;

    constexpr WritableRange(Type* iteratorStart, const uint32_t capacity)
        : iterator(iteratorStart), iteratorEnd(iteratorStart + capacity)
    {}

    [[nodiscard]] constexpr bool writeAndAdvance(const Type& value)
    {
        if (iterator < iteratorEnd)
        {
            *iterator = value;
            iterator++;
            return true;
        }
        return false;
    }
};

/// @brief A minimal ASCII StringView with shortened name to be used in TypeToString.
struct Sv
{
    const char* data;   ///< Pointer to the start of ASCII string
    uint32_t    length; ///< Number of bytes of the ASCII string

    /// @brief  Construct empty Sv
    constexpr Sv() : data(nullptr), length(0) {}

    /// @brief Construct Sv from a pointer to a char* and a length
    /// @param data Pointer to the string
    /// @param length Number of bytes representing length of the string
    constexpr Sv(const char* data, uint32_t length) : data(data), length(length) {}

    /// @brief Construct Sv from a string literal
    /// @tparam N Number of characters in the string
    /// @param data Pointer to the array of characters
    template <uint32_t N>
    constexpr Sv(const char (&data)[N]) : data(data), length(N - 1)
    {}
};

/// @brief Returns name of type T (ClNm stands for ClassName, but we shorten it to save bytes on symbol mangling)
/// @tparam T Type to get the name of
/// @return A Sv (StringView) with the given name (works on CLANG, GCC, MSVC)
template <typename T>
static constexpr Sv ClNm()
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
    return Sv(it, length - trim_chars);
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
    return Sv(itStart, static_cast<int>(it - itStart));
#endif
    // clang-format on
}

using TypeStringView = Sv;

/// @brief Strips down class name produced by ClNm to reduce binary size (from C++17 going forward)
/// @tparam T A type that will be stringized
template <typename T>
struct TypeToString
{
#if SC_LANGUAGE_CPP_AT_LEAST_17
  private:
    // In C++ 17 we trim the long string producted by ClassName<T> to reduce executable size
    [[nodiscard]] static constexpr auto TrimClassName()
    {
        constexpr auto className = ClNm<T>();

        ArrayWithSize<char, className.length> trimmedName;
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
    [[nodiscard]] static constexpr TypeStringView get() { return TypeStringView(value.values, value.size); }
#else
    [[nodiscard]] static constexpr TypeStringView get()
    {
        auto className = ClNm<T>();
        return TypeStringView(className.data, className.length);
    }
#endif
};

//! @}

} // namespace Reflection
} // namespace SC
