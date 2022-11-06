#pragma once

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
} // namespace SC
