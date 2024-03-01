#pragma once

namespace SC
{
namespace Algorithm
{

/// @brief Returns the smallest value
/// @tparam Type Type of data to evaluate (requires the < operator)
/// @param first The first value
/// @param second The second value
/// @return The smallest value
template <typename Type>
constexpr Type& min(const Type& first, const Type& second)
{
    return first < second ? first : second;
}

/// @brief Returns the higher value
/// @tparam Type Type of data to evaluate (requires the > operator)
/// @param first The first value
/// @param second The second value
/// @return The higher value
template <typename Type>
constexpr Type& max(const Type& first, const Type& second)
{
    return first > second ? first : second;
}

} // namespace Algorithm
} // namespace SC