// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Testing.h"
#include "../Foundation/Assert.h"
#include "../Foundation/Globals.h"
#include "../Strings/Console.h"
#include <stdlib.h> // exit
#include <string.h> // strlen

namespace SC
{
static const StringView redEMOJI   = "\xf0\x9f\x9f\xa5"_u8;
static const StringView greenEMOJI = "\xf0\x9f\x9f\xa9"_u8;
} // namespace SC
SC::TestReport::TestReport(Console& console, int argc, const char** argv) : console(console)
{
    for (int idx = 1; idx < argc; ++idx)
    {
        const auto param = StringView::fromNullTerminated(argv[idx], StringEncoding::Ascii);
        if (param == "--quiet"_a8)
        {
            quietMode = true;
        }
        if (param == "--test"_a8 && testToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                testToRun = StringView::fromNullTerminated(argv[idx + 1], StringEncoding::Ascii);
                if (not quietMode)
                {
                    console.print("TestReport::Running single test \"{}\"\n"_a8, testToRun);
                }
            }
        }
        if (param == "--test-section"_a8 && sectionToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                sectionToRun = StringView::fromNullTerminated(argv[idx + 1], StringEncoding::Ascii);

                if (not quietMode)
                {
                    console.print("TestReport::Running single section \"{}\"\n"_a8, sectionToRun);
                }
            }
        }
    }
    if (not testToRun.isEmpty() || not sectionToRun.isEmpty())
    {
        console.print("\n"_a8);
    }
}

SC::TestReport::~TestReport()
{
    if (quietMode)
        return;
    if (numTestsFailed > 0)
    {
        console.print(redEMOJI);
        console.print(" TOTAL Failed = {} (Succeeded = {})"_a8, numTestsFailed, numTestsSucceeded);
    }
    else
    {
        console.print(greenEMOJI);
        console.print(" TOTAL Succeeded = {}"_a8, numTestsSucceeded);
    }
    console.print("\n---------------------------------------------------\n"_a8);
}

void SC::TestReport::runGlobalMemoryReport(bool reportFailure)
{
    MemoryAllocator::Statistics stats = Globals::getGlobal().allocator.statistics;
    console.print("[[ Memory Report ]]\n");
    console.print("\t - Allocations   = {}\n", stats.numAllocate);
    console.print("\t - Deallocations = {}\n", stats.numRelease);
    console.print("\t - Reallocations = {}\n", stats.numReallocate);
    if (reportFailure)
    {
        if (stats.numAllocate == stats.numRelease)
        {
            console.print(greenEMOJI);
            console.print(" [[ Memory Report ]] SUCCEDED = 1\n");
        }
        else
        {
            console.print(redEMOJI);
            console.print(" [[ Memory Report ]] FAILED = 1\n");
            numTestsFailed++;
        }
    }
    console.print("---------------------------------------------------\n"_a8);
}

//-------------------------------------------------------------------------------------------
SC::TestCase::TestCase(TestReport& report, StringView testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0), numSectionTestsFailed(0),
      printedSection(false)
{
    if (report.isTestEnabled(testName))
    {
        if (not report.quietMode)
        {
            report.console.print("[[ {} ]]\n\n"_a8, testName);
        }
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
        if (not report.quietMode)
        {
            report.console.print("\n"_a8);
            if (numTestsFailed > 0)
            {
                report.console.print(redEMOJI);
                report.console.print(" [[ "_a8);
                report.console.print(testName);
                report.console.print(" ]]"_a8);
                report.console.print(" FAILED = {} (Succeeded = {})\n"_a8, numTestsFailed, numTestsSucceeded);
            }
            else
            {
                report.console.print(greenEMOJI);
                report.console.print(" [[ "_a8);
                report.console.print(testName);
                report.console.print(" ]]"_a8);
                report.console.print(" SUCCEEDED = {}\n"_a8, numTestsSucceeded);
            }
            report.console.print("---------------------------------------------------\n"_a8);
        }
        report.numTestsFailed += numTestsFailed;
        report.numTestsSucceeded += numTestsSucceeded;
        report.testCaseFinished(*this);
    }
}

bool SC::TestCase::recordExpectation(StringView expression, bool status, StringView detailedError)
{
    SC_ASSERT_DEBUG(expression.isNullTerminated());
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
        report.console.print("\t\t"_a8);
        report.console.print(redEMOJI);
        if (detailedError.isEmpty())
        {
            report.console.print(" [FAIL] {}\n"_a8, expression);
        }
        else
        {
            report.console.print(" [FAIL] {} - Error: {}\n"_u8, expression, detailedError);
        }
        if (report.firstFailedTest.isEmpty())
        {
            report.firstFailedTest = expression;
        }
    }
    return status;
}

bool SC::TestCase::recordExpectation(StringView expression, Result status)
{
    return recordExpectation(
        expression, status,
        StringView({status.message, status.message ? ::strlen(status.message) : 0}, true, StringEncoding::Ascii));
}

bool SC::TestCase::test_section(StringView sectionName, Execute execution)
{
    numSectionTestsFailed = 0;
    bool isTestEnabled;
    switch (execution)
    {
    case Execute::Default: //
        isTestEnabled = report.isTestEnabled(testName) and report.isSectionEnabled(sectionName);
        break;
    case Execute::OnlyExplicit: //
        isTestEnabled = report.sectionToRun == sectionName;
        break;
    default: return false;
    }
    if (isTestEnabled)
    {
        SC_ASSERT_DEBUG(sectionName.isNullTerminated());
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
    console.print("\t- "_a8);
    if (testCase.numSectionTestsFailed > 0)
    {
        console.print(redEMOJI);
    }
    else
    {
        console.print(greenEMOJI);
    }
    console.print(" {}::{}\n"_a8, testCase.testName, currentSection);
}

void SC::TestReport::testCaseFinished(TestCase& testCase)
{
    if (abortOnFirstFailedTest && testCase.numTestsFailed > 0)
    {
#if SC_CONFIGURATION_RELEASE
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
