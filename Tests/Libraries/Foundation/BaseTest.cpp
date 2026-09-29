// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Common/Assert.h"
#include "Libraries/Common/TypeTraits.h"
#include "Libraries/Memory/Memory.h"
#include "Libraries/Testing/Limits.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct BaseTest;

struct ResultTestDomainA
{
    enum class Error : uint32_t
    {
        InvalidValue = 1
    };

    Result   status;
    uint32_t detail = 0;

    static constexpr ResultCategory category() { return ResultCategory(0xa0000001u); }

    constexpr ResultTestDomainA() : status(true) {}
    constexpr ResultTestDomainA(Error error, uint32_t detail = 0)
        : status(Result::Error(category(), static_cast<uint32_t>(error))), detail(detail)
    {}
    constexpr ResultTestDomainA(Result status) : status(status) {}

    template <typename Other>
    constexpr ResultTestDomainA(const Other& other) : status(other.toResult())
    {}

    constexpr        operator bool() const { return status; }
    constexpr        operator Result() const { return status; }
    constexpr Result toResult() const { return status; }
    constexpr bool   is(Error error) const { return status.isError(category(), static_cast<uint32_t>(error)); }
};

struct ResultTestDomainB
{
    enum class Error : uint32_t
    {
        NotReady = 1
    };

    Result   status;
    uint32_t detail = 0;

    static constexpr ResultCategory category() { return ResultCategory(0xb0000001u); }

    constexpr ResultTestDomainB() : status(true) {}
    constexpr ResultTestDomainB(Error error, uint32_t detail = 0)
        : status(Result::Error(category(), static_cast<uint32_t>(error))), detail(detail)
    {}
    constexpr ResultTestDomainB(Result status) : status(status) {}

    template <typename Other>
    constexpr ResultTestDomainB(const Other& other) : status(other.toResult())
    {}

    constexpr        operator bool() const { return status; }
    constexpr        operator Result() const { return status; }
    constexpr Result toResult() const { return status; }
};

struct ResultExplicitCopyProbe
{
    bool*  moved;
    Result status;

    ResultExplicitCopyProbe(bool& moved, Result status) : moved(&moved), status(status) {}
    ResultExplicitCopyProbe(const ResultExplicitCopyProbe& other) : moved(other.moved), status(other.status) {}
    ResultExplicitCopyProbe(ResultExplicitCopyProbe&& other) : moved(other.moved), status(other.status)
    {
        *other.moved = true;
    }

    operator bool() const { return status; }
    Result toResult() const { return status; }
};
} // namespace SC

struct SC::BaseTest : public SC::TestCase
{
    static ResultTestDomainA propagateSameDomain(ResultTestDomainA result)
    {
        SC_TRY(result)
        return ResultTestDomainA();
    }

    static ResultTestDomainA propagatePlain(Result result)
    {
        SC_TRY(result)
        return ResultTestDomainA();
    }

    static ResultTestDomainA propagateForeignDomain(ResultTestDomainB result)
    {
        SC_TRY(result)
        return ResultTestDomainA();
    }

    static Result propagateToPlain(ResultTestDomainA result)
    {
        SC_TRY(result)
        return Result(true);
    }

    static Result countedFailure(int& evaluations)
    {
        evaluations += 1;
        return Result::Error(ResultTestDomainA::category(),
                             static_cast<uint32_t>(ResultTestDomainA::Error::InvalidValue));
    }

    static Result evaluateOnce(int& evaluations)
    {
        SC_TRY(countedFailure(evaluations))
        return Result(true);
    }

