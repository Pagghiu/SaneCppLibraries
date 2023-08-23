// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Assert.h"
#include "Language.h"
#include "StringView.h"

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
template <typename Value, typename ErrorType = ReturnCode>
struct Result;
} // namespace SC

template <typename Value, typename ErrorType>
struct [[nodiscard]] SC::Result
{
    // We cannot have a reference in union, so we use ReferenceWrapper
    typedef typename Conditional<IsReference<Value>::value, ReferenceWrapper<Value>, Value>::type ValueType;

  private:
    union
    {
        ValueType value;
        ErrorType error;
    };
    bool holdsError;

  public:
    SC_CONSTEXPR_CONSTRUCTOR_NEW Result(Value&& v)
    {
        new (&value, PlacementNew()) ValueType(forward<Value>(v));
        holdsError = false;
    }

    SC_CONSTEXPR_CONSTRUCTOR_NEW Result(ErrorType&& e)
    {
        new (&error, PlacementNew()) ErrorType(e);
        holdsError = true;
    }

    SC_CONSTEXPR_DESTRUCTOR ~Result()
    {
        if (not holdsError)
        {
            value.~ValueType();
        }
        else
        {
            error.~ErrorType();
        }
    }
    constexpr Result() = delete;
#if SC_CPP_AT_LEAST_20
    // In C++ 20 we can force this to actually _never_ copy/move :)
    constexpr Result(Result&& other)      = delete;
    constexpr Result(const Result& other) = delete;
#else
    SC_CONSTEXPR_CONSTRUCTOR_NEW Result(Result&& other) noexcept
    {
        holdsError = other.holdsError;
        if (holdsError)
        {
            new (&error, PlacementNew()) ErrorType(move(other.error));
        }
        else
        {
            new (&value, PlacementNew()) ValueType(move(other.value));
        }
    }
    SC_CONSTEXPR_CONSTRUCTOR_NEW Result(const Result& other)
    {
        holdsError = other.holdsError;
        if (holdsError)
        {
            new (&error, PlacementNew()) ErrorType(other.error);
        }
        else
        {
            new (&value, PlacementNew()) ValueType(other.value);
        }
    }
#endif
    constexpr Result& operator=(const Result& other) const = delete;
    constexpr Result& operator=(Result&& other) const      = delete;

    constexpr bool isError() const { return holdsError == true; }

    constexpr bool isValid() const { return holdsError == false; }

    constexpr ErrorType releaseError() const { return move(error); }

    Value&& releaseValue() { return move(value); }

    const ErrorType& getError() const
    {
        SC_DEBUG_ASSERT(holdsError);
        return error;
    }

    const Value& getValue() const
    {
        SC_DEBUG_ASSERT(not holdsError);
        return value;
    }
};
#define SC_TRY_IF(expression)                                                                                          \
    {                                                                                                                  \
        if (auto _exprResConv = (expression))                                                                          \
            SC_LIKELY                                                                                                  \
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
        SC_UNLIKELY                                                                                                    \
        {                                                                                                              \
            return SC::ReturnCode(failedMessage);                                                                      \
        }

#define SC__TRY_IMPL2(assignment, expression, Counter)                                                                 \
    auto _temporary_result##Counter = (expression);                                                                    \
    if (_temporary_result##Counter.isError())                                                                          \
        SC_UNLIKELY                                                                                                    \
        {                                                                                                              \
            return _temporary_result##Counter.releaseError();                                                          \
        }                                                                                                              \
    assignment = _temporary_result##Counter.releaseValue()
#define SC__TRY_IMPL1(assignment, expression, Counter) SC__TRY_IMPL2(assignment, expression, Counter)
#define SC_TRY(assignment, expression)                 SC__TRY_IMPL1(assignment, expression, __COUNTER__)

#define SC__MUST_IMPL2(assignment, expression, Counter)                                                                \
    auto _temporary_result##Counter = (expression);                                                                    \
    SC_DEBUG_ASSERT(not _temporary_result##Counter.isError());                                                         \
    assignment = _temporary_result##Counter.releaseValue()

#define SC__MUST_IMPL1(assignment, expression, Counter) SC__MUST_IMPL2(assignment, expression, Counter)

#define SC_MUST(assignment, expression) SC__MUST_IMPL1(assignment, expression, __COUNTER__)
#define SC_TRUST_RESULT(expression)     (void)expression

#define SC_TRY_ASSIGN(destination, source, errorCode)                                                                  \
    {                                                                                                                  \
        constexpr auto maxValue = static_cast<decltype(destination)>(MaxValue());                                      \
        if (processInfo.dwProcessId <= maxValue)                                                                       \
        {                                                                                                              \
            destination = static_cast<decltype(destination)>(source);                                                  \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            return errorCode;                                                                                          \
        }                                                                                                              \
    }
