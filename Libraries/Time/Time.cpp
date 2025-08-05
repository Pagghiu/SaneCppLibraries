// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <time.h>

#include "../Time/Time.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sys/timeb.h>
#else
#include <math.h> // round
#endif
#include <stdint.h> // INT64_MAX

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

SC::Time::Realtime SC::Time::Realtime::now()
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

SC::Time::Monotonic SC::Time::Monotonic::now()
{
#if SC_PLATFORM_WINDOWS
    LARGE_INTEGER queryPerformanceFrequency;
    LARGE_INTEGER performanceCounter;
    // Since WinXP this is guaranteed to succeed
    QueryPerformanceFrequency(&queryPerformanceFrequency);
    QueryPerformanceCounter(&performanceCounter);
    return static_cast<int64_t>(performanceCounter.QuadPart * 1000 / queryPerformanceFrequency.QuadPart);

#else
    struct timespec nowTimeSpec;
    clock_gettime(CLOCK_MONOTONIC, &nowTimeSpec);
    int64_t ms = static_cast<int64_t>(nowTimeSpec.tv_sec) * 1000;
    ms += static_cast<int64_t>(nowTimeSpec.tv_nsec / (1000 * 1000));
    return ms;
#endif
}

bool SC::Time::Absolute::parseLocal(ParseResult& result) const
{
    const time_t seconds = static_cast<time_t>(milliseconds / 1000);
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
    const time_t seconds = static_cast<time_t>(milliseconds / 1000);
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

bool SC::Time::Absolute::isLaterThanOrEqualTo(Absolute other) const { return milliseconds >= other.milliseconds; }

bool SC::Time::Absolute::isLaterThan(Absolute other) const { return milliseconds > other.milliseconds; }

SC::Time::Milliseconds SC::Time::Absolute::subtractExact(Time::Absolute other) const
{
    return Milliseconds(milliseconds - other.milliseconds);
}

SC::Time::Absolute SC::Time::Absolute::offsetBy(Milliseconds other) const
{
    constexpr int64_t maxValue = INT64_MAX;
    if (milliseconds > maxValue - other.ms)
    {
        return Time::Absolute(maxValue);
    }
    return Time::Absolute(milliseconds + other.ms);
}

SC::Time::HighResolutionCounter::HighResolutionCounter()
{
    part1 = 0;
#if SC_PLATFORM_WINDOWS
    LARGE_INTEGER queryPerformanceFrequency;
    // Since WinXP this is guaranteed to succeed
    QueryPerformanceFrequency(&queryPerformanceFrequency);
    part2 = queryPerformanceFrequency.QuadPart;
#else
    part2 = 0;
#endif
}

SC::Time::HighResolutionCounter& SC::Time::HighResolutionCounter::snap()
{
#if SC_PLATFORM_WINDOWS
    LARGE_INTEGER performanceCounter;
    QueryPerformanceCounter(&performanceCounter);
    part1 = performanceCounter.QuadPart;
#else
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    part1 = ts.tv_sec;
    part2 = ts.tv_nsec;
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
    // Normalization
    constexpr int32_t secondsToNanoseconds = 1e9;
    while (newCounter.part2 >= secondsToNanoseconds)
    {
        newCounter.part1 += 1;
        newCounter.part2 -= secondsToNanoseconds;
    }
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
    constexpr int32_t secondsToNanoseconds = 1000000000;
    return Relative::fromSeconds(part1 + static_cast<double>(part2) / secondsToNanoseconds);
#endif
}

SC::Time::Nanoseconds SC::Time::HighResolutionCounter::toNanoseconds() const
{
#if SC_PLATFORM_WINDOWS
    return Nanoseconds(static_cast<uint64_t>(part1 * 100000 / part2 * 10000));
#else
    constexpr uint32_t secondsToNanoseconds = 1000000000;
    return Nanoseconds(static_cast<int64_t>(part1) * secondsToNanoseconds + static_cast<int64_t>(part2));
#endif
}

SC::Time::Milliseconds SC::Time::HighResolutionCounter::toMilliseconds() const
{
#if SC_PLATFORM_WINDOWS
    return Milliseconds(static_cast<int64_t>(part1 * 1000 / part2));
#else
    return Milliseconds(static_cast<int64_t>(part1) * 1000 + static_cast<int64_t>(part2) / 1000000);
#endif
}

SC::Time::Seconds SC::Time::HighResolutionCounter::toSeconds() const
{
#if SC_PLATFORM_WINDOWS
    return Seconds(static_cast<int64_t>(part1 / part2));
#else
    constexpr uint32_t secondsToNanoseconds = 1e9;
    return Seconds(static_cast<int64_t>(part1) + static_cast<int64_t>(part2) / secondsToNanoseconds);
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
    int64_t newSeconds     = part1 - other.part1;
    int64_t newNanoseconds = part2 - other.part2;
    // Normalization
    constexpr int32_t secondsToNanoseconds = 1e9;
    while (newNanoseconds < 0)
    {
        newNanoseconds += secondsToNanoseconds;
        newSeconds -= 1;
    }
    res.part1 = newSeconds;
    res.part2 = newNanoseconds;
#endif
    return res;
}
