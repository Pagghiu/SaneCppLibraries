// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Testing.h"
#include "../Foundation/Assert.h"
#include <stdlib.h> // exit
#include <string.h> // strlen

namespace SC
{
static const StringSpan redEMOJI   = StringSpan("\xf0\x9f\x9f\xa5", true, StringEncoding::Utf8);
static const StringSpan greenEMOJI = StringSpan("\xf0\x9f\x9f\xa9", true, StringEncoding::Utf8);

} // namespace SC
SC::TestReport::TestReport(IOutput& console, int argc, const char** argv) : console(console)
{
    for (int idx = 1; idx < argc; ++idx)
    {
        const auto param = StringSpan::fromNullTerminated(argv[idx], StringEncoding::Ascii);
        if (param == "--quiet")
        {
            quietMode = true;
        }
        if (param == "--test" && testToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                testToRun = StringSpan::fromNullTerminated(argv[idx + 1], StringEncoding::Ascii);
                if (not quietMode)
                {
                    console.print("TestReport::Running single test \"{}\"\n", testToRun);
                }
            }
        }
        if (param == "--test-section" && sectionToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                sectionToRun = StringSpan::fromNullTerminated(argv[idx + 1], StringEncoding::Ascii);

                if (not quietMode)
                {
                    console.print("TestReport::Running single section \"{}\"\n", sectionToRun);
                }
            }
        }
    }

    if (not quietMode and (not testToRun.isEmpty() or not sectionToRun.isEmpty()))
    {
        console.print("\n");
    }
}

SC::TestReport::~TestReport()
{
    if (quietMode)
        return;
    if (numTestsFailed > 0)
    {
        console.print(redEMOJI);
        console.print(" TOTAL Failed = {} (Succeeded = {})", numTestsFailed, numTestsSucceeded);
    }
    else
    {
        console.print(greenEMOJI);
        console.print(" TOTAL Succeeded = {}", numTestsSucceeded, numTestsSucceeded);
    }
    console.print("\n---------------------------------------------------\n");
}

void SC::TestReport::internalRunGlobalMemoryReport(MemoryStatistics stats, bool reportFailure)
{
    if (quietMode)
        return;
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
    console.print("---------------------------------------------------\n");
}

//-------------------------------------------------------------------------------------------
SC::TestCase::TestCase(TestReport& report, StringSpan testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0), numSectionTestsFailed(0),
      printedSection(false)
{
    if (report.isTestEnabled(testName))
    {
        if (not report.quietMode)
        {
            report.console.print("[[ {} ]]\n\n", testName);
        }
        report.firstFailedTest = StringSpan();
        report.currentSection  = StringSpan();
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
            report.console.print("\n");
            if (numTestsFailed > 0)
            {
                report.console.print(redEMOJI);
                report.console.print(" [[ ");
                report.console.print(testName);
                report.console.print(" ]]");
                report.console.print(" FAILED = {} (Succeeded = {})\n", numTestsFailed, numTestsSucceeded);
            }
            else
            {
                report.console.print(greenEMOJI);
                report.console.print(" [[ ");
                report.console.print(testName);
                report.console.print(" ]]");
                report.console.print(" SUCCEEDED = {}\n", numTestsSucceeded);
            }
            report.console.print("---------------------------------------------------\n");
        }
        report.numTestsFailed += numTestsFailed;
        report.numTestsSucceeded += numTestsSucceeded;
        report.testCaseFinished(*this);
    }
}

bool SC::TestCase::recordExpectation(StringSpan expression, bool status, StringSpan detailedError)
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
        report.console.print("\t\t");
        report.console.print(redEMOJI);
        if (detailedError.isEmpty())
        {
            report.console.print(" [FAIL] {}\n", expression);
        }
        else
        {
            report.console.print(" [FAIL] {} - Error: {}\n", expression, detailedError);
        }
        if (report.firstFailedTest.isEmpty())
        {
            report.firstFailedTest = expression;
        }
    }
    return status;
}

bool SC::TestCase::recordExpectation(StringSpan expression, Result status)
{
    return recordExpectation(
        expression, status,
        StringSpan({status.message, status.message ? ::strlen(status.message) : 0}, true, StringEncoding::Ascii));
}

bool SC::TestCase::test_section(StringSpan sectionName, Execute execution)
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
        report.currentSection = StringSpan();
        return false;
    }
}

void SC::TestReport::printSectionResult(TestCase& testCase)
{
    if (quietMode)
    {
        return;
    }
    console.print("\t- ");
    console.print(testCase.numSectionTestsFailed > 0 ? redEMOJI : greenEMOJI);
    console.print(" {}::{}\n", testCase.testName, currentSection);
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

bool SC::TestReport::isTestEnabled(StringSpan testName) const { return testToRun.isEmpty() || testToRun == testName; }

bool SC::TestReport::isSectionEnabled(StringSpan sectionName) const
{
    return sectionToRun.isEmpty() || sectionName == sectionToRun;
}

int SC::TestReport::getTestReturnCode() const { return numTestsFailed > 0 ? -1 : 0; }
