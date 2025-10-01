// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Time/Time.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"
#include <stdint.h>

namespace SC
{
struct TimeTest;
}

struct SC::TimeTest : public SC::TestCase
{
    inline void testAbsoluteParseLocal();
    inline void testHighResolutionCounterSnap();
    inline void testHighResolutionCounterOffsetBy();
    inline void testHighResolutionCounterIsLaterOn();
    inline void testRelativeTime();
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
            testHighResolutionCounterOffsetBy();
        }
        if (test_section("HighResolutionCounter::isLaterOnOrEqual"))
        {
            testHighResolutionCounterIsLaterOn();
        }
        if (test_section("Relative"))
        {
            testRelativeTime();
        }
    }
};

void SC::TimeTest::testAbsoluteParseLocal()
{
    //! [absoluteParseLocalSnippet]
    Time::Absolute::ParseResult local;
    SC_TEST_EXPECT(Time::Realtime::now().parseLocal(local));
    SC_TEST_EXPECT(local.year > 2022);
    String result;
    (void)StringBuilder::format(result, "{} {:02}/{:02}/{} {:02}:{:02}:{:02} {}", local.getDay(), local.dayOfMonth,
                                local.getMonth(), local.year, local.hour, local.minutes, local.seconds,
                                local.isDaylightSaving ? "DAYLIGHT SAVING" : "NO DAYLIGHT SAVING");

    report.console.printLine(result.view());
    //! [absoluteParseLocalSnippet]
}

void SC::TimeTest::testHighResolutionCounterSnap()
{
    //! [highResolutionCounterSnapSnippet]
    Time::HighResolutionCounter start, end;
    start.snap();
    Thread::Sleep(100);
    end.snap();

    // Test all time conversion methods
    Time::Milliseconds elapsedMs = end.subtractExact(start).toMilliseconds();
    SC_TEST_EXPECT(elapsedMs < 2000_ms and elapsedMs > 0_ms);

    // Test nanoseconds conversion
    Time::Nanoseconds elapsedNs = end.toNanoseconds();
    SC_TEST_EXPECT(elapsedNs.ns > 800000000); // More than 800ms in ns

    // Test seconds conversion
    Time::Seconds elapsedSec = end.toSeconds();
    SC_TEST_EXPECT(elapsedSec.sec >= 0); // Should be roughly 1 second

    // Test relative time and approximate subtraction
    Time::Relative relativeTime = end.getRelative();
    SC_TEST_EXPECT(relativeTime > Time::Relative::fromSeconds(0.8));

    Time::Relative approxTime = end.subtractApproximate(start);
    SC_TEST_EXPECT(approxTime > Time::Relative::fromSeconds(0.05));
    //! [highResolutionCounterSnapSnippet]
}

