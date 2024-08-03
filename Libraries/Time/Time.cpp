// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <time.h>

#include "../Foundation/Limits.h"
#include "Time.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sys/timeb.h>
#else
#include <math.h> // round
#endif

struct SC::Time::Absolute::Internal
{
    [[nodiscard]] static bool tmToParsed(const struct tm& result, ParseResult& local)
    {
        local.year       = static_cast<decltype(local.year)>(1900 + result.tm_year);
        local.month      = static_cast<decltype(local.month)>(result.tm_mon);
        local.dayOfMonth = static_cast<decltype(local.dayOfMonth)>(result.tm_mday);
        local.dayOfWeek  = static_cast<decltype(local.dayOfWeek)>(result.tm_wday);
        local.dayOfYear  = static_cast<decltype(local.dayOfYear)>(result.tm_yday);
        local.hour       = static_cast<decltype(local.hour)>(result.tm_hour);
        local.minutes    = static_cast<decltype(local.minutes)>(result.tm_min);
        local.seconds    = static_cast<decltype(local.seconds)>(result.tm_sec);

        local.isDaylightSaving = result.tm_isdst > 0;

        if (::strftime(local.monthName, sizeof(local.monthName), "%b", &result) == 0)
            return false;
        if (::strftime(local.dayName, sizeof(local.dayName), "%a", &result) == 0)
            return false;

        return true;
    }
};

const char* SC::Time::Absolute::ParseResult::getMonth() const { return monthName; }

const char* SC::Time::Absolute::ParseResult::getDay() const { return dayName; }

SC::Time::Absolute SC::Time::Absolute::now()
{
#if SC_PLATFORM_WINDOWS
    struct _timeb t;
    _ftime_s(&t);
    return static_cast<int64_t>(t.time) * 1000 + t.millitm;
#else
    struct timespec nowTimeSpec;
    clock_gettime(CLOCK_REALTIME, &nowTimeSpec);
    return static_cast<int64_t>(round(nowTimeSpec.tv_nsec / 1.0e6) + nowTimeSpec.tv_sec * 1000);
#endif
}

bool SC::Time::Absolute::parseLocal(ParseResult& result) const
{
    const time_t seconds = static_cast<time_t>(millisecondsSinceEpoch / 1000);
    struct tm    parsedTm;
#if SC_PLATFORM_WINDOWS
    if (_localtime64_s(&parsedTm, &seconds) != 0)
    {
        return false;
    }
#else
    if (localtime_r(&seconds, &parsedTm) == nullptr)
    {
        return false;
    }
#endif
    return Internal::tmToParsed(parsedTm, result);
}

bool SC::Time::Absolute::parseUTC(ParseResult& result) const
{
    const time_t seconds = static_cast<time_t>(millisecondsSinceEpoch / 1000);
    struct tm    parsedTm;
#if SC_PLATFORM_WINDOWS
    if (_gmtime64_s(&parsedTm, &seconds) != 0)
    {
        return false;
    }
#else
    if (gmtime_r(&seconds, &parsedTm) == nullptr)
    {
        return false;
    }
#endif
    return Internal::tmToParsed(parsedTm, result);
}

SC::Time::Relative SC::Time::Absolute::subtract(Absolute other)
{
    const auto diff = millisecondsSinceEpoch - other.millisecondsSinceEpoch;
    return Relative::fromSeconds(diff / 1000.0);
}

SC::Time::HighResolutionCounter::HighResolutionCounter()
{
    part1 = 0;
#if SC_PLATFORM_WINDOWS
    LARGE_INTEGER queryPerformanceFrequency;
    // Since WinXP this is guaranteed to succeed
    QueryPerformanceFrequency(&queryPerformanceFrequency);
    static_assert(static_cast<decltype(queryPerformanceFrequency.QuadPart)>(MaxValue()) ==
                      static_cast<decltype(part2)>(MaxValue()),
                  "Change part2");
    part2 = queryPerformanceFrequency.QuadPart;
#else
    part2 = 0;
#endif
}

SC::Time::HighResolutionCounter& SC::Time::HighResolutionCounter::snap()
{
#if SC_PLATFORM_WINDOWS
    LARGE_INTEGER performanceCounter;
    static_assert(static_cast<decltype(performanceCounter.QuadPart)>(MaxValue()) ==
                      static_cast<decltype(part1)>(MaxValue()),
                  "Change part1");
    QueryPerformanceCounter(&performanceCounter);
    part1 = performanceCounter.QuadPart;
#else
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    static_assert(static_cast<decltype(ts.tv_sec)>(MaxValue()) <= static_cast<decltype(part1)>(MaxValue()),
                  "Change part1");
    static_assert(static_cast<decltype(ts.tv_nsec)>(MaxValue()) <= static_cast<decltype(part2)>(MaxValue()),
                  "Change part2");
    part1                                       = ts.tv_sec;
    part2                                       = ts.tv_nsec;
#endif
    return *this;
}

SC::Time::HighResolutionCounter SC::Time::HighResolutionCounter::offsetBy(Milliseconds other) const
{
    HighResolutionCounter newCounter = *this;
#if SC_PLATFORM_WINDOWS
    newCounter.part1 += other.ms * part2 / 1000;
#else
    constexpr int32_t millisecondsToNanoseconds = 1e6;
    newCounter.part1 += other.ms / 1000;
    newCounter.part2 += (other.ms % 1000) * millisecondsToNanoseconds;
#endif
    return newCounter;
}

bool SC::Time::HighResolutionCounter::isLaterThanOrEqualTo(HighResolutionCounter other) const
{
#if SC_PLATFORM_WINDOWS
    return part1 >= other.part1;
#else
    return (part1 > other.part1) or ((part1 == other.part1) and (part2 >= other.part2));
#endif
}

SC::Time::Relative SC::Time::HighResolutionCounter::subtractApproximate(HighResolutionCounter other) const
{
    return subtractExact(other).getRelative();
}

SC::Time::Relative SC::Time::HighResolutionCounter::getRelative() const
{
#if SC_PLATFORM_WINDOWS
    return Relative::fromSeconds(static_cast<double>(part1) / part2);
#else
    constexpr int32_t secondsToNanoseconds = 1e9;
    return Relative::fromSeconds(part1 + static_cast<double>(part2) / secondsToNanoseconds);
#endif
}

[[nodiscard]] SC::Time::HighResolutionCounter SC::Time::HighResolutionCounter::subtractExact(
    HighResolutionCounter other) const
{
    HighResolutionCounter res;
#if SC_PLATFORM_WINDOWS
    res.part1 = part1 - other.part1;
    res.part2 = part2;
#else
    int64_t           newSeconds           = part1 - other.part1;
    int64_t           newNanoseconds       = part2 - other.part2;
    constexpr int32_t secondsToNanoseconds = 1e9;
    if (newNanoseconds < 0)
    {
        newNanoseconds += secondsToNanoseconds;
        newSeconds -= 1;
    }
    res.part1 = newSeconds;
    res.part2 = newNanoseconds;
#endif
    return res;
}
