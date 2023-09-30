// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Compiler.h" //forward
namespace SC
{
struct [[nodiscard]] Result
{
    const char* message;
    explicit constexpr Result(bool result) : message(result ? nullptr : "Unspecified Error") {}

    template <int numChars>
    static constexpr Result Error(const char (&msg)[numChars])
    {
        return Result(msg);
    }

    static constexpr Result FromStableCharPointer(const char* msg) { return Result(msg); }

    constexpr operator bool() const { return message == nullptr; }

  private:
    explicit constexpr Result(const char* message) : message(message) {}
};
} // namespace SC

#define SC_TRY(expression)                                                                                             \
    {                                                                                                                  \
        if (auto _exprResConv = SC::Result(expression))                                                                \
            SC_LANGUAGE_LIKELY                                                                                         \
            {                                                                                                          \
                (void)0;                                                                                               \
            }                                                                                                          \
        else                                                                                                           \
        {                                                                                                              \
            return _exprResConv;                                                                                       \
        }                                                                                                              \
    }
#define SC_TRY_MSG(expression, failedMessage)                                                                          \
    if (not(expression))                                                                                               \
        SC_LANGUAGE_UNLIKELY                                                                                           \
        {                                                                                                              \
            return SC::Result::Error(failedMessage);                                                                   \
        }

#define SC_TRUST_RESULT(expression) SC_ASSERT_RELEASE(expression)
