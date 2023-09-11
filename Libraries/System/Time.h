// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Types.h"

namespace SC
{
struct AbsoluteTime;
struct RelativeTime;
struct TimeCounter;

struct IntegerMilliseconds;
struct IntegerSeconds;
} // namespace SC

struct SC::IntegerMilliseconds
{
    constexpr IntegerMilliseconds() : ms(0) {}
    constexpr explicit IntegerMilliseconds(int64_t ms) : ms(ms){};
    int64_t ms;
};

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

struct SC::RelativeTime
{
    double floatingSeconds = 0;

    static RelativeTime fromSeconds(double seconds) { return {seconds}; }
    IntegerMilliseconds inRoundedUpperMilliseconds() const
    {
        return IntegerMilliseconds(static_cast<int64_t>(floatingSeconds * 1000.0 + 0.5f));
    }
    IntegerSeconds inSeconds() const { return IntegerSeconds(static_cast<int64_t>(floatingSeconds)); }
};

struct SC::AbsoluteTime
{
    AbsoluteTime(int64_t millisecondsSinceEpoch) : millisecondsSinceEpoch(millisecondsSinceEpoch) {}

    [[nodiscard]] static AbsoluteTime now();

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

    [[nodiscard]] bool parseLocal(Parsed& result) const;
    [[nodiscard]] bool parseUTC(Parsed& result) const;

    [[nodiscard]] RelativeTime subtract(AbsoluteTime other);

    [[nodiscard]] int64_t getMillisecondsSinceEpoch() const { return millisecondsSinceEpoch; }

  private:
    struct Internal;
    int64_t millisecondsSinceEpoch;
};

struct SC::TimeCounter
{
    TimeCounter();
    TimeCounter&               snap();
    [[nodiscard]] TimeCounter  offsetBy(IntegerMilliseconds ms) const;
    [[nodiscard]] bool         isLaterThanOrEqualTo(TimeCounter other) const;
    [[nodiscard]] RelativeTime subtractApproximate(TimeCounter other) const;
    [[nodiscard]] TimeCounter  subtractExact(TimeCounter other) const;

    int64_t part1;
    int64_t part2;

  private:
    struct Internal;
};
