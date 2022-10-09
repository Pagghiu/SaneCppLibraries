#pragma once
#include "Console.h" // SC_BREAK_DEBUGGER
#include "StringView.h"

namespace SC
{
struct TestCase;

struct TestReport
{
    bool       abortOnFirstFailedTest = true;
    uint32_t   numTestsSucceeded      = 0;
    uint32_t   numTestsFailed         = 0;
    StringView firstFailedTest;
    StringView testToRun;
    StringView sectionToRun;

    TestReport(int argc, const char** argv);
    ~TestReport();

    void               testCaseFinished(TestCase& testCase);
    [[nodiscard]] bool isTestEnabled(StringView testName) const;
    [[nodiscard]] bool isSectionEnabled(StringView sectionName) const;
    [[nodiscard]] int  getTestReturnCode() const;
};

struct TestCase
{
    TestCase(TestReport& report, StringView testName);
    ~TestCase();
    void               recordExpectation(StringView expression, bool status);
    [[nodiscard]] bool test_section(StringView sectionName);

    const StringView testName;
    uint32_t         numTestsSucceeded;
    uint32_t         numTestsFailed;
    TestReport&      report;
};
} // namespace SC

#define SC_TEST_EXPECT(e)                                                                                              \
    (__builtin_expect((e), 0) ? recordExpectation(#e, true) : (recordExpectation(#e, false), SC_BREAK_DEBUGGER))
