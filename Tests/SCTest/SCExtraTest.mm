// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/Path.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Testing/Testing.h"

#import <XCTest/XCTest.h>

@interface sanecppXCTest : XCTestCase

@end

@implementation sanecppXCTest

- (void)setUp
{
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown
{
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testA
{
    using namespace SC;
    const char* argv[] = {"", "--test", "XCTest", "--test-section", "operators"};

    char buffer[1024];
    Console console(buffer);

    TestReport::Output<Console> trConsole = {console};

    TestReport report(trConsole, 5, argv);
    if (report.hasStartupFailure())
        return;
    report.debugBreakOnFailedTest = true;

    globalConsole = &console;

    {
        TestCase tc(report, "XCTest");
        (void)tc.test_section("operators");
        tc.recordExpectation("Test", false);
        (void)tc.test_section("operators yeah");
    }
    (void)report.getTestReturnCode();
}

int main(int argc, const char* argv[]);
- (void)testMain
{
    main(0, 0);
}

@end
