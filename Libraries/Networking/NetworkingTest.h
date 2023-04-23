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
            // Look for an available port
            constexpr int startTcpPort = 5050;
            int           tcpPort;
            ReturnCode    bound = true;
            for (tcpPort = startTcpPort; tcpPort < startTcpPort + 10; ++tcpPort)
            {
                bound = server.listen("127.0.0.1", tcpPort);
                if (bound)
                {
                    break;
                }
            }
            SC_TEST_EXPECT(bound);
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
                params.connectRes = client.connect("127.0.0.1", tcpPort);
                char buf[1]       = {testValue};
                params.writeRes   = client.write({buf, sizeof(buf)});
                params.eventObject.wait();
                buf[0]++;
                params.writeRes = client.write({buf, sizeof(buf)});
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
            SC_TEST_EXPECT(not acceptedClient.readWithTimeout({buf, sizeof(buf)}, 10_ms));
            params.eventObject.signal();
            SC_TEST_EXPECT(acceptedClient.readWithTimeout({buf, sizeof(buf)}, 10000_ms));
            SC_TEST_EXPECT(buf[0] == testValue + 1);
            SC_TEST_EXPECT(acceptedClient.close());
            SC_TEST_EXPECT(server.close());
            params.eventObject.signal();
            SC_TEST_EXPECT(thread.join());
            SC_TEST_EXPECT(params.connectRes and params.writeRes and params.closeRes);
        }
    }
};
