// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Types.h"

namespace SC
{
struct AbsoluteTime;
struct RelativeTime;
} // namespace SC

struct SC::AbsoluteTime
{
    AbsoluteTime(int64_t millisecondsSinceEpoch) : millisecondsSinceEpoch(millisecondsSinceEpoch) {}

    [[nodiscard]] static AbsoluteTime now();

    struct Parsed
    {
        bool isDaylightSaving = false;

        uint16_t year       = 0;
        uint8_t  month      = 0;
        uint8_t  dayOfMonth = 0;
        uint8_t  dayOfWeek  = 0;
        uint8_t  dayOfYear  = 0;
        uint8_t  hour       = 0;
        uint8_t  minutes    = 0;
        uint8_t  seconds    = 0;
    };

    [[nodiscard]] bool parseLocal(Parsed& result) const;
    [[nodiscard]] bool parseUTC(Parsed& result) const;
    int64_t            millisecondsSinceEpoch;

  private:
    struct Internal;
};
