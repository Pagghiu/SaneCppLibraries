// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../HttpClient.h"
#include "../../Socket/SocketDescriptor.h"
#include "../../Testing/Testing.h"

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
