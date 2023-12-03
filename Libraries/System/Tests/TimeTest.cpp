// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Time.h"
#include "../../Testing/Testing.h"
#include "../../Threading/Threading.h"

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
            Time::Absolute::ParseResult local;
            SC_TEST_EXPECT(Time::Absolute::now().parseLocal(local));
            SC_TEST_EXPECT(local.year > 2022);

            report.console.print("{:02}/{:02}/{} {:02}:{:02}:{:02} {}", local.dayOfMonth, local.month, local.year,
                                 local.hour, local.minutes, local.seconds,
                                 local.isDaylightSaving ? "DAYLIGHT SAVING" : "NO DAYLIGHT SAVING");
        }
        if (test_section("HighResolutionCounter::snap / subtract"))
        {
            Time::HighResolutionCounter start, end;
            start.snap();
            Thread::Sleep(100);
            end.snap();
            Time::Relative elapsed = end.subtractApproximate(start);
            SC_TEST_EXPECT(elapsed.inRoundedUpperMilliseconds().ms < 150 and
                           elapsed.inRoundedUpperMilliseconds().ms > 50);
        }
        if (test_section("HighResolutionCounter::offsetBy"))
        {
            Time::HighResolutionCounter start;
            start.snap();
            const Time::HighResolutionCounter end     = start.offsetBy(Time::Milliseconds(321));
            Time::Relative                    elapsed = end.subtractApproximate(start);
            SC_TEST_EXPECT(elapsed.inRoundedUpperMilliseconds().ms == 321);
        }
        if (test_section("HighResolutionCounter::isLaterOnOrEqual"))
        {
            Time::HighResolutionCounter start;
            start.snap();
            const Time::HighResolutionCounter end = start.offsetBy(Time::Milliseconds(123));
            SC_TEST_EXPECT(end.isLaterThanOrEqualTo(start));
            SC_TEST_EXPECT(not start.isLaterThanOrEqualTo(end));
        }
    }
};

namespace SC
{
void runTimeTest(SC::TestReport& report) { TimeTest test(report); }
} // namespace SC
