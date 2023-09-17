// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "../Containers/Vector.h"
#include "../Objects/Result.h"

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
            StringView sv(value.data(), static_cast<uint32_t>(value.size() - 1), true, StringEncoding::Ascii);
            SC_TEST_EXPECT(sv == "CIAO!");
        }
        if (test_section("nested_succeed"))
        {
            SC_MUST(int res, nestedFail2(false));
            SC_TEST_EXPECT(res == 6);
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

    struct customError : public ReturnCode
    {
        MyEnum errorCode;
        customError(StringView message, MyEnum errorCode = error_code_1) : ReturnCode(message), errorCode(errorCode) {}
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

    Result<Vector<char>> getString(bool fail)
    {
        if (fail)
        {
            StringView sv = "-12";
            int32_t    value;
            SC_TRY_MSG(sv.parseInt32(value), "Parse Int failed"_a8);
            return ReturnCode("Error: cannot do stuff"_a8);
        }
        else
        {
            Vector<char> valueTest;
            SC_TRY_MSG(valueTest.appendCopy("CIAO!", 6), "Failed Append"_a8);
            return valueTest;
        }
    }

    Result<int> nestedFail1(bool fail)
    {
        SC_TRY(int value, nestedFail2(fail));
        return value + 1;
    }

    Result<int> nestedFail2(bool fail)
    {
        SC_TRY(auto value, getString(fail));
        return static_cast<int>(value.size());
    }
};