void SC::TimeTest::testHighResolutionCounterOffsetBy()
{
    //! [highResolutionCounterOffsetBySnippet]
    Time::HighResolutionCounter start, end;
    end = start.offsetBy(Time::Milliseconds(321));

    Time::Milliseconds elapsed = end.subtractExact(start).toMilliseconds();
    SC_TEST_EXPECT(elapsed == 321_ms);

    // Test overflow handling
    Time::Absolute maxTime(static_cast<int64_t>(INT64_MAX - 1000));
    Time::Absolute overflowed = maxTime.offsetBy(Time::Milliseconds(2000));
    SC_TEST_EXPECT(overflowed.milliseconds == INT64_MAX);

    // Test time normalization by adding times that cause nanosecond overflow
    Time::HighResolutionCounter counter;
    counter = counter.offsetBy(999_ms);
    // Add enough milliseconds to cause overflow from nanoseconds to seconds
    counter = counter.offsetBy(2001_ms); // This will cause nanoseconds to overflow the 1e9 limit

    // The total time should be 3 seconds (2001 + 999 = 3000 ms)
    Time::HighResolutionCounter base;
    Time::HighResolutionCounter normalized = counter.subtractExact(base);
    Time::Milliseconds          totalMs    = normalized.toMilliseconds();
    SC_TEST_EXPECT(totalMs == Time::Milliseconds(3000)); // Total time should be 3 seconds
    SC_TEST_EXPECT(normalized.toSeconds() == 3_sec);

    // Test time normalization by subtracting larger from smaller time
    Time::HighResolutionCounter smaller = base;
    smaller                             = smaller.offsetBy(100_ms); // Add 0.1 seconds

    Time::HighResolutionCounter larger = base;
    larger                             = larger.offsetBy(500_ms); // Add 0.5 seconds

    // Subtracting larger from smaller should give normalized negative time
    Time::HighResolutionCounter negDiff   = smaller.subtractExact(larger);
    Time::Milliseconds          negDiffMs = negDiff.toMilliseconds();
    SC_TEST_EXPECT(negDiffMs == Time::Milliseconds(-400)); // -0.4 seconds

    // While subtracting smaller from larger gives positive time
    Time::HighResolutionCounter posDiff   = larger.subtractExact(smaller);
    Time::Milliseconds          posDiffMs = posDiff.toMilliseconds();
    SC_TEST_EXPECT(posDiffMs == Time::Milliseconds(400)); // +0.4 seconds
    //! [highResolutionCounterOffsetBySnippet]
}

void SC::TimeTest::testHighResolutionCounterIsLaterOn()
{
    //! [highResolutionCounterIsLaterOnSnippet]
    Time::HighResolutionCounter start;
    start.snap();
    const Time::HighResolutionCounter end = start.offsetBy(123_ms);
    SC_TEST_EXPECT(end.isLaterThanOrEqualTo(start));
    SC_TEST_EXPECT(not start.isLaterThanOrEqualTo(end));
    //! [highResolutionCounterIsLaterOnSnippet]
}

void SC::TimeTest::testRelativeTime()
{
    Time::Relative relative0 = Time::Relative::fromSeconds(0.0);
    Time::Relative relative1 = 1000000_ns;
    Time::Relative relative2 = 1_sec;
    Time::Relative relative4;
    Time::Relative relative5 = 100_ms;

    SC_TEST_EXPECT(relative0 < relative1);
    SC_TEST_EXPECT(relative1 > relative0);
    SC_TEST_EXPECT(relative0 == relative0);
    SC_TEST_EXPECT(relative1 < relative2);
    SC_TEST_EXPECT(relative0.toNanoseconds() == 0_ns);
    SC_TEST_EXPECT(relative1.toMilliseconds() == 1_ms);
    SC_TEST_EXPECT(relative2.toSeconds() == 1_sec);
    SC_TEST_EXPECT(relative4 == relative0);
    SC_TEST_EXPECT(relative4 < relative5);

    Time::Nanoseconds nanoSecond0;
    Time::Nanoseconds nanoSecond1 = 15_ns;
    SC_TEST_EXPECT(nanoSecond0 < nanoSecond1);
    SC_TEST_EXPECT(nanoSecond1 > nanoSecond0);

    Time::Seconds second0;
    Time::Seconds second1 = 15_sec;
    SC_TEST_EXPECT(second0 < second1);
    SC_TEST_EXPECT(second1 > second0);

    Time::Milliseconds milliseconds0;
    Time::Milliseconds milliseconds1 = second1;
    SC_TEST_EXPECT(milliseconds0 < milliseconds1);
    SC_TEST_EXPECT(milliseconds1 > milliseconds0);

    Time::Monotonic absolute0;
    Time::Monotonic absolute1 = TimeMs{1};
    Time::Realtime  absolute2 = TimeMs{2};

    SC_TEST_EXPECT(absolute1.subtractExact(absolute0) == 1_ms);
    SC_TEST_EXPECT(absolute1.getMonotonicMilliseconds() == 1);
    SC_TEST_EXPECT(absolute2.getMillisecondsSinceEpoch() == 2);
}

namespace SC
{
void runTimeTest(SC::TestReport& report) { TimeTest test(report); }
} // namespace SC
