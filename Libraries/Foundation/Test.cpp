// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Test.h"
#include "Assert.h"
#include "Console.h"
#include <stdlib.h> // exit
namespace SC
{
static const StringView redEMOJI   = "\xf0\x9f\x9f\xa5"_u8;
static const StringView greenEMOJI = "\xf0\x9f\x9f\xa9"_u8;
} // namespace SC
SC::TestReport::TestReport(int argc, const char** argv)
{
    for (int idx = 1; idx < argc; ++idx)
    {
        const auto param = StringView(argv[idx], strlen(argv[idx]), true, StringEncoding::Ascii);
        if (param == "--test"_a8 && testToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                testToRun = StringView(argv[idx + 1], strlen(argv[idx + 1]), true, StringEncoding::Ascii);

                Console::print("TestReport::Running single test \"{}\"\n"_a8, testToRun);
            }
        }
        if (param == "--test-section"_a8 && sectionToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                sectionToRun = StringView(argv[idx + 1], strlen(argv[idx + 1]), true, StringEncoding::Ascii);

                Console::print("TestReport::Running single section \"{}\"\n"_a8, sectionToRun);
            }
        }
    }
    if (not testToRun.isEmpty() || not sectionToRun.isEmpty())
    {
        Console::print("\n"_a8);
    }
}

SC::TestReport::~TestReport()
{
    if (numTestsFailed > 0)
    {
        Console::print(redEMOJI);
        Console::print(" TOTAL Failed = {} (Succeeded = {})"_a8, numTestsFailed, numTestsSucceeded);
    }
    else
    {
        Console::print(greenEMOJI);
        Console::print(" TOTAL Succeeded = {}"_a8, numTestsSucceeded);
    }
    Console::print("\n---------------------------------------------------\n"_a8);
}

//-------------------------------------------------------------------------------------------
SC::TestCase::TestCase(TestReport& report, StringView testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0), numSectionTestsFailed(0),
      printedSection(false)
{
    if (report.isTestEnabled(testName))
    {
        Console::print("[[ {} ]]\n\n"_a8, testName);
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

        Console::print("\n"_a8);
        if (numTestsFailed > 0)
        {
            Console::print(redEMOJI);
            Console::print(" [[ "_a8);
            Console::print(testName);
            Console::print(" ]]"_a8);
            Console::print(" FAILED = {} (Succeeded = {})\n"_a8, numTestsFailed, numTestsSucceeded);
        }
        else
        {
            Console::print(greenEMOJI);
            Console::print(" [[ "_a8);
            Console::print(testName);
            Console::print(" ]]"_a8);
            Console::print(" SUCCEEDED = {}\n"_a8, numTestsSucceeded);
        }
        Console::print("---------------------------------------------------\n"_a8);
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
        Console::print("\t\t"_a8);
        Console::print(redEMOJI);
        if (detailedError.isEmpty())
        {
            Console::print(" [FAIL] {}\n"_a8, expression);
        }
        else
        {
            Console::print(" [FAIL] {} - Error: {}\n"_a8, expression, detailedError);
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
    Console::print("\t- "_a8);
    if (testCase.numSectionTestsFailed > 0)
    {
        Console::print(redEMOJI);
    }
    else
    {
        Console::print(greenEMOJI);
    }
    Console::print(" {}::{}\n"_a8, testCase.testName, currentSection);
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
