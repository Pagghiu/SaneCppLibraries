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
struct Monotonic;
struct Realtime;
struct Relative;
struct HighResolutionCounter;

struct Milliseconds;
struct Nanoseconds;
struct Seconds;
} // namespace Time
} // namespace SC

//! @defgroup group_time Time

//! @addtogroup group_time
//! @copybrief library_time (see @ref library_time for more details)

//! @{

/// @brief Type-safe wrapper of uint64 used to represent nanoseconds
struct SC::Time::Nanoseconds
{
    constexpr Nanoseconds() : ns(0) {}
    constexpr explicit Nanoseconds(int64_t ns) : ns(ns){};
    int64_t ns;

    [[nodiscard]] bool operator>(const Nanoseconds other) const { return ns > other.ns; }
    [[nodiscard]] bool operator<(const Nanoseconds other) const { return ns < other.ns; }
    [[nodiscard]] bool operator==(const Nanoseconds other) const { return ns == other.ns; }
};

/// @brief Type-safe wrapper of uint64 used to represent milliseconds
struct SC::Time::Milliseconds
{
    constexpr Milliseconds() : ms(0) {}
    constexpr explicit Milliseconds(int64_t ms) : ms(ms){};
    int64_t ms;

    [[nodiscard]] bool operator>(const Milliseconds other) const { return ms > other.ms; }
    [[nodiscard]] bool operator<(const Milliseconds other) const { return ms < other.ms; }
    [[nodiscard]] bool operator==(const Milliseconds other) const { return ms == other.ms; }
};

/// @brief Type-safe wrapper of uint64 used to represent seconds
struct SC::Time::Seconds
{
    constexpr Seconds() : sec(0) {}
    constexpr explicit Seconds(int64_t sec) : sec(sec){};
    int64_t sec;

    [[nodiscard]] bool operator>(const Seconds other) const { return sec > other.sec; }
    [[nodiscard]] bool operator<(const Seconds other) const { return sec < other.sec; }
    [[nodiscard]] bool operator==(const Seconds other) const { return sec == other.sec; }

    constexpr operator Milliseconds() { return Milliseconds(sec * 1000); }
};

/// @brief Interval of time represented with 64 bit double precision float
struct SC::Time::Relative
{
    /// @brief how many seconds have elapsed in
    Relative() : seconds(0.0) {}

    /// @brief Construct a Relative from milliseconds
    Relative(Milliseconds time) : seconds(static_cast<double>(time.ms / 1e3)) {}

    /// @brief Construct a Relative from nanoseconds
    Relative(Nanoseconds time) : seconds(static_cast<double>(time.ns / 1e9)) {}

    /// @brief Construct a Relative from seconds
    Relative(Seconds time) : seconds(static_cast<double>(time.sec)) {}

    static Relative fromSeconds(double seconds) { return Relative(seconds); }

    [[nodiscard]] bool operator>(const Relative other) const { return seconds > other.seconds; }
    [[nodiscard]] bool operator<(const Relative other) const { return seconds < other.seconds; }
    [[nodiscard]] bool operator==(const Relative other) const { return toNanoseconds() == other.toNanoseconds(); }

    Seconds      toSeconds() const { return Seconds(static_cast<int64_t>(seconds + 0.5)); }
    Nanoseconds  toNanoseconds() const { return Nanoseconds(static_cast<int64_t>(seconds * 1e9 + 0.5)); }
    Milliseconds toMilliseconds() const { return Milliseconds(static_cast<int64_t>(seconds * 1e3 + 0.5)); }

  private:
    Relative(double seconds) : seconds(seconds) {}
    double seconds = 0;
};

// User defined literals
// Using "unsigned long long int" instead of int64_t because it's mandated by the standard.
namespace SC
{
// clang-format off
inline Time::Nanoseconds  operator""_ns(unsigned long long int ns) { return Time::Nanoseconds(static_cast<int64_t>(ns)); }
inline Time::Milliseconds operator""_ms(unsigned long long int ms) { return Time::Milliseconds(static_cast<int64_t>(ms)); }
inline Time::Seconds      operator""_sec(unsigned long long int sec) { return Time::Seconds(static_cast<int64_t>(sec)); }
// clang-format on

} // namespace SC

/// @brief Absolute time as realtime or monotonically increasing clock
/// @see Monotonic
/// @see Realtime
struct SC::Time::Absolute
{
  protected:
    struct Internal;
    int64_t milliseconds;

  public:
    /// @brief Construct an Absolute time equal to epoch
    Absolute() : milliseconds(0) {}

    /// @brief Construct an Absolute from milliseconds since epoch
    /// @param milliseconds Number of milliseconds since epoch
    Absolute(int64_t milliseconds) : milliseconds(milliseconds) {}

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

        const char* getMonth() const;
        const char* getDay() const;
        bool        isDaylightSaving = false;

      private:
        friend struct Internal;
        char monthName[16];
        char dayName[16];
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

    /// @brief Check if this Absolute time is later or equal to another Absolute time
    /// @note Comparing Absolute times obtained with different clock sources (monotonic/realtime) is meaningless
    [[nodiscard]] bool isLaterThanOrEqualTo(Absolute other) const;

    /// @brief Check if this Absolute time is lather than another Absolute time
    /// @note Comparing Absolute times obtained with different clock sources (monotonic/realtime) is meaningless
    [[nodiscard]] bool isLaterThan(Absolute other) const;

    /// @brief Obtains the difference between this time and the other time
    /// @note Subtracting Absolute times obtained with different clock sources (monotonic/realtime) is meaningless
    [[nodiscard]] Milliseconds subtractExact(Absolute other) const;

    /// @brief Offset this absolute time with a relative time in milliseconds
    [[nodiscard]] Absolute offsetBy(Milliseconds other) const;
};

/// @brief Represent monotonically increasing time (use Monotonic::now for current time)
struct SC::Time::Monotonic : public Absolute
{
    using Absolute::Absolute;

    /// @brief Obtain time according to monotonic clock
    [[nodiscard]] static Monotonic now();

    /// @brief Return given time as monotonically incrementing milliseconds
    [[nodiscard]] int64_t getMonotonicMilliseconds() const { return milliseconds; }
};

/// @brief Represents a realtime clock in milliseconds since epoch (use Realtime::now for current time)
struct SC::Time::Realtime : public Absolute
{
    using Absolute::Absolute;

    /// @brief Obtain time according to realtime clock
    [[nodiscard]] static Realtime now();

    /// @brief Return given time as milliseconds since epoch
    [[nodiscard]] int64_t getMillisecondsSinceEpoch() const { return milliseconds; }
};

/// @brief An high resolution time counter
struct SC::Time::HighResolutionCounter
{
    HighResolutionCounter();

    /// @brief Sets HighResolutionCounter to current instant
    /// @n
    /// Example:
    /// @snippet Libraries/Time/Tests/TimeTest.cpp highResolutionCounterSnapSnippet
    HighResolutionCounter& snap();

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

    /// @brief Converts to a Relative struct
    Relative getRelative() const;

    /// @brief Converts to Nanoseconds
    Nanoseconds toNanoseconds() const;

    /// @brief Converts to Milliseconds
    Milliseconds toMilliseconds() const;

    /// @brief Converts to Seconds
    Seconds toSeconds() const;

    int64_t part1;
    int64_t part2;

  private:
    struct Internal;
};

//! @}
