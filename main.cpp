#include "sanecppFoundation/arrayTest.h"
#include "sanecppFoundation/consoleTest.h"
#include "sanecppFoundation/memoryTest.h"
#include "sanecppFoundation/osTest.h"
#include "sanecppFoundation/resultTest.h"
#include "sanecppFoundation/stringViewTest.h"
#include "sanecppFoundation/vectorTest.h"

int main(int argc, const char* argv[])
{
    sanecpp::TestReport report(argc, argv);
    {
        sanecpp::ArrayTest test(report);
    }
    {
        sanecpp::VectorTest test(report);
    }
    {
        sanecpp::ResultTest test(report);
    }
    {
        sanecpp::StringViewTest test(report);
    }
    {
        sanecpp::OsTest test(report);
    }
    {
        sanecpp::ConsoleTest test(report);
    }
    {
        sanecpp::MemoryTest test(report);
    }
    return report.getTestReturnCode();
}
