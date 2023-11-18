// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/PrimitiveTypes.h"

namespace SC
{
struct AbsoluteTime;
struct RelativeTime;
struct TimeCounter;

struct IntegerMilliseconds;
struct IntegerSeconds;
} // namespace SC

//! @addtogroup group_system
//! @{

/// @brief Type-safe wrapper of uint64 used to represent milliseconds
struct SC::IntegerMilliseconds
{
    constexpr IntegerMilliseconds() : ms(0) {}
    constexpr explicit IntegerMilliseconds(int64_t ms) : ms(ms){};
    int64_t ms;
};

/// @brief Type-safe wrapper of uint64 used to represent seconds
struct SC::IntegerSeconds
{
    constexpr IntegerSeconds() : sec(0) {}
    constexpr explicit IntegerSeconds(int64_t sec) : sec(sec){};

    constexpr operator IntegerMilliseconds() { return IntegerMilliseconds(sec * 1000); }
    int64_t   sec;
};

namespace SC
{
constexpr inline SC::IntegerMilliseconds operator""_ms(uint64_t ms)
{
    return IntegerMilliseconds(static_cast<int64_t>(ms));
}
constexpr inline SC::IntegerSeconds operator""_sec(uint64_t sec) { return IntegerSeconds(static_cast<int64_t>(sec)); }
} // namespace SC

/// @brief Interval of time represented with 64 bit double precision float
struct SC::RelativeTime
{
    /// @brief how many seconds have elapsed in
    RelativeTime() : floatingSeconds(0.0) {}

    /// @brief Construct a Relative from seconds
    /// @param seconds A double representing an interval of time in seconds
    /// @return A RelativeTime representing the given interval in seconds
    static RelativeTime fromSeconds(double seconds) { return RelativeTime(seconds); }

    /// @brief Converts current time to IntegerMilliseconds, rounding to upper integer
    /// @return An IntegerMilliseconds struct holding the time converted to milliseconds
    IntegerMilliseconds inRoundedUpperMilliseconds() const
    {
        return IntegerMilliseconds(static_cast<int64_t>(floatingSeconds * 1000.0 + 0.5f));
    }
    IntegerSeconds inSeconds() const { return IntegerSeconds(static_cast<int64_t>(floatingSeconds)); }

  private:
    RelativeTime(double floatingSeconds) : floatingSeconds(floatingSeconds) {}
    double floatingSeconds = 0;
};

/// @brief Absolute time represented with milliseconds since epoch
struct SC::AbsoluteTime
{
    /// @brief Construct an AbsoluteTime from milliseconds since epoch
    /// @param millisecondsSinceEpoch Number of milliseconds since epoch
    AbsoluteTime(int64_t millisecondsSinceEpoch) : millisecondsSinceEpoch(millisecondsSinceEpoch) {}

    /// @brief Obtain AbsoluteTime representing current time
    /// @return An AbsoluteTime representing current time
    [[nodiscard]] static AbsoluteTime now();

    /// @brief Holds information on a parsed absolute time
    struct Parsed
    {
        uint16_t year       = 0;
        uint8_t  month      = 0;
        uint8_t  dayOfMonth = 0;
        uint8_t  dayOfWeek  = 0;
        uint8_t  dayOfYear  = 0;
        uint8_t  hour       = 0;
        uint8_t  minutes    = 0;
        uint8_t  seconds    = 0;

        bool isDaylightSaving = false;
    };

    /// @brief Parses local time to a Parsed structure
    /// @param[out] result The Parsed structure holding current date / time
    /// @return `true` if time has been parsed successfully
    [[nodiscard]] bool parseLocal(Parsed& result) const;

    /// @brief Parses UTC time to a Parsed structure
    /// @param[out] result The Parsed structure holding current date / time
    /// @return `true` if time has been parsed successfully
    [[nodiscard]] bool parseUTC(Parsed& result) const;

    /// @brief Obtain the RelativeTime by subtracting this AbsoluteTime with another one
    /// @param other Another AbsoluteTime to be subtracted
    /// @return A RelativeTime representing the time interval between the two AbsoluteTime
    [[nodiscard]] RelativeTime subtract(AbsoluteTime other);

    /// @brief Return given time as milliseconds since epoch
    /// @return Time in milliseconds since epoch
    [[nodiscard]] int64_t getMillisecondsSinceEpoch() const { return millisecondsSinceEpoch; }

  private:
    struct Internal;
    int64_t millisecondsSinceEpoch;
};

/// @brief An high resolution time counter
struct SC::TimeCounter
{
    TimeCounter();

    /// @brief Sets TimeCounter to current instant
    void snap();

    /// @brief Returns a TimeCounter offset by a given number of IntegerMilliseconds
    /// @param ms How many IntegerMilliseconds the returned TimeCounter must be offset of
    /// @return A TimeCounter that is offset by `ms`
    [[nodiscard]] TimeCounter offsetBy(IntegerMilliseconds ms) const;

    /// @brief Check if this TimeCounter is later or equal to another TimeCounter
    /// @param other The TimeCounter to be used in the comparison
    /// @return `true` if this TimeCounter is later or equal to another TimeCounter
    [[nodiscard]] bool isLaterThanOrEqualTo(TimeCounter other) const;

    /// @brief Subtracts another TimeCounter from this one, returning an approximate RelativeTime
    /// @param other The TimeCounter to be subtracted
    /// @return A RelativeTime holding the time interval between the two TimeCounter
    [[nodiscard]] RelativeTime subtractApproximate(TimeCounter other) const;

    /// @brief Subtracts another TimeCounter from this one, returning a precise TimeCounter
    /// @param other The TimeCounter to be subtracted
    /// @return A TimeCounter holding the time interval between the two TimeCounter
    [[nodiscard]] TimeCounter subtractExact(TimeCounter other) const;

    int64_t part1;
    int64_t part2;

  private:
    struct Internal;
};

//! @}
