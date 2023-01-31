// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
        const auto param = StringView(argv[idx], strlen(argv[idx]), true, StringEncoding::Utf8);
        if (param == "--test" && testToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                testToRun = StringView(argv[idx + 1], strlen(argv[idx + 1]), true, StringEncoding::Utf8);

                Console::print("TestReport::Running single test \"{}\"\n", argv[idx + 1]);
            }
        }
        if (param == "--test-section" && sectionToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                sectionToRun = StringView(argv[idx + 1], strlen(argv[idx + 1]), true, StringEncoding::Utf8);

                Console::print("TestReport::Running single section \"{}\"\n", argv[idx + 1]);
            }
        }
    }
    if (not testToRun.isEmpty() || not sectionToRun.isEmpty())
    {
        Console::print("\n");
    }
}

SC::TestReport::~TestReport()
{
    if (numTestsFailed > 0)
    {
        Console::print(redEMOJI);
        Console::print(" TOTAL Failed = {} (Succeeded = {})", numTestsFailed, numTestsSucceeded);
    }
    else
    {
        Console::print(greenEMOJI);
        Console::print(" TOTAL Succeeded = {}", numTestsSucceeded);
    }
    Console::print("\n---------------------------------------------------\n");
}

//-------------------------------------------------------------------------------------------
SC::TestCase::TestCase(TestReport& report, StringView testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0), numSectionTestsFailed(0),
      printedSection(false)
{
    if (report.isTestEnabled(testName))
    {
        Console::print("[[ {} ]]\n\n", testName);
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

        Console::print("\n");
        if (numTestsFailed > 0)
        {
            Console::print(redEMOJI);
            Console::print(" [[ ");
            Console::print(testName);
            Console::print(" ]]");
            Console::print(" FAILED = {} (Succeeded = {})\n", numTestsFailed, numTestsSucceeded);
        }
        else
        {
            Console::print(greenEMOJI);
            Console::print(" [[ ");
            Console::print(testName);
            Console::print(" ]]");
            Console::print(" SUCCEEDED = {}\n", numTestsSucceeded);
        }
        Console::print("---------------------------------------------------\n");
        report.numTestsFailed += numTestsFailed;
        report.numTestsSucceeded += numTestsSucceeded;
        report.testCaseFinished(*this);
    }
}

bool SC::TestCase::recordExpectation(StringView expression, bool status, StringView detailedError)
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
        Console::print("\t\t");
        Console::print(redEMOJI);
        if (detailedError.isEmpty())
        {
            Console::print(" [FAIL] {}\n", expression);
        }
        else
        {
            Console::print(" [FAIL] {} - Error: {}\n", expression, detailedError);
        }
        if (report.firstFailedTest.isEmpty())
        {
            report.firstFailedTest = expression;
        }
    }
    return status;
}

bool SC::TestCase::recordExpectation(StringView expression, ReturnCode status)
{
    return recordExpectation(expression, status, status.message);
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
    Console::print("\t- ");
    if (testCase.numSectionTestsFailed > 0)
    {
        Console::print(redEMOJI);
    }
    else
    {
        Console::print(greenEMOJI);
    }
    Console::print(" {}::{}\n", testCase.testName, currentSection);
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
