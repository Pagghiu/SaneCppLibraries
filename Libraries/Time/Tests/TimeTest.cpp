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
    inline void testAbsoluteParseLocal();
    inline void testHighResolutionCounterSnap();
    inline void testHighResolutionCounterOffseBy();
    inline void testHighResolutionCounterIsLaterOn();
    TimeTest(SC::TestReport& report) : TestCase(report, "TimeTest")
    {
        if (test_section("AbsoluteTime::parseLocal"))
        {
            testAbsoluteParseLocal();
        }
        if (test_section("HighResolutionCounter::snap / subtract"))
        {
            testHighResolutionCounterSnap();
        }
        if (test_section("HighResolutionCounter::offsetBy"))
        {
            testHighResolutionCounterOffseBy();
        }
        if (test_section("HighResolutionCounter::isLaterOnOrEqual"))
        {
            testHighResolutionCounterIsLaterOn();
        }
    }
};

void SC::TimeTest::testAbsoluteParseLocal()
{
    //! [absoluteParseLocalSnippet]
    Time::Absolute::ParseResult local;
    SC_TEST_EXPECT(Time::Absolute::now().parseLocal(local));
    SC_TEST_EXPECT(local.year > 2022);

    report.console.print("{:02}/{:02}/{} {:02}:{:02}:{:02} {}", local.dayOfMonth, local.month, local.year, local.hour,
                         local.minutes, local.seconds,
                         local.isDaylightSaving ? "DAYLIGHT SAVING" : "NO DAYLIGHT SAVING");
    //! [absoluteParseLocalSnippet]
}
void SC::TimeTest::testHighResolutionCounterSnap()
{
    //! [highResolutionCounterSnapSnippet]
    Time::HighResolutionCounter start, end;
    start.snap();
    Thread::Sleep(100);
    end.snap();
    Time::Relative elapsed = end.subtractApproximate(start);
    SC_TEST_EXPECT(elapsed.inRoundedUpperMilliseconds().ms < 150 and elapsed.inRoundedUpperMilliseconds().ms > 50);
    //! [highResolutionCounterSnapSnippet]
}

void SC::TimeTest::testHighResolutionCounterOffseBy()
{
    //! [highResolutionCounterOffsetBySnippet]
    Time::HighResolutionCounter start, end;
    start.snap();
    end                    = start.offsetBy(Time::Milliseconds(321));
    Time::Relative elapsed = end.subtractApproximate(start);
    SC_TEST_EXPECT(elapsed.inRoundedUpperMilliseconds().ms == 321);
    //! [highResolutionCounterOffsetBySnippet]
}
void SC::TimeTest::testHighResolutionCounterIsLaterOn()
{
    //! [highResolutionCounterIsLaterOnSnippet]
    Time::HighResolutionCounter start;
    start.snap();
    const Time::HighResolutionCounter end = start.offsetBy(Time::Milliseconds(123));
    SC_TEST_EXPECT(end.isLaterThanOrEqualTo(start));
    SC_TEST_EXPECT(not start.isLaterThanOrEqualTo(end));
    //! [highResolutionCounterIsLaterOnSnippet]
}

namespace SC
{
void runTimeTest(SC::TestReport& report) { TimeTest test(report); }
} // namespace SC