    BaseTest(SC::TestReport& report) : TestCase(report, "BaseTest")
    {
        if (test_section("new/delete"))
        {
            int* a = new int(2);
            SC_TEST_EXPECT(a[0] == 2);
            delete a;
            int* b = new int[2];
            delete[] b;
        }
        if (test_section("Assert::printBacktrace"))
        {
            Assert::printBacktrace("a!=b",
                                   Result::Error(ResultTestDomainA::category(), ResultTestDomainA::Error::InvalidValue),
                                   SC_NATIVE_STR("FileName.cpp"), "Function", 12);
        }

        if (test_section("Result structured identity"))
        {
            static_assert(sizeof(Result) == 8, "Result must remain eight bytes");
            static_assert(alignof(Result) == alignof(uint64_t), "Result must retain numeric identity alignment");
            static_assert(__is_standard_layout(Result), "Result must remain standard-layout");
            static_assert(TypeTraits::IsTriviallyCopyable<Result>::value, "Result must remain trivially copyable");
            static_assert(sizeof(ResultCategory) == sizeof(uint32_t), "ResultCategory must remain 32-bit");

            const Result success(true);
            SC_TEST_EXPECT(success);
            SC_TEST_EXPECT(success.category() == ResultCategory(Result::UncategorizedValue));
            SC_TEST_EXPECT(success.errorValue() == 0);

            const Result unspecified(false);
            SC_TEST_EXPECT(not unspecified);
            SC_TEST_EXPECT(unspecified.category() == ResultCategory(Result::UncategorizedValue));
            SC_TEST_EXPECT(unspecified.errorValue() == Result::UnspecifiedErrorValue);

            const Result structured = Result::Error(ResultTestDomainA::category(), 42);
            SC_TEST_EXPECT(not structured);
            SC_TEST_EXPECT(structured.category() == ResultTestDomainA::category());
            SC_TEST_EXPECT(structured.errorValue() == 42);
            SC_TEST_EXPECT(structured.isError(ResultTestDomainA::category(), 42));
            SC_TEST_EXPECT(not structured.isError(ResultTestDomainA::category(), 0));

            const Result malformed = Result::Error(ResultTestDomainA::category(), 0);
            SC_TEST_EXPECT(not malformed);
            SC_TEST_EXPECT(malformed.category() == ResultCategory(Result::UncategorizedValue));
            SC_TEST_EXPECT(malformed.errorValue() == Result::UnspecifiedErrorValue);
        }

        if (test_section("Result enriched propagation"))
        {
            const ResultTestDomainA localFailure(ResultTestDomainA::Error::InvalidValue, 73);
            const ResultTestDomainA sameDomain = propagateSameDomain(localFailure);
            SC_TEST_EXPECT(not sameDomain);
            SC_TEST_EXPECT(sameDomain.is(ResultTestDomainA::Error::InvalidValue));
            SC_TEST_EXPECT(sameDomain.detail == 73);

            const ResultTestDomainA plainFailure = propagatePlain(Result::Error(ResultTestDomainA::category(), 2));
            SC_TEST_EXPECT(not plainFailure);
            SC_TEST_EXPECT(plainFailure.status.isError(ResultTestDomainA::category(), 2));
            SC_TEST_EXPECT(plainFailure.detail == 0);

            const ResultTestDomainB foreignInput(ResultTestDomainB::Error::NotReady, 91);
            const ResultTestDomainA foreignFailure = propagateForeignDomain(foreignInput);
            SC_TEST_EXPECT(not foreignFailure);
            SC_TEST_EXPECT(foreignFailure.status.isError(ResultTestDomainB::category(), 1));
            SC_TEST_EXPECT(not foreignFailure.is(ResultTestDomainA::Error::InvalidValue));
            SC_TEST_EXPECT(foreignFailure.detail == 0);

            const Result plainFromEnriched = propagateToPlain(localFailure);
            SC_TEST_EXPECT(not plainFromEnriched);
            SC_TEST_EXPECT(plainFromEnriched.isError(ResultTestDomainA::category(), 1));

            int evaluations = 0;
            SC_TEST_EXPECT(not evaluateOnce(evaluations));
            SC_TEST_EXPECT(evaluations == 1);

            bool                    moved = false;
            ResultExplicitCopyProbe probe(moved, Result(true));
            const auto              copied = Result::Explicit(probe);
            (void)copied;
            SC_TEST_EXPECT(not moved);
        }

        if (test_section("Limits (coverage)"))
        {
            uint8_t  maxU8  = MaxValue();
            uint16_t maxU16 = MaxValue();
            uint32_t maxU32 = MaxValue();
            uint64_t maxU64 = MaxValue();
            int8_t   maxI8  = MaxValue();
            int16_t  maxI16 = MaxValue();
            int32_t  maxI32 = MaxValue();
            int64_t  maxI64 = MaxValue();
            ssize_t  maxSS  = MaxValue();
            size_t   maxS   = MaxValue();
            float    maxF   = MaxValue();
            double   maxD   = MaxValue();

            (void)(maxU8);
            (void)(maxU16);
            (void)(maxU32);
            (void)(maxU64);
            (void)(maxI8);
            (void)(maxI16);
            (void)(maxI32);
            (void)(maxI64);
            (void)(maxSS);
            (void)(maxS);
            (void)(maxF);
            (void)(maxD);
        }
    }
};

namespace SC
{
void runBaseTest(SC::TestReport& report) { BaseTest test(report); }
} // namespace SC
