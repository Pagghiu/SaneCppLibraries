// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Threading/Threading.h"
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
        if (test_section("AbsoluteTime::parseLocal"))
        {
            AbsoluteTime         time = AbsoluteTime::now();
            AbsoluteTime::Parsed local;
            SC_TEST_EXPECT(time.parseLocal(local));
            SC_TEST_EXPECT(local.year > 2022);

            report.console.print("{:02}/{:02}/{} {:02}:{:02}:{:02} {}", local.dayOfMonth, local.month, local.year,
                                 local.hour, local.minutes, local.seconds,
                                 local.isDaylightSaving ? "DAYLIGHT SAVING"_a8 : "NO DAYLIGHT SAVING"_a8);
        }
        if (test_section("TimeCounter::snap / subtract"))
        {
            TimeCounter start, end;
            start.snap();
            Thread::Sleep(100);
            end.snap();
            RelativeTime elapsed = end.subtractApproximate(start);
            SC_TEST_EXPECT(elapsed.inRoundedUpperMilliseconds().ms < 150 and
                           elapsed.inRoundedUpperMilliseconds().ms > 50);
        }
        if (test_section("TimeCounter::offsetBy"))
        {
            TimeCounter start;
            start.snap();
            const TimeCounter end     = start.offsetBy(IntegerMilliseconds(321));
            RelativeTime      elapsed = end.subtractApproximate(start);
            SC_TEST_EXPECT(elapsed.inRoundedUpperMilliseconds().ms == 321);
        }
        if (test_section("TimeCounter::isLaterOnOrEqual"))
        {
            TimeCounter start;
            start.snap();
            const TimeCounter end = start.offsetBy(IntegerMilliseconds(123));
            SC_TEST_EXPECT(end.isLaterThanOrEqualTo(start));
            SC_TEST_EXPECT(not start.isLaterThanOrEqualTo(end));
        }
    }
};
