#pragma once
#include "console.h" // SANECPP_BREAK_DEBUGGER
#include "stringView.h"

namespace sanecpp
{
struct TestCase;

struct TestReport
{
    bool       abortOnFirstFailedTest = true;
    uint32_t   numTestsSucceeded      = 0;
    uint32_t   numTestsFailed         = 0;
    stringView firstFailedTest;
    stringView testToRun;
    stringView sectionToRun;

    TestReport(int argc, const char** argv);
    ~TestReport();

    void               testCaseFinished(TestCase& testCase);
    [[nodiscard]] bool isTestEnabled(stringView testName) const;
    [[nodiscard]] bool isSectionEnabled(stringView sectionName) const;
    [[nodiscard]] int  getTestReturnCode() const;
};

struct TestCase
{
    TestCase(TestReport& report, stringView testName);
    ~TestCase();
    void               recordExpectation(stringView expression, bool status);
    [[nodiscard]] bool START_SECTION(stringView sectionName);

    const stringView testName;
    uint32_t         numTestsSucceeded;
    uint32_t         numTestsFailed;
    TestReport&      report;
};
} // namespace sanecpp

#define SANECPP_TEST_EXPECT(e)                                                                                         \
    (__builtin_expect((e), 0) ? recordExpectation(#e, true) : (recordExpectation(#e, false), SANECPP_BREAK_DEBUGGER))
