// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_RESULT_ERROR_FORMATTER_DEFINITION_H
#if SC_FOUNDATION_RESULT_ERROR_FORMATTER_DEFINITION_H != 1
#error "ResultErrorFormatter.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_RESULT_ERROR_FORMATTER_DEFINITION_H 1

#include "PrimitiveDefinitions.h"
#include "Span.h"

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Outcome of writing an optional human-readable Result diagnostic.
enum class ResultErrorFormatStatus : uint8_t
{
    Success,
    InsufficientCapacity,
    NotAnError,
    ForeignCategory,
    UnknownError,
};

/// @brief Allocation-free formatting outcome that does not recursively use Result.
struct ResultErrorFormat
{
    ResultErrorFormatStatus status;
    size_t                  requiredCapacity;

    /// @brief Returns true only when the complete null-terminated message was written.
    constexpr operator bool() const { return status == ResultErrorFormatStatus::Success; }
};

/// @brief Helpers shared by opt-in, library-owned error formatters and application translations.
struct ResultErrorFormatter
{
    /// @brief Copies UTF-8 text to output and appends a null terminator.
    /// @details requiredCapacity includes the terminator. Insufficient output is cleared and never receives a partial
    /// message. Passing an empty output span performs an exact sizing query.
    static ResultErrorFormat formatMessage(Span<const char> message, Span<char> output)
    {
        const size_t requiredCapacity = message.sizeInElements() + 1;
        if (output.sizeInElements() < requiredCapacity)
        {
            clear(output);
            return {ResultErrorFormatStatus::InsufficientCapacity, requiredCapacity};
        }
        for (size_t index = 0; index < message.sizeInElements(); ++index)
        {
            output[index] = message[index];
        }
        output[message.sizeInElements()] = '\0';
        return {ResultErrorFormatStatus::Success, requiredCapacity};
    }

    /// @brief Convenience overload for a null-terminated character array.
    template <size_t NumChars>
    static ResultErrorFormat formatMessage(const char (&message)[NumChars], Span<char> output)
    {
        return formatMessage(Span<const char>(message, NumChars - 1), output);
    }

    /// @brief Produces a non-success status and clears any non-empty output span.
    static ResultErrorFormat failure(ResultErrorFormatStatus status, Span<char> output)
    {
        clear(output);
        return {status, 0};
    }

  private:
    static void clear(Span<char> output)
    {
        if (not output.empty())
            output[0] = '\0';
    }
};

//! @}
} // namespace SC

#endif // SC_FOUNDATION_RESULT_ERROR_FORMATTER_DEFINITION_H
