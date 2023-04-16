// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Libraries/Testing/Test.h"
#import <XCTest/XCTest.h>
int        main(int argc, const char* argv[]);
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

extern "C" int  __cxa_guard_acquire(uint64_t* guard_object);
extern "C" void __cxa_guard_release(uint64_t* guard_object);
- (void)testA
{
    using namespace SC;
    const char*    argv[] = {"", "--test", "XCTest", "--test-section", "operators"};
    SC::TestReport report(5, argv);
    {
        TestCase tc(report, "XCTest");
        (void)tc.test_section("operators");
        tc.recordExpectation("Test", false);
        (void)tc.test_section("operators sadf");
    }
    (void)report.getTestReturnCode();
    __cxa_guard_acquire(nullptr);
    __cxa_guard_release(nullptr);
}

- (void)testMain
{
    main(0, 0);
}

@end
