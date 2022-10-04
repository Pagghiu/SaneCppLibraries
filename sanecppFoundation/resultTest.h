#pragma once
#include "result.h"
#include "test.h"
#include "vector.h"

namespace sanecpp
{
struct ResultTest;
}

struct sanecpp::ResultTest : public sanecpp::TestCase
{
    ResultTest(sanecpp::TestReport& report) : TestCase(report, "ResultTest")
    {
        using namespace sanecpp;
        if (START_SECTION("normal"))
        {
            auto res   = getString(false);
            auto value = res.releaseValue();
            SANECPP_TEST_EXPECT(!res.isError());
            stringView sv(value.data(), static_cast<uint32_t>(value.size() - 1), true);
            SANECPP_TEST_EXPECT(sv == "CIAO!");
        }
        if (START_SECTION("nested_succeed"))
        {
            int res = SANECPP_MUST(nestedFail2(false));
            SANECPP_TEST_EXPECT(res = 7);
        }
        if (START_SECTION("nested_fail"))
        {
            auto res = nestedFail2(true);
            SANECPP_TEST_EXPECT(res.isError());
            SANECPP_TEST_EXPECT(res.getError().message == "Error: cannot do stuff");
        }
        if (START_SECTION("error_multires"))
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

    struct customError : public error
    {
        MyEnum errorCode;
        customError(stringView message, MyEnum errorCode = error_code_1) : error(message), errorCode(errorCode) {}
    };

    result<int, customError> failMultipleReasons(int reason)
    {
        switch (reason)
        {
        case 1: return customError("Fail 1");
        case 2: return customError("Fail 2", error_code_2);
        }
        return 12345;
    }

    result<vector<char_t>> getString(bool fail)
    {
        if (fail)
        {
            stringView sv = "-12";
            int32_t    value;
            SANECPP_TRY_WRAP(sv.parseInt32(&value), "Parse Int failed");
            return error("Error: cannot do stuff");
        }
        else
        {
            vector<char_t> valueTest;
            SANECPP_TRY_WRAP(valueTest.appendCopy("CIAO!", strlen("CIAO!") + 1), "Failed Append");
            return valueTest;
        }
    }

    result<int> nestedFail1(bool fail)
    {
        int value = SANECPP_TRY(nestedFail2(fail));
        return value + 1;
    }

    result<int> nestedFail2(bool fail)
    {
        auto value = SANECPP_TRY(getString(fail));
        return static_cast<int>(value.size());
    }
};
