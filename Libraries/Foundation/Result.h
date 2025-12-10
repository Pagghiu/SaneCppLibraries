// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Compiler.h" // SC_LANGUAGE_LIKELY
namespace SC
{
struct SC_COMPILER_EXPORT Result;
//! @addtogroup group_foundation_utility
//! @{

/// @brief An ascii string used as boolean result. #SC_TRY macro forwards errors to caller.
struct [[nodiscard]] Result
{
    /// @brief If == nullptr then Result is valid. If != nullptr it's the reason of the error
    const char* message;
    /// @brief Build a Result object from a boolean.
    /// @param result Passing `true` constructs a valid Result. Passing `false` constructs invalid Result.
    explicit constexpr Result(bool result) : message(result ? nullptr : "Unspecified Error") {}

    /// @brief Constructs an Error from a pointer to an ASCII string literal
    /// @tparam numChars Size of the character array holding the ASCII string
    /// @param msg The custom error message
    /// @return A Result object in invalid state
    template <int numChars>
    static constexpr Result Error(const char (&msg)[numChars])
    {
        return Result(msg);
    }

    /// @brief Constructs an Error from a pointer to an ascii string.
    ///        Caller of this function must ensure such pointer to be valid until Result is used.
    /// @param msg  Pointer to ASCII string representing the message.
    /// @return A Result object in invalid state
    static constexpr Result FromStableCharPointer(const char* msg) { return Result(msg); }

    /// @brief Converts to `true` if the Result is valid, to `false` if it's invalid
    constexpr operator bool() const { return message == nullptr; }

  private:
    explicit constexpr Result(const char* message) : message(message) {}
};
//! @}
} // namespace SC

//! @addtogroup group_foundation_utility
//! @{

/// @brief Checks the value of the given expression and if failed, returns this value to caller
#define SC_TRY(expression)                                                                                             \
    {                                                                                                                  \
        if (auto _exprResConv = SC::Result(expression))                                                                \
            SC_LANGUAGE_LIKELY { (void)0; }                                                                            \
        else                                                                                                           \
        {                                                                                                              \
            return _exprResConv;                                                                                       \
        }                                                                                                              \
    }

/// @brief Checks the value of the given expression and if failed, returns a result with failedMessage to caller
#define SC_TRY_MSG(expression, failedMessage)                                                                          \
    if (not(expression))                                                                                               \
        SC_LANGUAGE_UNLIKELY { return SC::Result::Error(failedMessage); }
//! @}
