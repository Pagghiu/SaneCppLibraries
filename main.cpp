#include "libraries/foundation/ArrayTest.h"
#include "libraries/foundation/ConsoleTest.h"
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
    // clang-format off
    { ArrayTest             test(report); }
    { VectorTest            test(report); }
    { ResultTest            test(report); }
    { StringViewTest        test(report); }
    { StringFunctionsTest   test(report); }
    { StringTest            test(report); }
    { StringBuilderTest     test(report); }
    { OSTest                test(report); }
    { ConsoleTest           test(report); }
    { MemoryTest            test(report); }
    // clang-format on

    return report.getTestReturnCode();
}
