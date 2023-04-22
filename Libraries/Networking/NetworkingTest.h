// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "Networking.h"

namespace SC
{
struct NetworkingTest;
}

struct SC::NetworkingTest : public SC::TestCase
{
    NetworkingTest(SC::TestReport& report) : TestCase(report, "NetworkingTest")
    {
        using namespace SC;
        if (test_section("tcp client server"))
        {
            TCPServer server;
            SC_TEST_EXPECT(server.listen("127.0.0.1", 5050));
            constexpr char testValue = 123;
            struct Params
            {
                ReturnCode  connectRes = false;
                ReturnCode  writeRes   = false;
                ReturnCode  closeRes   = false;
                EventObject eventObject;
            } params;
            Action func = [&]()
            {
                TCPClient client;
                params.connectRes = client.connect("127.0.0.1", 5050);
                char buf[1]       = {testValue};
                params.writeRes   = client.write({buf, sizeof(buf)});
                params.eventObject.wait();
                params.closeRes = client.close();
            };
            Thread thread;
            SC_TEST_EXPECT(thread.start("tcp", &func));
            TCPClient acceptedClient;
            SC_TEST_EXPECT(server.accept(acceptedClient));
            SC_TEST_EXPECT(acceptedClient.socket.isValid());
            char buf[1] = {0};
            SC_TEST_EXPECT(acceptedClient.read({buf, sizeof(buf)}));
            SC_TEST_EXPECT(buf[0] == testValue and testValue != 0);
            SC_TEST_EXPECT(server.close());
            SC_TEST_EXPECT(acceptedClient.close());
            params.eventObject.signal();
            SC_TEST_EXPECT(thread.join());
            SC_TEST_EXPECT(params.connectRes and params.writeRes and params.closeRes);
        }
    }
};
