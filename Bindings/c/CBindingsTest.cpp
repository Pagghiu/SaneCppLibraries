#include "../../Libraries/Testing/Testing.h"

// Includes
#include "sc_hashing/sc_hashing.h"

// Tests
extern "C" const char* sc_hashing_test();

namespace SC
{
struct CBindingsTest;
}

struct SC::CBindingsTest : public SC::TestCase
{
    CBindingsTest(SC::TestReport& report) : TestCase(report, "CBindingsTest")
    {
        using namespace SC;
        const char* res;
        if (test_section("Hashing"))
        {
            res = sc_hashing_test();
            recordExpectation(res ? StringView::fromNullTerminated(res, StringEncoding::Utf8) : "Hashing",
                              res == nullptr);
        }
    }
};
namespace SC
{
void runCBindingsTest(SC::TestReport& report) { CBindingsTest test(report); }
} // namespace SC
