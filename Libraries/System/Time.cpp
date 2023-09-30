// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include <time.h>

#include "../Foundation/Base/Limits.h"
#include "Time.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sys/timeb.h>
#else
#include <math.h> // round
#endif

struct SC::AbsoluteTime::Internal
{
    static void tmToParsed(const struct tm& result, Parsed& local)
    {
        local.year             = static_cast<decltype(local.year)>(1900 + result.tm_year);
        local.month            = static_cast<decltype(local.month)>(result.tm_mon);
        local.dayOfMonth       = static_cast<decltype(local.dayOfMonth)>(result.tm_mday);
        local.dayOfWeek        = static_cast<decltype(local.dayOfWeek)>(result.tm_wday);
        local.dayOfYear        = static_cast<decltype(local.dayOfYear)>(result.tm_yday);
        local.hour             = static_cast<decltype(local.hour)>(result.tm_hour);
        local.minutes          = static_cast<decltype(local.minutes)>(result.tm_min);
        local.seconds          = static_cast<decltype(local.seconds)>(result.tm_sec);
        local.isDaylightSaving = result.tm_isdst > 0;
    }
};

SC::AbsoluteTime SC::AbsoluteTime::now()
{
#if SC_PLATFORM_WINDOWS
    struct _timeb t;
    _ftime_s(&t);
    return static_cast<int64_t>(t.time) * 1000 + t.millitm;
#else
    struct timespec tspec;
    clock_gettime(CLOCK_REALTIME, &tspec);
    return static_cast<int64_t>(round(tspec.tv_nsec / 1.0e6) + tspec.tv_sec * 1000);
#endif
}

bool SC::AbsoluteTime::parseLocal(Parsed& result) const
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
    Internal::tmToParsed(parsedTm, result);
    return true;
}

bool SC::AbsoluteTime::parseUTC(Parsed& result) const
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
    Internal::tmToParsed(parsedTm, result);
    return true;
}

SC::RelativeTime SC::AbsoluteTime::subtract(AbsoluteTime other)
{
    const auto diff = millisecondsSinceEpoch - other.millisecondsSinceEpoch;
    return RelativeTime::fromSeconds(diff / 1000.0);
}

SC::TimeCounter::TimeCounter()
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

SC::TimeCounter& SC::TimeCounter::snap()
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

SC::TimeCounter SC::TimeCounter::offsetBy(IntegerMilliseconds other) const
{
    TimeCounter newCounter = *this;
#if SC_PLATFORM_WINDOWS
    newCounter.part1 += other.ms * part2 / 1000;
#else
    constexpr int32_t millisecondsToNanoseconds = 1e6;
    newCounter.part1 += other.ms / 1000;
    newCounter.part2 += (other.ms % 1000) * millisecondsToNanoseconds;
#endif
    return newCounter;
}

bool SC::TimeCounter::isLaterThanOrEqualTo(TimeCounter other) const
{
#if SC_PLATFORM_WINDOWS
    return part1 >= other.part1;
#else
    return (part1 > other.part1) or ((part1 == other.part1) and (part2 >= other.part2));
#endif
}

SC::RelativeTime SC::TimeCounter::subtractApproximate(TimeCounter other) const
{
    TimeCounter res = subtractExact(other);
#if SC_PLATFORM_WINDOWS
    return RelativeTime::fromSeconds(static_cast<double>(res.part1) / res.part2);
#else
    constexpr int32_t secondsToNanoseconds = 1e9;
    return RelativeTime::fromSeconds(res.part1 + static_cast<double>(res.part2) / secondsToNanoseconds);
#endif
}

[[nodiscard]] SC::TimeCounter SC::TimeCounter::subtractExact(TimeCounter other) const
{
    TimeCounter res;
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
