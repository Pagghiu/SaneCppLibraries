// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Test.h"
#include "Time.h"

namespace SC
{
struct TimeTest;
}

struct SC::TimeTest : public SC::TestCase
{
    TimeTest(SC::TestReport& report) : TestCase(report, "TimeTest")
    {
        AbsoluteTime         time = AbsoluteTime::now();
        AbsoluteTime::Parsed local;
        SC_TEST_EXPECT(time.parseLocal(local));
        SC_TEST_EXPECT(local.year > 2022);

        report.console.print("{:02}/{:02}/{} {:02}:{:02}:{:02} {}", local.dayOfMonth, local.month, local.year,
                             local.hour, local.minutes, local.seconds,
                             local.isDaylightSaving ? "DAYLIGHT SAVING"_a8 : "NO DAYLIGHT SAVING"_a8);
    }
};
