// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Libraries/FileSystem/Path.h"
#include "../../Libraries/Foundation/Containers/SmallVector.h"
#include "../../Libraries/System/System.h"
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
    const char* argv[] = {"", "--test", "XCTest", "--test-section", "operators"};
    SC::SmallVector<char, 1024 * sizeof(SC::native_char_t)> globalConsoleConversionBuffer;
    SystemDirectories                                       directories;
    if (not directories.init())
        return;
    SystemFunctions functions;
    if (not functions.initNetworking())
        return;
    Console              console(globalConsoleConversionBuffer);
    SC::SmallString<255> correctedPath;
    TestReport           report(console, 5, argv);
    report.applicationRootDirectory = directories.applicationRootDirectory.view();
    report.executableFile           = directories.executableFile.view();
    {
        SmallVector<StringView, 50> components;
        (void)Path::normalizeUNCAndTrimQuotes(SC_COMPILER_LIBRARY_PATH, components, correctedPath, Path::AsNative);
        // If you hit this assertion you must figure out a way to derive location of Libraries
        SC_ASSERT_RELEASE(Path::isAbsolute(correctedPath.view(), SC::Path::AsNative));
    }
    report.libraryRootDirectory   = correctedPath.view();
    report.debugBreakOnFailedTest = true;

    globalConsole = &console;

    {
        TestCase tc(report, "XCTest");
        (void)tc.test_section("operators");
        tc.recordExpectation("Test", false);
        (void)tc.test_section("operators sadf");
    }
    (void)report.getTestReturnCode();
    //    __cxa_guard_acquire(nullptr);
    //    __cxa_guard_release(nullptr);
}

- (void)testMain
{
    main(0, 0);
}

@end
