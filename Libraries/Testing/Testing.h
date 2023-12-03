// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"
#include "../System/Console.h" // SC_COMPILER_DEBUG_BREAK

namespace SC
{
struct TestCase;
struct TestReport;
} // namespace SC

//! @defgroup group_testing Testing
//! @copybrief library_testing (see @ref library_testing for more details)

//! @addtogroup group_testing
//! @{

/// @brief Collects multiple SC::TestCase and reports their results
struct SC::TestReport
{
    Console&   console;                       ///< The passed in console object where to print results
    bool       abortOnFirstFailedTest = true; ///< If `true` will abort after first failed test
    bool       debugBreakOnFailedTest = true; ///< If `true` will issue a debugger halt when a test fails
    StringView libraryRootDirectory;          ///< Path to SC Library
    StringView executableFile;                ///< Path to current executable
    StringView applicationRootDirectory;      ///< Path to application (on macOS is different from executable path)

    /// @brief Build from a console and executalbe arguments
    /// @param console A Console object where to print test results
    /// @param argc Number of command line arguments
    /// @param argv Command line arguments Arguments
    TestReport(Console& console, int argc, const char** argv);
    ~TestReport();

    /// @brief Gets return code for this process
    /// @return `-1` if tests are failed, `0` if tests are successfull
    [[nodiscard]] int getTestReturnCode() const;

  private:
    [[nodiscard]] bool isTestEnabled(StringView testName) const;
    [[nodiscard]] bool isSectionEnabled(StringView sectionName) const;
    void               testCaseFinished(TestCase& testCase);
    void               printSectionResult(TestCase& testCase);

    friend struct TestCase;
    uint32_t   numTestsSucceeded = 0;
    uint32_t   numTestsFailed    = 0;
    StringView currentSection;
    StringView firstFailedTest;
    StringView testToRun;
    StringView sectionToRun;
};

/// @brief A Test case, with multiple section
struct SC::TestCase
{
    /// @brief Adds this TestCase to a TestReport with a name
    /// @param report The parten TestReport
    /// @param testName Name of this TestCase
    TestCase(TestReport& report, StringView testName);
    ~TestCase();

    /// @brief Records an expectation for a given expression
    /// @param expression The string-ized expression of this test
    /// @param status The boolean expectation of a test
    /// @param detailedError A detailed error message
    /// @return `status`
    bool recordExpectation(StringView expression, bool status, StringView detailedError = StringView());

    /// @brief Records an expectation for a given expression
    /// @param expression The string-ized expression of this test
    /// @param status A Result object, output from a test
    /// @return `false` if `status` Result is not valid
    bool recordExpectation(StringView expression, Result status);

    /// @brief Starts a new test section
    /// @param sectionName The name of the section
    /// @return `true` if the test is enabled, `false` otherwise
    [[nodiscard]] bool test_section(StringView sectionName);

    TestReport& report; ///< The TestReport object passed in the constructor
  private:
    friend struct TestReport;
    const StringView testName;
    uint32_t         numTestsSucceeded;
    uint32_t         numTestsFailed;
    uint32_t         numSectionTestsFailed;
    bool             printedSection;
};

/// Records a test expectation (eventually aborting or breaking o n failed test)
#define SC_TEST_EXPECT(e)                                                                                              \
    recordExpectation(StringView(#e##_a8), (e))                                                                        \
        ? (void)0                                                                                                      \
        : (TestCase::report.debugBreakOnFailedTest ? SC_COMPILER_DEBUG_BREAK : (void)0)

//! @}
