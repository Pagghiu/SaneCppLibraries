#pragma once
#include "Assert.h"
#include "Language.h"
#include "StringView.h"

namespace SC
{
struct [[nodiscard]] Error
{
    const StringView message;
    constexpr Error() {}
    constexpr Error(const StringView message) : message(message) {}
};
template <typename Value, typename Error = Error>
struct Result;
} // namespace SC

template <typename Value, typename Error>
struct [[nodiscard]] SC::Result
{
    // We cannot have a reference in union, so we use ReferenceWrapper
    typedef typename Conditional<IsReference<Value>::value, ReferenceWrapper<Value>, Value>::type ValueType;

  private:
    union
    {
        ValueType value;
        Error     error;
    };
    bool holdsError;

  public:
    SC_CONSTEXPR_CONSTRUCTOR_NEW Result(Value&& v)
    {
        new (&value, PlacementNew()) ValueType(forward<Value>(v));
        holdsError = false;
    }

    SC_CONSTEXPR_CONSTRUCTOR_NEW Result(Error&& e)
    {
        new (&error, PlacementNew()) Error(e);
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
            error.~Error();
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
            new (&error, PlacementNew()) Error(move(other.error));
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
            new (&error, PlacementNew()) Error(other.error);
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

    constexpr Error releaseError() const { return move(error); }

    Value releaseValue() const { return move(value); }

    const Error& getError() const
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
    if (!(expression))                                                                                                 \
    {                                                                                                                  \
        return false;                                                                                                  \
    }
#define SC_TRY_WRAP(expression, failedMessage)                                                                         \
    if (!(expression))                                                                                                 \
    {                                                                                                                  \
        return Error{failedMessage};                                                                                   \
    }

#define SC__TRY_IMPL2(assignment, expression, Counter)                                                                 \
    auto _temporary_result##Counter = (expression);                                                                    \
    if (_temporary_result##Counter.isError())                                                                          \
    {                                                                                                                  \
        return _temporary_result##Counter.releaseError();                                                              \
    }                                                                                                                  \
    assignment = _temporary_result##Counter.releaseValue()
#define SC__TRY_IMPL1(assignment, expression, Counter) SC__TRY_IMPL2(assignment, expression, Counter)
#define SC_TRY(assignment, expression)                 SC__TRY_IMPL1(assignment, expression, __COUNTER__)

#define SC__MUST_IMPL2(assignment, expression, Counter)                                                                \
    auto _temporary_result##Counter = (expression);                                                                    \
    SC_DEBUG_ASSERT(not _temporary_result##Counter.isError());                                                         \
    assignment = _temporary_result##Counter.releaseValue()

#define SC__MUST_IMPL1(assignment, expression, Counter) SC__MUST_IMPL2(assignment, expression, Counter)

#define SC_MUST(assignment, expression) SC__MUST_IMPL1(assignment, expression, __COUNTER__)
