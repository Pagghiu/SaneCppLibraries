#include "Test.h"
#include "Assert.h"
#include "Console.h"
#include <stdlib.h> // exit

SC::TestReport::TestReport(int argc, const char** argv)
{
    for (int idx = 1; idx < argc; ++idx)
    {
        const auto param = StringView(argv[idx], strlen(argv[idx]), true);
        if (param == "--test" && testToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                testToRun = StringView(argv[idx + 1], strlen(argv[idx + 1]), true);

                Console::c_printf("TestReport::Running single test \"%s\"\n", argv[idx + 1]);
            }
        }
        if (param == "--test-section" && sectionToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                sectionToRun = StringView(argv[idx + 1], strlen(argv[idx + 1]), true);

                Console::c_printf("TestReport::Running single section \"%s\"\n", argv[idx + 1]);
            }
        }
    }
    if (not testToRun.isEmpty() || not sectionToRun.isEmpty())
    {
        Console::c_printf("\n");
    }
}

SC::TestReport::~TestReport()
{
    Console::c_printf("TOTAL Succeeded = %d (Failed %d)", numTestsSucceeded + numTestsFailed, numTestsFailed);
    Console::c_printf("\n---------------------------------------------------\n");
}

//-------------------------------------------------------------------------------------------
SC::TestCase::TestCase(TestReport& report, StringView testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0)
{
    if (report.isTestEnabled(testName))
    {
        SC_DEBUG_ASSERT(testName.isNullTerminated());
        Console::c_printf("[[%s]]\n", testName.bytesWithoutTerminator());
        report.firstFailedTest = StringView();
    }
}

SC::TestCase::~TestCase()
{
    if (report.isTestEnabled(testName))
    {
        Console::c_printf("---------------------------------------------------\n");
        Console::c_printf("Succeeded = %d (Failed %d)\n", numTestsSucceeded + numTestsFailed, numTestsFailed);
        Console::c_printf("---------------------------------------------------\n");
        report.numTestsFailed += numTestsFailed;
        report.numTestsSucceeded += numTestsSucceeded;
        report.testCaseFinished(*this);
    }
}

void SC::TestCase::recordExpectation(StringView expression, bool status)
{
    SC_DEBUG_ASSERT(expression.isNullTerminated());
    if (status)
    {
        numTestsSucceeded++;
        // Console::c_printf("  \033[32m[SUCCESS]\033[0m %s\n", expression.bytesWithoutTerminator());
        // Console::c_printf("\t\t[SUCC] %s\n", expression.bytesWithoutTerminator());
    }
    else
    {
        numTestsFailed++;
        // Console::c_printf("  \033[31m[FAILED]\033[0m %s\n", expression.bytesWithoutTerminator());
        Console::c_printf("\t\t[FAIL] %s\n", expression.bytesWithoutTerminator());
        if (report.firstFailedTest.isEmpty())
        {
            report.firstFailedTest = expression;
        }
    }
}

bool SC::TestCase::test_section(StringView sectionName)
{
    if (report.isTestEnabled(testName) && report.isSectionEnabled(sectionName))
    {
        SC_DEBUG_ASSERT(sectionName.isNullTerminated());
        Console::c_printf("\t- %s::%s\n", testName.bytesWithoutTerminator(), sectionName.bytesWithoutTerminator());
        return true;
    }
    return false;
}

void SC::TestReport::testCaseFinished(TestCase& testCase)
{
    if (abortOnFirstFailedTest && testCase.numTestsFailed > 0)
    {
        Console::c_printf("---------------------------------------------------\n"
                          "FAILED TEST\n"
                          "%s\n"
                          "---------------------------------------------------\n",
                          firstFailedTest.bytesWithoutTerminator());
#if SC_RELEASE
        ::exit(-1);
#endif
    }
}

bool SC::TestReport::isTestEnabled(StringView testName) const { return testToRun.isEmpty() || testToRun == testName; }

bool SC::TestReport::isSectionEnabled(StringView sectionName) const
{
    return sectionToRun.isEmpty() || sectionName == sectionToRun;
}

int SC::TestReport::getTestReturnCode() const
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
