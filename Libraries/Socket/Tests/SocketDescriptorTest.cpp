// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../SocketDescriptor.h"
#include "../../Strings/SmallString.h"
#include "../../Testing/Testing.h"
#include "../../Threading/Threading.h"

namespace SC
{
struct SocketDescriptorTest;
}

struct SC::SocketDescriptorTest : public SC::TestCase
{
    inline void parseAddress();
    inline void resolveDNS();
    inline void socketDescriptor();
    inline void tcpClientServer();
    SocketDescriptorTest(SC::TestReport& report) : TestCase(report, "SocketDescriptorTest")
    {
        using namespace SC;
        if (test_section("parseAddress"))
        {
            parseAddress();
        }
        if (test_section("DNS"))
        {
            resolveDNS();
        }
        if (test_section("socket"))
        {
            socketDescriptor();
        }
        if (test_section("tcp client server"))
        {
            tcpClientServer();
        }
    }
};

void SC::SocketDescriptorTest::parseAddress()
{
    //! [socketIpAddressSnippet]
    SocketIPAddress address;
    SC_TEST_EXPECT(not address.fromAddressPort("1223.22.44.1", 6666));
    SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", 123));
    SC_TEST_EXPECT(address.fromAddressPort("::1", 123));
    //! [socketIpAddressSnippet]
}

void SC::SocketDescriptorTest::resolveDNS()
{
    //! [resolveDNSSnippet]
    SmallString<256> ipAddress;
    SC_TEST_EXPECT(SocketDNS::resolveDNS("localhost", ipAddress));
    SC_TEST_EXPECT(ipAddress.view() == "127.0.0.1");
    //! [resolveDNSSnippet]
}

void SC::SocketDescriptorTest::socketDescriptor()
{
    //! [socketDescriptorSnippet]
    bool isInheritable;

    // We are testing only the inheritable because on windows there is no reliable
    // way of checking if a non-connected socket is in non-blocking mode
    SocketDescriptor socket;
    SC_TEST_EXPECT(socket.create(SocketFlags::AddressFamilyIPV4, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                 SocketFlags::NonBlocking, SocketFlags::NonInheritable));
    SC_TEST_EXPECT(socket.isValid());
    isInheritable = false;
    SC_TEST_EXPECT(socket.isInheritable(isInheritable));
    SC_TEST_EXPECT(not isInheritable);
    SC_TEST_EXPECT(socket.close());

    SC_TEST_EXPECT(socket.create(SocketFlags::AddressFamilyIPV4, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                 SocketFlags::Blocking, SocketFlags::NonInheritable));
    SC_TEST_EXPECT(socket.isValid());
    isInheritable = false;
    SC_TEST_EXPECT(socket.isInheritable(isInheritable));
    SC_TEST_EXPECT(not isInheritable);
    SC_TEST_EXPECT(socket.close());

    SC_TEST_EXPECT(socket.create(SocketFlags::AddressFamilyIPV4, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                 SocketFlags::Blocking, SocketFlags::Inheritable));
    SC_TEST_EXPECT(socket.isValid());
    isInheritable = false;
    SC_TEST_EXPECT(socket.isInheritable(isInheritable));
    SC_TEST_EXPECT(isInheritable);
    SC_TEST_EXPECT(socket.close());
    //! [socketDescriptorSnippet]
}

void SC::SocketDescriptorTest::tcpClientServer()
{
    SocketDescriptor           serverSocket;
    SocketServer               server(serverSocket);
    SocketFlags::AddressFamily invalidFamily;
    SC_TEST_EXPECT(not serverSocket.getAddressFamily(invalidFamily));
    // Look for an available port
    static constexpr uint16_t   tcpPort       = 5050;
    static constexpr StringView serverAddress = "::1"; //"127.0.0.1"

    SocketIPAddress nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort(serverAddress, tcpPort));
    SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(server.listen(nativeAddress, tcpPort));
    constexpr char testValue = 123;
    struct Params
    {
        Result      connectRes = Result(false);
        Result      writeRes   = Result(false);
        Result      closeRes   = Result(false);
        EventObject eventObject;
    } params;
    auto func = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("func"));
        SocketDescriptor clientSocket;
        SocketClient     client(clientSocket);
        params.connectRes = client.connect(serverAddress, tcpPort);
        char buf[1]       = {testValue};
        params.writeRes   = client.write({buf, sizeof(buf)});
        params.eventObject.wait();
        buf[0]++;
        params.writeRes = client.write({buf, sizeof(buf)});
        params.eventObject.wait();
        params.closeRes = client.close();
    };
    Thread thread;
    SC_TEST_EXPECT(thread.start(func));
    SocketFlags::AddressFamily family;
    SC_TEST_EXPECT(serverSocket.getAddressFamily(family));
    SocketDescriptor acceptedClientSocket;
    SC_TEST_EXPECT(server.accept(family, acceptedClientSocket));
    SC_TEST_EXPECT(acceptedClientSocket.isValid());
    char         buf[1] = {0};
    SocketClient acceptedClient(acceptedClientSocket);
    Span<char>   readData;
    SC_TEST_EXPECT(acceptedClient.read({buf, sizeof(buf)}, readData));
    SC_TEST_EXPECT(buf[0] == testValue and testValue != 0);
    SC_TEST_EXPECT(not acceptedClient.readWithTimeout({buf, sizeof(buf)}, readData, Time::Milliseconds(10)));
    params.eventObject.signal();
    SC_TEST_EXPECT(acceptedClient.readWithTimeout({buf, sizeof(buf)}, readData, Time::Seconds(10)));
    SC_TEST_EXPECT(buf[0] == testValue + 1);
    SC_TEST_EXPECT(acceptedClient.close());
    SC_TEST_EXPECT(server.close());
    params.eventObject.signal();
    SC_TEST_EXPECT(thread.join());
    SC_TEST_EXPECT(params.connectRes and params.writeRes and params.closeRes);
}

namespace SC
{
void runSocketDescriptorTest(SC::TestReport& report) { SocketDescriptorTest test(report); }
} // namespace SC
