// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct TestingTest;
}

struct SC::TestingTest : public SC::TestCase
{
    bool intercept = false;

    int boolCalls   = 0;
    int resultCalls = 0;

    StringSpan expression;
    bool       lastStatus = false;
    Result     lastResult = Result(true);

    bool recordExpectation(StringSpan text, bool status)
    {
        if (not intercept)
        {
            return TestCase::recordExpectation(text, status);
        }
        boolCalls++;
        expression = text;
        lastStatus = status;
        return status;
    }

    bool recordExpectation(StringSpan text, Result status)
    {
        if (not intercept)
        {
            return TestCase::recordExpectation(text, status);
        }
        resultCalls++;
        expression = text;
        lastStatus = static_cast<bool>(status);
        lastResult = status;
        return static_cast<bool>(status);
    }

    TestingTest(TestReport& report) : TestCase(report, "TestingTest")
    {
        if (test_section("expectation dispatch and single evaluation"))
        {
            const bool debugBreakOnFailure = report.debugBreakOnFailedTest;
            report.debugBreakOnFailedTest  = false;
            intercept                      = true;

            int evaluations = 0;
            SC_TEST_EXPECT(++evaluations == 0);
            const bool falseForwarded = not lastStatus;
            const bool textForwarded  = expression == StringSpan("++evaluations == 0") and
                                       expression.isNullTerminated() and
                                       expression.getEncoding() == StringEncoding::Ascii;

            SC_TEST_EXPECT(++evaluations == 2);
            const bool trueForwarded = lastStatus;

            SC_TEST_EXPECT((++evaluations, Result::Error(ResultCategory(123), 7)));
            const bool errorForwarded = not lastStatus and lastResult.isError(ResultCategory(123), 7);

            SC_TEST_EXPECT((++evaluations, Result(true)));
            const bool successForwarded = lastStatus;

            const Result error = Result::Error(ResultCategory(124), 8);
            SC_TEST_EXPECT(error);
            const bool referenceForwarded = not lastStatus and lastResult.isError(ResultCategory(124), 8);

            struct BitFields
            {
                unsigned int success : 1;
                unsigned int failure : 1;
            } bits{1, 0};
            SC_TEST_EXPECT(bits.success);
            const bool trueBitForwarded = lastStatus;
            SC_TEST_EXPECT(bits.failure);
            const bool falseBitForwarded = not lastStatus;

            struct NoncopyableStatus
            {
                int& conversions;

                explicit NoncopyableStatus(int& conversions) : conversions(conversions) {}
                NoncopyableStatus(const NoncopyableStatus&) = delete;
                operator bool()
                {
                    conversions++;
                    return true;
                }
            };
            int               conversions = 0;
            NoncopyableStatus convertible(conversions);
            SC_TEST_EXPECT(convertible);
            const bool conversionForwarded = lastStatus and conversions == 1;

            intercept                     = false;
            report.debugBreakOnFailedTest = debugBreakOnFailure;

            SC_TEST_EXPECT(evaluations == 4);
            SC_TEST_EXPECT(boolCalls == 5);
            SC_TEST_EXPECT(resultCalls == 3);
            SC_TEST_EXPECT(falseForwarded);
            SC_TEST_EXPECT(textForwarded);
            SC_TEST_EXPECT(trueForwarded);
            SC_TEST_EXPECT(errorForwarded);
            SC_TEST_EXPECT(successForwarded);
            SC_TEST_EXPECT(referenceForwarded);
            SC_TEST_EXPECT(trueBitForwarded);
            SC_TEST_EXPECT(falseBitForwarded);
            SC_TEST_EXPECT(conversionForwarded);
        }
    }
};

namespace SC
{
void runTestingTest(TestReport& report) { TestingTest test(report); }
} // namespace SC
