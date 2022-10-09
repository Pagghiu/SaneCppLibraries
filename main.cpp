#include "libraries/foundation/ArrayTest.h"
#include "libraries/foundation/ConsoleTest.h"
#include "libraries/foundation/MemoryTest.h"
#include "libraries/foundation/OSTest.h"
#include "libraries/foundation/ResultTest.h"
#include "libraries/foundation/StringViewTest.h"
#include "libraries/foundation/VectorTest.h"

int main(int argc, const char* argv[])
{
    SC::TestReport report(argc, argv);
    {
        SC::ArrayTest test(report);
    }
    {
        SC::VectorTest test(report);
    }
    {
        SC::ResultTest test(report);
    }
    {
        SC::StringViewTest test(report);
    }
    {
        SC::OSTest test(report);
    }
    {
        SC::ConsoleTest test(report);
    }
    {
        SC::MemoryTest test(report);
    }
    return report.getTestReturnCode();
}
