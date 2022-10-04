#include "test.h"
#include "assert.h"
#include "console.h"
#include <stdlib.h> // exit

sanecpp::TestReport::TestReport(int argc, const char** argv)
{
    for (int idx = 1; idx < argc; ++idx)
    {
        const auto param = stringView(argv[idx], strlen(argv[idx]), true);
        if (param == "--test" && testToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                testToRun = stringView(argv[idx + 1], strlen(argv[idx + 1]), true);

                printf("TestReport::Running single test \"%s\"\n", argv[idx + 1]);
            }
        }
        if (param == "--test-section" && sectionToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                sectionToRun = stringView(argv[idx + 1], strlen(argv[idx + 1]), true);

                printf("TestReport::Running single section \"%s\"\n", argv[idx + 1]);
            }
        }
    }
    if (not testToRun.isEmpty() || not sectionToRun.isEmpty())
    {
        printf("\n");
    }
}

sanecpp::TestReport::~TestReport()
{
    printf("TOTAL Succeeded = %d (Failed %d)", numTestsSucceeded + numTestsFailed, numTestsFailed);
    printf("\n---------------------------------------------------\n");
}

//-------------------------------------------------------------------------------------------
sanecpp::TestCase::TestCase(TestReport& report, stringView testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0)
{
    if (report.isTestEnabled(testName))
    {
        SANECPP_DEBUG_ASSERT(testName.isNullTerminated());
        printf("[[%s]]\n", testName.getText());
        report.firstFailedTest = stringView();
    }
}

sanecpp::TestCase::~TestCase()
{
    if (report.isTestEnabled(testName))
    {
        printf("---------------------------------------------------\n");
        printf("Succeeded = %d (Failed %d)\n", numTestsSucceeded + numTestsFailed, numTestsFailed);
        printf("---------------------------------------------------\n");
        report.numTestsFailed += numTestsFailed;
        report.numTestsSucceeded += numTestsSucceeded;
        report.testCaseFinished(*this);
    }
}

void sanecpp::TestCase::recordExpectation(stringView expression, bool status)
{
    SANECPP_DEBUG_ASSERT(expression.isNullTerminated());
    if (status)
    {
        numTestsSucceeded++;
        // printf("  \033[32m[SUCCESS]\033[0m %s\n", expression.getText());
        // printf("\t\t[SUCC] %s\n", expression.getText());
    }
    else
    {
        numTestsFailed++;
        // printf("  \033[31m[FAILED]\033[0m %s\n", expression.getText());
        printf("\t\t[FAIL] %s\n", expression.getText());
        if (report.firstFailedTest.isEmpty())
        {
            report.firstFailedTest = expression;
        }
    }
}

bool sanecpp::TestCase::START_SECTION(stringView sectionName)
{
    if (report.isTestEnabled(testName) && report.isSectionEnabled(sectionName))
    {
        SANECPP_DEBUG_ASSERT(sectionName.isNullTerminated());
        printf("\t- %s::%s\n", testName.getText(), sectionName.getText());
        return true;
    }
    return false;
}

void sanecpp::TestReport::testCaseFinished(TestCase& testCase)
{
    if (abortOnFirstFailedTest && testCase.numTestsFailed > 0)
    {
        printf("---------------------------------------------------\n"
               "FAILED TEST\n"
               "%s\n"
               "---------------------------------------------------\n",
               firstFailedTest.getText());
#if SANECPP_RELEASE
        ::exit(-1);
#endif
    }
}

bool sanecpp::TestReport::isTestEnabled(stringView testName) const
{
    return testToRun.isEmpty() || testToRun == testName;
}

bool sanecpp::TestReport::isSectionEnabled(stringView sectionName) const
{
    return sectionToRun.isEmpty() || sectionName == sectionToRun;
}

int sanecpp::TestReport::getTestReturnCode() const
{
    if (numTestsFailed > 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}
