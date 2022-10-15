#include "libraries/foundation/ArrayTest.h"
#include "libraries/foundation/ConsoleTest.h"
#include "libraries/foundation/MapTest.h"
#include "libraries/foundation/MemoryTest.h"
#include "libraries/foundation/OSTest.h"
#include "libraries/foundation/ResultTest.h"
#include "libraries/foundation/StringBuilderTest.h"
#include "libraries/foundation/StringFunctionsTest.h"
#include "libraries/foundation/StringTest.h"
#include "libraries/foundation/StringViewTest.h"
#include "libraries/foundation/VectorTest.h"

int main(int argc, const char* argv[])
{
    using namespace SC;
    TestReport report(argc, argv);
    report.debugBreakOnFailedTest = false;
    // clang-format off
    { OSTest                test(report); }
    { ConsoleTest           test(report); }
    { MemoryTest            test(report); }
    { ArrayTest             test(report); }
    { VectorTest            test(report); }
    { ResultTest            test(report); }
    { StringViewTest        test(report); }
    { StringFunctionsTest   test(report); }
    { StringTest            test(report); }
    { StringBuilderTest     test(report); }
    { MapTest               test(report); }
    // clang-format on

    return report.getTestReturnCode();
}
