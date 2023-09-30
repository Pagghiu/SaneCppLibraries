// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Strings/StringView.h"

namespace SC
{
struct [[nodiscard]] ReturnCode
{
    StringView message;
    constexpr ReturnCode(bool result) : message(result ? ""_a8 : "Unspecified Error"_a8) {}
    constexpr ReturnCode(const char* message) = delete;
    constexpr ReturnCode(const StringView message) : message(message) {}
    constexpr ReturnCode(const ReturnCode& other) : message(other.message) {}
    constexpr ReturnCode& operator=(const ReturnCode& other)
    {
        message = other.message;
        return *this;
    }
         operator bool() const { return message.isEmpty(); }
    bool isError() const { return not message.isEmpty(); }
};
template <typename F>
struct Deferred
{
    Deferred(F&& f) : f(forward<F>(f)) {}
    ~Deferred()
    {
        if (armed)
            f();
    }
    void disarm() { armed = false; }

  private:
    F    f;
    bool armed = true;
};

template <typename F>
Deferred<F> MakeDeferred(F&& f)
{
    return Deferred<F>(forward<F>(f));
}

} // namespace SC

#define SC_TRY(expression)                                                                                             \
    {                                                                                                                  \
        if (auto _exprResConv = (expression))                                                                          \
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
            return SC::ReturnCode(failedMessage);                                                                      \
        }

#define SC_TRUST_RESULT(expression) SC_ASSERT_RELEASE(expression)
