// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Time.h"

#if SC_PLATFORM_WINDOWS
#include <sys/timeb.h>
#else
#include <math.h> // round
#endif
#include <time.h>

struct SC::AbsoluteTime::Internal
{
    static void tmToParsed(const struct tm& result, Parsed& local)
    {
        local.year             = 1900 + result.tm_year;
        local.month            = result.tm_mon;
        local.dayOfMonth       = result.tm_mday;
        local.dayOfWeek        = result.tm_wday;
        local.dayOfYear        = result.tm_yday;
        local.hour             = result.tm_hour;
        local.minutes          = result.tm_min;
        local.seconds          = result.tm_sec;
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
    return round(tspec.tv_nsec / 1.0e6) + tspec.tv_sec * 1000;
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
