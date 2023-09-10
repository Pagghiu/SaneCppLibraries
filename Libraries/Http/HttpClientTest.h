// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Socket/SocketDescriptor.h"
#include "../Testing/Test.h"
#include "HttpClient.h"

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
