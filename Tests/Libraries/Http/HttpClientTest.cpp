// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpClient.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HttpClientTest;
}

struct SC::HttpClientTest : public SC::TestCase
{
    HttpClientTest(SC::TestReport& report) : TestCase(report, "HttpClientTest")
    {
        if (test_section("sample")) {}
    }
};

namespace SC
{
void runHttpClientTest(SC::TestReport& report) { HttpClientTest test(report); }
} // namespace SC
