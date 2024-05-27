// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"

namespace SC
{
/// @brief Absolute, relative time and high frequency counter
namespace Time
{
struct Absolute;
struct Relative;
struct HighResolutionCounter;

struct Milliseconds;
struct Seconds;
} // namespace Time
} // namespace SC

//! @defgroup group_time Time

//! @addtogroup group_time
//! @copybrief library_time (see @ref library_time for more details)

//! @{

/// @brief Type-safe wrapper of uint64 used to represent milliseconds
struct SC::Time::Milliseconds
{
    constexpr Milliseconds() : ms(0) {}
    constexpr explicit Milliseconds(int64_t ms) : ms(ms){};
    int64_t ms;

    bool operator>(const Milliseconds other) const { return ms > other.ms; }
    bool operator<(const Milliseconds other) const { return ms < other.ms; }
};

/// @brief Type-safe wrapper of uint64 used to represent seconds
struct SC::Time::Seconds
{
    constexpr Seconds() : sec(0) {}
    constexpr explicit Seconds(int64_t sec) : sec(sec){};

    constexpr operator Milliseconds() { return Milliseconds(sec * 1000); }
    int64_t   sec;
};

/// @brief Interval of time represented with 64 bit double precision float
struct SC::Time::Relative
{
    /// @brief how many seconds have elapsed in
    Relative() : floatingSeconds(0.0) {}

    /// @brief Construct a Relative from seconds
    /// @param seconds A double representing an interval of time in seconds
    /// @return A Relative representing the given interval in seconds
    static Relative fromSeconds(double seconds) { return Relative(seconds); }

    /// @brief Converts current time to Milliseconds, rounding to upper integer
    /// @return An Milliseconds struct holding the time converted to milliseconds
    Milliseconds inRoundedUpperMilliseconds() const
    {
        return Milliseconds(static_cast<int64_t>(floatingSeconds * 1000.0 + 0.5f));
    }
    Seconds inSeconds() const { return Seconds(static_cast<int64_t>(floatingSeconds)); }

  private:
    Relative(double floatingSeconds) : floatingSeconds(floatingSeconds) {}
    double floatingSeconds = 0;
};

/// @brief Absolute time represented with milliseconds since epoch
struct SC::Time::Absolute
{
    /// @brief Construct an Absolute from milliseconds since epoch
    /// @param millisecondsSinceEpoch Number of milliseconds since epoch
    Absolute(int64_t millisecondsSinceEpoch) : millisecondsSinceEpoch(millisecondsSinceEpoch) {}

    /// @brief Obtain Absolute representing current time
    /// @return An Absolute representing current time
    [[nodiscard]] static Absolute now();

    /// @brief Holds information on a parsed absolute time from Absolute::parseLocal
    struct ParseResult
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
    /// @n
    /// Example:
    /// @snippet Libraries/Time/Tests/TimeTest.cpp absoluteParseLocalSnippet
    [[nodiscard]] bool parseLocal(ParseResult& result) const;

    /// @brief Parses UTC time to a Parsed structure
    /// @param[out] result The Parsed structure holding current date / time
    /// @return `true` if time has been parsed successfully
    [[nodiscard]] bool parseUTC(ParseResult& result) const;

    /// @brief Obtain the Relative by subtracting this Absolute with another one
    /// @param other Another Absolute to be subtracted
    /// @return A Relative representing the time interval between the two Absolute
    [[nodiscard]] Relative subtract(Absolute other);

    /// @brief Return given time as milliseconds since epoch
    /// @return Time in milliseconds since epoch
    [[nodiscard]] int64_t getMillisecondsSinceEpoch() const { return millisecondsSinceEpoch; }

  private:
    struct Internal;
    int64_t millisecondsSinceEpoch;
};

/// @brief An high resolution time counter
struct SC::Time::HighResolutionCounter
{
    HighResolutionCounter();

    /// @brief Sets HighResolutionCounter to current instant
    /// @n
    /// Example:
    /// @snippet Libraries/Time/Tests/TimeTest.cpp highResolutionCounterSnapSnippet
    void snap();

    /// @brief Returns a HighResolutionCounter offset by a given number of Milliseconds
    /// @param ms How many Milliseconds the returned HighResolutionCounter must be offset of
    /// @return A HighResolutionCounter that is offset by `ms`
    /// @n
    /// Example:
    /// @snippet Libraries/Time/Tests/TimeTest.cpp highResolutionCounterOffsetBySnippet
    [[nodiscard]] HighResolutionCounter offsetBy(Milliseconds ms) const;

    /// @brief Check if this HighResolutionCounter is later or equal to another HighResolutionCounter
    /// @param other The HighResolutionCounter to be used in the comparison
    /// @return `true` if this HighResolutionCounter is later or equal to another HighResolutionCounter
    /// @n
    /// Example:
    /// @snippet Libraries/Time/Tests/TimeTest.cpp highResolutionCounterIsLaterOnSnippet
    [[nodiscard]] bool isLaterThanOrEqualTo(HighResolutionCounter other) const;

    /// @brief Subtracts another HighResolutionCounter from this one, returning an approximate Relative
    /// @param other The HighResolutionCounter to be subtracted
    /// @return A Relative holding the time interval between the two HighResolutionCounter
    /// @n
    /// Example:
    /// @snippet Libraries/Time/Tests/TimeTest.cpp highResolutionCounterOffsetBySnippet
    [[nodiscard]] Relative subtractApproximate(HighResolutionCounter other) const;

    /// @brief Subtracts another HighResolutionCounter from this one, returning a precise HighResolutionCounter
    /// @param other The HighResolutionCounter to be subtracted
    /// @return A HighResolutionCounter holding the time interval between the two HighResolutionCounter
    [[nodiscard]] HighResolutionCounter subtractExact(HighResolutionCounter other) const;

    Relative getRelative() const;

    int64_t part1;
    int64_t part2;

  private:
    struct Internal;
};

//! @}
