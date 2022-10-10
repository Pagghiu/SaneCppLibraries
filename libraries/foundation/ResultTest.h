#pragma once
#include "Result.h"
#include "Test.h"
#include "Vector.h"

namespace SC
{
struct ResultTest;
}

struct SC::ResultTest : public SC::TestCase
{
    ResultTest(SC::TestReport& report) : TestCase(report, "ResultTest")
    {
        using namespace SC;
        if (test_section("normal"))
        {
            auto res   = getString(false);
            auto value = res.releaseValue();
            SC_TEST_EXPECT(!res.isError());
            StringView sv(value.data(), static_cast<uint32_t>(value.size() - 1), true);
            SC_TEST_EXPECT(sv == "CIAO!");
        }
        if (test_section("nested_succeed"))
        {
            int res = SC_MUST(nestedFail2(false));
            SC_TEST_EXPECT(res = 7);
        }
        if (test_section("nested_fail"))
        {
            auto res = nestedFail2(true);
            SC_TEST_EXPECT(res.isError());
            SC_TEST_EXPECT(res.getError().message == "Error: cannot do stuff");
        }
        if (test_section("error_multires"))
        {
            auto res = failMultipleReasons(1);
            if (res.isError())
            {
                switch (res.getError().errorCode)
                {
                case error_code_1: break;
                case error_code_2: break;
                }
            }
        }
    }
    enum MyEnum
    {
        error_code_1 = 1,
        error_code_2 = 2
    };

    struct customError : public Error
    {
        MyEnum errorCode;
        customError(StringView message, MyEnum errorCode = error_code_1) : Error(message), errorCode(errorCode) {}
    };

    Result<int, customError> failMultipleReasons(int reason)
    {
        switch (reason)
        {
        case 1: return customError("Fail 1");
        case 2: return customError("Fail 2", error_code_2);
        }
        return 12345;
    }

    Result<Vector<char_t>> getString(bool fail)
    {
        if (fail)
        {
            StringView sv = "-12";
            int32_t    value;
            SC_TRY_WRAP(sv.parseInt32(&value), "Parse Int failed");
            return Error("Error: cannot do stuff");
        }
        else
        {
            Vector<char_t> valueTest;
            SC_TRY_WRAP(valueTest.appendCopy("CIAO!", strlen("CIAO!") + 1), "Failed Append");
            return valueTest;
        }
    }

    Result<int> nestedFail1(bool fail)
    {
        int value = SC_TRY(nestedFail2(fail));
        return value + 1;
    }

    Result<int> nestedFail2(bool fail)
    {
        auto value = SC_TRY(getString(fail));
        return static_cast<int>(value.size());
    }
};
