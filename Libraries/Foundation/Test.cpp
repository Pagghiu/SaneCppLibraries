#include "Test.h"
#include "Assert.h"
#include "Console.h"
#include <stdlib.h> // exit
namespace SC
{
static const StringView redEMOJI   = "\xf0\x9f\x9f\xa5";
static const StringView greenEMOJI = "\xf0\x9f\x9f\xa9";
} // namespace SC
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
    if (numTestsFailed > 0)
    {
        Console::printUTF8(redEMOJI);
        Console::c_printf(" TOTAL Failed = %d (Succeeded = %d)", numTestsFailed, numTestsSucceeded);
    }
    else
    {
        Console::printUTF8(greenEMOJI);
        Console::c_printf(" TOTAL Succeeded = %d", numTestsSucceeded);
    }
    Console::c_printf("\n---------------------------------------------------\n");
}

//-------------------------------------------------------------------------------------------
SC::TestCase::TestCase(TestReport& report, StringView testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0), numSectionTestsFailed(0),
      printedSection(false)
{
    if (report.isTestEnabled(testName))
    {
        SC_DEBUG_ASSERT(testName.isNullTerminated());
        Console::c_printf("[[ %s ]]\n\n", testName.bytesWithoutTerminator());
        report.firstFailedTest = StringView();
        report.currentSection  = StringView();
    }
}

SC::TestCase::~TestCase()
{
    if (report.isTestEnabled(testName))
    {
        if (not printedSection and not report.currentSection.isEmpty())
        {
            report.printSectionResult(*this);
        }

        Console::printUTF8("\n");
        if (numTestsFailed > 0)
        {
            Console::printUTF8(redEMOJI);
            Console::printUTF8(" [[ ");
            Console::printUTF8(testName);
            Console::printUTF8(" ]]");
            Console::c_printf(" FAILED = %d (Succeeded = %d)\n", numTestsFailed, numTestsSucceeded);
        }
        else
        {
            Console::printUTF8(greenEMOJI);
            Console::printUTF8(" [[ ");
            Console::printUTF8(testName);
            Console::printUTF8(" ]]");
            Console::c_printf(" SUCCEEDED = %d\n", numTestsSucceeded);
        }
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
    }
    else
    {
        numSectionTestsFailed++;
        numTestsFailed++;
        report.printSectionResult(*this);
        printedSection = true;
        Console::printUTF8("\t\t");
        Console::printUTF8(redEMOJI);
        Console::c_printf(" [FAIL] %s\n", expression.bytesWithoutTerminator());
        if (report.firstFailedTest.isEmpty())
        {
            report.firstFailedTest = expression;
        }
    }
}

bool SC::TestCase::test_section(StringView sectionName)
{
    numSectionTestsFailed = 0;
    if (report.isTestEnabled(testName) && report.isSectionEnabled(sectionName))
    {
        SC_DEBUG_ASSERT(sectionName.isNullTerminated());
        if (not report.currentSection.isEmpty())
        {
            report.printSectionResult(*this);
        }
        report.currentSection = sectionName;
        return true;
    }
    else
    {
        report.currentSection = StringView();
        return false;
    }
}

void SC::TestReport::printSectionResult(TestCase& testCase)
{
    Console::printUTF8("\t- ");
    if (testCase.numSectionTestsFailed > 0)
    {
        Console::printUTF8(redEMOJI);
    }
    else
    {
        Console::printUTF8(greenEMOJI);
    }
    Console::c_printf(" %s::%s\n", testCase.testName.bytesWithoutTerminator(), currentSection.bytesWithoutTerminator());
}

void SC::TestReport::testCaseFinished(TestCase& testCase)
{
    if (abortOnFirstFailedTest && testCase.numTestsFailed > 0)
    {
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
