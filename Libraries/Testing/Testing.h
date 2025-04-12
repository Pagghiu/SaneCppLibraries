// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/Console.h" // SC_COMPILER_DEBUG_BREAK
#include "../Strings/StringView.h"

namespace SC
{
struct TestCase;
struct TestReport;
} // namespace SC

//! @defgroup group_testing Testing
//! @copybrief library_testing (see @ref library_testing for more details)

//! @addtogroup group_testing
//! @{

/// @brief Collects multiple TestCase and reports their results.
///
/// This is passed as argument to SC::TestCase derived classes, and contains handle to _globals_ like a console,
/// paths to the library and application root, path to executable etc.
struct SC::TestReport
{
    Console&   console;                  ///< The passed in console object where to print results
    StringView libraryRootDirectory;     ///< Path to sources directory for library
    StringView executableFile;           ///< Path to current executable
    StringView applicationRootDirectory; ///< Path to application (on macOS is different from executable path)

    // Options
    bool abortOnFirstFailedTest = true;  ///< If `true` will abort after first failed test
    bool debugBreakOnFailedTest = true;  ///< If `true` will issue a debugger halt when a test fails
    bool quietMode              = false; ///< If `true` will not print recaps at start or end of the test

    /// @brief Build from a console and executable arguments
    /// @param console A Console object where to print test results
    /// @param argc Number of command line arguments
    /// @param argv Command line arguments Arguments
    TestReport(Console& console, int argc, const char** argv);
    ~TestReport();

    /// @brief Gets return code for this process
    /// @return `-1` if tests are failed, `0` if tests are successful
    [[nodiscard]] int getTestReturnCode() const;

    /// @brief Runs a report for the Global Memory Allocator and prints its results
    void runGlobalMemoryReport(bool reportFailure = true);

  private:
    [[nodiscard]] bool isTestEnabled(StringView testName) const;
    [[nodiscard]] bool isSectionEnabled(StringView sectionName) const;

    void testCaseFinished(TestCase& testCase);
    void printSectionResult(TestCase& testCase);

    friend struct TestCase;
    uint32_t   numTestsSucceeded = 0;
    uint32_t   numTestsFailed    = 0;
    StringView currentSection;
    StringView firstFailedTest;
    StringView testToRun;
    StringView sectionToRun;
};

/// @brief A test case that can be split into multiple sections.
/// To create a test case derive from SC::TestCase and run tests in the constructor
///
/// Example:
/// @snippet Tests/Libraries/Strings/ConsoleTest.cpp testingSnippet
struct SC::TestCase
{
    /// @brief Adds this TestCase to a TestReport with a name
    /// @param report The current TestReport
    /// @param testName Name of this TestCase
    TestCase(TestReport& report, StringView testName);
    ~TestCase();

    /// @brief Records an expectation for a given expression
    /// @param expression Expression converted to string
    /// @param status The boolean expectation of a test
    /// @param detailedError A detailed error message
    /// @return `status`
    bool recordExpectation(StringView expression, bool status, StringView detailedError = StringView());

    /// @brief Records an expectation for a given expression
    /// @param expression Expression converted to string
    /// @param status A Result object, output from a test
    /// @return `false` if `status` Result is not valid
    bool recordExpectation(StringView expression, Result status);

    enum class Execute
    {
        Default,     ///< Test is executed if all tests are enabled or if this specific one matches --test-section
        OnlyExplicit ///< Test is executed only if explicitly requested with --test-section
    };

    /// @brief Starts a new test section
    /// @param sectionName The name of the section
    /// @param execution Execution criteria
    /// @return `true` if the test is enabled, `false` otherwise
    [[nodiscard]] bool test_section(StringView sectionName, Execute execution = Execute::Default);

    TestReport& report; ///< The TestReport object passed in the constructor
  private:
    friend struct TestReport;
    const StringView testName;

    uint32_t numTestsSucceeded;
    uint32_t numTestsFailed;
    uint32_t numSectionTestsFailed;
    bool     printedSection;
};

/// Records a test expectation (eventually aborting or breaking o n failed test)
#define SC_TEST_EXPECT(e)                                                                                              \
    recordExpectation(StringView(#e##_a8), (e))                                                                        \
        ? (void)0                                                                                                      \
        : (TestCase::report.debugBreakOnFailedTest ? SC_COMPILER_DEBUG_BREAK : (void)0)

//! @}
