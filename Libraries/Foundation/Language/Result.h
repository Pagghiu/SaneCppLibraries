// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Compiler.h" //forward
namespace SC
{
struct [[nodiscard]] ReturnCode
{
    const char* message;
    explicit constexpr ReturnCode(bool result) : message(result ? nullptr : "Unspecified Error") {}

    template <int SIZE>
    static constexpr ReturnCode Error(const char (&msg)[SIZE])
    {
        return ReturnCode(msg);
    }

    static constexpr ReturnCode FromStableCharPointer(const char* msg) { return ReturnCode(msg); }

    constexpr operator bool() const { return message == nullptr; }

  private:
    explicit constexpr ReturnCode(const char* message) : message(message) {}
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
        if (auto _exprResConv = SC::ReturnCode(expression))                                                            \
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
            return SC::ReturnCode::Error(failedMessage);                                                               \
        }

#define SC_TRUST_RESULT(expression) SC_ASSERT_RELEASE(expression)
