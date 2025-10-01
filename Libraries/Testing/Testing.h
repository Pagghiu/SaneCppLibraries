// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/StringPath.h"
#include "../Foundation/StringSpan.h"

namespace SC
{
struct TestCase;
//! @defgroup group_testing Testing
//! @copybrief library_testing (see @ref library_testing for more details)

//! @addtogroup group_testing
//! @{

/// @brief Collects multiple TestCase and reports their results.
///
/// This is passed as argument to SC::TestCase derived classes, and contains handle to _globals_ like a console,
/// paths to the library and application root, path to executable etc.
struct TestReport
{
    struct IOutput
    {
        virtual ~IOutput() = default;

        virtual void printLine(StringSpan text)                           = 0;
        virtual void print(StringSpan text)                               = 0;
        virtual void print(StringSpan text, StringSpan p0)                = 0;
        virtual void print(StringSpan text, StringSpan p0, StringSpan p1) = 0;
        virtual void print(StringSpan text, size_t p0)                    = 0;
        virtual void print(StringSpan text, size_t p0, size_t p1)         = 0;
    };

    template <typename ConsoleType>
    struct Output final : public TestReport::IOutput
    {
        ConsoleType& console;
        Output(ConsoleType& console) : console(console) {}

        virtual void printLine(StringSpan text) override { console.printLine(text); }
        virtual void print(StringSpan text) override { console.print(text); }
        virtual void print(StringSpan text, StringSpan p0) override { console.print(text, p0); }
        virtual void print(StringSpan text, StringSpan p0, StringSpan p1) override { console.print(text, p0, p1); }
        virtual void print(StringSpan text, size_t p0) override { console.print(text, p0); }
        virtual void print(StringSpan text, size_t p0, size_t p1) override { console.print(text, p0, p1); }
    };
    IOutput& console;

    StringPath libraryRootDirectory;     ///< Path to sources directory for library
    StringPath executableFile;           ///< Path to current executable
    StringPath applicationRootDirectory; ///< Path to application (on macOS is different from executable path)

    // Options
    bool abortOnFirstFailedTest = true;  ///< If `true` will abort after first failed test
    bool debugBreakOnFailedTest = true;  ///< If `true` will issue a debugger halt when a test fails
    bool quietMode              = false; ///< If `true` will not print recaps at start or end of the test

    /// @brief Build from a console and executable arguments
    /// @param console A Console object where to print test results
    /// @param argc Number of command line arguments
    /// @param argv Command line arguments Arguments
    TestReport(IOutput& console, int argc, const char** argv);
    ~TestReport();

    /// @brief Gets return code for this process
    /// @return `-1` if tests are failed, `0` if tests are successful
    [[nodiscard]] int getTestReturnCode() const;

    template <typename Statistics>
    void runGlobalMemoryReport(Statistics stats, bool reportFailure = true)
    {
        TestReport::MemoryStatistics memStats;
        memStats.numAllocate   = stats.numAllocate;
        memStats.numReallocate = stats.numReallocate;
        memStats.numRelease    = stats.numRelease;
        internalRunGlobalMemoryReport(memStats, reportFailure);
    }

  private:
    struct MemoryStatistics
    {
        size_t numAllocate   = 0; ///< How many times MemoryAllocator::allocate has been called
        size_t numReallocate = 0; ///< How many times MemoryAllocator::reallocate has been called
        size_t numRelease    = 0; ///< How many times MemoryAllocator::release has been called
    };

    /// @brief Runs a report for the Global Memory Allocator and prints its results
    void internalRunGlobalMemoryReport(MemoryStatistics stats, bool reportFailure);

    [[nodiscard]] bool isTestEnabled(StringSpan testName) const;
    [[nodiscard]] bool isSectionEnabled(StringSpan sectionName) const;

    void testCaseFinished(TestCase& testCase);
    void printSectionResult(TestCase& testCase);

    friend struct TestCase;
    uint32_t   numTestsSucceeded = 0;
    uint32_t   numTestsFailed    = 0;
    StringSpan currentSection;
    StringSpan firstFailedTest;
    StringSpan testToRun;
    StringSpan sectionToRun;
};

/// @brief A test case that can be split into multiple sections.
/// To create a test case derive from SC::TestCase and run tests in the constructor
///
/// Example:
/// @snippet Tests/Libraries/Strings/ConsoleTest.cpp testingSnippet
struct TestCase
{
    /// @brief Adds this TestCase to a TestReport with a name
    /// @param report The current TestReport
    /// @param testName Name of this TestCase
    TestCase(TestReport& report, StringSpan testName);
    ~TestCase();

    /// @brief Records an expectation for a given expression
    /// @param expression Expression converted to string
    /// @param status The boolean expectation of a test
    /// @param detailedError A detailed error message
    /// @return `status`
    bool recordExpectation(StringSpan expression, bool status, StringSpan detailedError = StringSpan());

    /// @brief Records an expectation for a given expression
    /// @param expression Expression converted to string
    /// @param status A Result object, output from a test
    /// @return `false` if `status` Result is not valid
    bool recordExpectation(StringSpan expression, Result status);

    enum class Execute
    {
        Default,     ///< Test is executed if all tests are enabled or if this specific one matches --test-section
        OnlyExplicit ///< Test is executed only if explicitly requested with --test-section
    };

    /// @brief Starts a new test section
    /// @param sectionName The name of the section
    /// @param execution Execution criteria
    /// @return `true` if the test is enabled, `false` otherwise
    [[nodiscard]] bool test_section(StringSpan sectionName, Execute execution = Execute::Default);

    TestReport& report; ///< The TestReport object passed in the constructor
  private:
    friend struct TestReport;
    const StringSpan testName;

    uint32_t numTestsSucceeded;
    uint32_t numTestsFailed;
    uint32_t numSectionTestsFailed;
    bool     printedSection;
};

// clang-format off
/// Records a test expectation (eventually aborting or breaking o n failed test)
#define SC_TEST_EXPECT(e) recordExpectation(StringSpan(#e), (e)) ? (void)0  : (TestCase::report.debugBreakOnFailedTest ? SC_COMPILER_DEBUG_BREAK : (void)0)
// clang-format on

//! @}
} // namespace SC
