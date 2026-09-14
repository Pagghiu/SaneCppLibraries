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

/// @brief Allocation-free writer shared by opt-in library formatters and application translations.
struct ResultErrorFormatter
{
    explicit ResultErrorFormatter(Span<char> output) : output(output), canWrite(not output.empty()) {}

    /// @brief Appends UTF-8 text. Call finish() after all fields have been appended.
    void append(Span<const char> text)
    {
        if (text.sizeInElements() > MaxSize - requiredCapacity)
        {
            requiredCapacity = MaxSize;
            canWrite         = false;
            return;
        }
        requiredCapacity += text.sizeInElements();
        if (not canWrite or output.sizeInElements() < requiredCapacity)
        {
            canWrite = false;
            return;
        }
        for (size_t index = 0; index < text.sizeInElements(); ++index)
        {
            output[writtenSize + index] = text[index];
        }
        writtenSize += text.sizeInElements();
    }

    /// @brief Convenience overload for a null-terminated character array.
    template <size_t NumChars>
    void append(const char (&text)[NumChars])
    {
        append(Span<const char>(text, NumChars - 1));
    }

    /// @brief Appends an unsigned decimal integer without allocation.
    void append(uint64_t value)
    {
        char   digits[20];
        size_t position = sizeof(digits);
        do
        {
            digits[--position] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        append(Span<const char>(digits + position, sizeof(digits) - position));
    }

    /// @brief Finishes the message, returning its exact capacity including the null terminator.
    ResultErrorFormat finish()
    {
        if (canWrite)
        {
            output[writtenSize] = '\0';
            return {ResultErrorFormatStatus::Success, requiredCapacity};
        }
        clear(output);
        return {ResultErrorFormatStatus::InsufficientCapacity, requiredCapacity};
    }

    /// @brief Copies UTF-8 text to output and appends a null terminator.
    /// @details requiredCapacity includes the terminator. Insufficient output is cleared and never receives a partial
    /// message. Passing an empty output span performs an exact sizing query.
    static ResultErrorFormat formatMessage(Span<const char> message, Span<char> output)
    {
        ResultErrorFormatter formatter(output);
        formatter.append(message);
        return formatter.finish();
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
    static constexpr size_t MaxSize = ~static_cast<size_t>(0);

    Span<char> output;
    size_t     writtenSize      = 0;
    size_t     requiredCapacity = 1;
    bool       canWrite;

    static void clear(Span<char> output)
    {
        if (not output.empty())
            output[0] = '\0';
    }
};

//! @}
} // namespace SC

#endif // SC_FOUNDATION_RESULT_ERROR_FORMATTER_DEFINITION_H
