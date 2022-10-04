#pragma once
#include "assert.h"
#include "language.h"
#include "stringView.h"

namespace sanecpp
{
struct [[nodiscard]] error
{
    const stringView message;
    constexpr error() {}
    constexpr error(const stringView message) : message(message) {}
};
template <typename Value, typename Error = error>
class result;
} // namespace sanecpp

template <typename Value, typename Error>
class [[nodiscard]] sanecpp::result
{
    union
    {
        Value value;
        Error error;
    };
    bool holdsError;

  public:
    SANECPP_CONSTEXPR_CONSTRUCTOR_NEW result(Value&& v)
    {
        new (&value, PlacementNew()) Value(forward<Value>(v));
        holdsError = false;
    }

    SANECPP_CONSTEXPR_CONSTRUCTOR_NEW result(Error&& e)
    {
        new (&error, PlacementNew()) Error(e);
        holdsError = true;
    }

    SANECPP_CONSTEXPR_DESTRUCTOR ~result()
    {
        if (not holdsError)
        {
            value.~Value();
        }
        else
        {
            error.~Error();
        }
    }
    constexpr result() = delete;
#if SANECPP_CPP_AT_LEAST_20
    // In C++ 20 we can force this to actually _never_ copy/move :)
    constexpr result(result&& other)      = delete;
    constexpr result(const result& other) = delete;
#else
    SANECPP_CONSTEXPR_CONSTRUCTOR_NEW result(result&& other)
    {
        holdsError = other.holdsError;
        if (holdsError)
        {
            new (&error, PlacementNew()) Error(move(other.error));
        }
        else
        {
            new (&value, PlacementNew()) Value(move(other.value));
        }
    }
    SANECPP_CONSTEXPR_CONSTRUCTOR_NEW result(const result& other)
    {
        holdsError = other.holdsError;
        if (holdsError)
        {
            new (&error, PlacementNew()) Error(other.error);
        }
        else
        {
            new (&value, PlacementNew()) Value(other.value);
        }
    }
#endif
    constexpr result& operator=(const result& other) const = delete;
    constexpr result& operator=(result&& other) const      = delete;

    constexpr bool isError() const { return holdsError == true; }

    constexpr Error releaseError() const { return move(error); }

    constexpr Value releaseValue() const { return move(value); }

    const Error& getError() const
    {
        SANECPP_DEBUG_ASSERT(holdsError);
        return error;
    }

    const Value& getValue() const
    {
        SANECPP_DEBUG_ASSERT(not holdsError);
        return value;
    }
};
#define SANECPP_TRY_IF(expression)                                                                                     \
    if (!(expression))                                                                                                 \
    {                                                                                                                  \
        return false;                                                                                                  \
    }
#define SANECPP_TRY_WRAP(expression, failedMessage)                                                                    \
    if (!(expression))                                                                                                 \
    {                                                                                                                  \
        return error{failedMessage};                                                                                   \
    }
#define SANECPP_TRY(expression)                                                                                        \
    ({                                                                                                                 \
        auto _temporary_result = (expression);                                                                         \
        if (_temporary_result.isError()) [[unlikely]]                                                                  \
            return _temporary_result.releaseError();                                                                   \
        _temporary_result.releaseValue();                                                                              \
    })
#define SANECPP_MUST(expression)                                                                                       \
    ({                                                                                                                 \
        auto _temporary_result = (expression);                                                                         \
        SANECPP_DEBUG_ASSERT(!_temporary_result.isError());                                                            \
        _temporary_result.releaseValue();                                                                              \
    })
