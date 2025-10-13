// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"

namespace SC
{
struct SocketTest;
}

struct SC::SocketTest : public SC::TestCase
{
    inline void parseAddress();
    inline void resolveDNS();
    inline void socketCreate();
    inline void socketClientServer(SocketFlags::SocketType socketType, SocketFlags::ProtocolType protocol);

    inline Result socketServerSnippet();
    inline Result socketClientAcceptSnippet();
    inline Result socketClientConnectSnippet();

    SocketTest(SC::TestReport& report) : TestCase(report, "SocketTest")
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
        if (test_section("socket base"))
        {
            socketCreate();
        }
        if (test_section("tcp client server"))
        {
            socketClientServer(SocketFlags::SocketStream, SocketFlags::ProtocolTcp);
        }
        if (test_section("udp client server (connected)"))
        {
            socketClientServer(SocketFlags::SocketDgram, SocketFlags::ProtocolUdp);
        }
    }
};

void SC::SocketTest::parseAddress()
{
    //! [socketIpAddressSnippet]
    SocketIPAddress address;
    SC_TEST_EXPECT(not address.fromAddressPort("1223.22.44.1", 6666));
    SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", 123));
    SC_TEST_EXPECT(address.getPort() == 123);
    SC_TEST_EXPECT(address.fromAddressPort("::1", 456));
    SC_TEST_EXPECT(address.getPort() == 456);

    const char  badMemory[]  = "oh yeah that's a really broken socket ip address";
    const auto& badIPAddress = *reinterpret_cast<const SocketIPAddress*>(&badMemory);
    SC_TEST_EXPECT(not badIPAddress.isValid());
    //! [socketIpAddressSnippet]
}

void SC::SocketTest::resolveDNS()
{
    //! [resolveDNSSnippet]
    char       buffer[256] = {0};
    Span<char> ipAddress   = {buffer};
    SC_TEST_EXPECT(SocketDNS::resolveDNS("localhost", ipAddress));
    StringView ipString = StringView(ipAddress, true, StringEncoding::Ascii);
    SC_TEST_EXPECT(ipString == "127.0.0.1" or ipString == "::1");
    //! [resolveDNSSnippet]
}

void SC::SocketTest::socketCreate()
{
    //! [socketCreateSnippet]
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
    //! [socketCreateSnippet]
}

void SC::SocketTest::socketClientServer(SocketFlags::SocketType socketType, SocketFlags::ProtocolType protocol)
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
    SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily(), socketType, protocol));
    SC_TEST_EXPECT(server.bind(nativeAddress));
    if (protocol == SocketFlags::ProtocolTcp)
    {
        SC_TEST_EXPECT(server.listen(0));
    }
    static constexpr char testValue = 123;
    struct Params
    {
        Result      connectRes = Result(false);
        Result      writeRes   = Result(false);
        Result      closeRes   = Result(false);
        EventObject eventObject;
    } params;
    SocketDescriptor clientSocket;
    SC_TEST_EXPECT(clientSocket.create(nativeAddress.getAddressFamily(), socketType, protocol));
    auto func = [&clientSocket, &params](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("func"));
        SocketClient client(clientSocket);
        params.connectRes = client.connect(serverAddress, tcpPort);
        char buf[1]       = {testValue};
        params.writeRes   = client.write({buf, sizeof(buf)});
        params.eventObject.wait();
        buf[0]++;
        params.writeRes = client.write({buf, sizeof(buf)});
        params.eventObject.wait();
        params.closeRes = clientSocket.close();
    };
    Thread thread;
    SC_TEST_EXPECT(thread.start(func));
    SocketFlags::AddressFamily family;
    SC_TEST_EXPECT(serverSocket.getAddressFamily(family));
    SocketDescriptor acceptedClientSocket;
    if (protocol == SocketFlags::ProtocolTcp)
    {
        SC_TEST_EXPECT(server.accept(family, acceptedClientSocket));
        SC_TEST_EXPECT(acceptedClientSocket.isValid());
    }

    SocketDescriptor& socket = protocol == SocketFlags::ProtocolTcp ? acceptedClientSocket : serverSocket;
    SocketClient      acceptedClient(socket);
    Span<char>        readData;
    char              buf[1] = {0};
    SC_TEST_EXPECT(acceptedClient.read({buf, sizeof(buf)}, readData));
    SC_TEST_EXPECT(buf[0] == testValue and testValue != 0);
    SC_TEST_EXPECT(not acceptedClient.readWithTimeout({buf, sizeof(buf)}, readData, 10));
    params.eventObject.signal();
    SC_TEST_EXPECT(acceptedClient.readWithTimeout({buf, sizeof(buf)}, readData, 10 * 1000));
    SC_TEST_EXPECT(buf[0] == testValue + 1);
    if (socketType == SocketFlags::SocketStream)
    {
        // This only makes sense on TCP sockets, it will fail on unconnected UDP sockets
        SC_TEST_EXPECT(socket.shutdown(SocketFlags::ShutdownBoth));
    }
    SC_TEST_EXPECT(socket.close());
    SC_TEST_EXPECT(server.close());
    params.eventObject.signal();
    SC_TEST_EXPECT(thread.join());
    SC_TEST_EXPECT(params.connectRes and params.writeRes and params.closeRes);
}

SC::Result SC::SocketTest::socketServerSnippet()
{
    //! [socketServerSnippet]
    SocketDescriptor serverSocket;
    SocketServer     server(serverSocket);

    // Look for an available port
    constexpr int    tcpPort       = 5050;
    const StringView serverAddress = "::1"; // or "127.0.0.1"
    SocketIPAddress  nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(serverAddress, tcpPort));
    SocketFlags::AddressFamily family = nativeAddress.getAddressFamily();

    // Create socket and start listening
    SC_TRY(serverSocket.create(family)); // By default creates a TCP Server

    // [Alternatively] Create an UDP socket instead
    // SC_TRY(serverSocket.create(family, SocketFlags::SocketDgram, SocketFlags::ProtocolUdp));

    SC_TRY(server.bind(nativeAddress)); // Bind the socket to the given address
    SC_TRY(server.listen(1));           // Start listening (skip this for UDP sockets)

    // Accept a client
    SocketDescriptor acceptedClientSocket;
    SC_TRY(server.accept(family, acceptedClientSocket));
    SC_TRY(acceptedClientSocket.isValid());

    // ... Do something with acceptedClientSocket
    //! [socketServerSnippet]
    return Result(true);
}

SC::Result SC::SocketTest::socketClientAcceptSnippet()
{
    SocketDescriptor serverSocket;
    SocketServer     server(serverSocket);

    // Look for an available port
    constexpr int    tcpPort       = 5050;
    const StringView serverAddress = "::1"; // or "127.0.0.1"
    SocketIPAddress  nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(serverAddress, tcpPort));

    // Create (TCP) socket and start listening
    SC_TRY(serverSocket.create(nativeAddress.getAddressFamily()));
    SC_TRY(server.bind(nativeAddress));
    SC_TRY(server.listen(1)); // Start listening (skip this for UDP sockets)

    SocketFlags::AddressFamily family;
    SC_TEST_EXPECT(serverSocket.getAddressFamily(family));

    //! [socketClientAcceptSnippet]
    SocketDescriptor acceptedClientSocket;
    // ... assuming to obtain a TCP socket using SocketServer::accept
    SC_TRY(server.accept(family, acceptedClientSocket));
    SC_TRY(acceptedClientSocket.isValid());

    // Read some data blocking until it's available
    char buf[256];

    SocketClient acceptedClient(acceptedClientSocket);
    Span<char>   readData;
    SC_TRY(acceptedClient.read({buf, sizeof(buf)}, readData));

    // ... later on

    // Read again blocking but with a timeout of 10 seconds
    SC_TRY(acceptedClient.readWithTimeout({buf, sizeof(buf)}, readData, 10 * 1000));

    // Close the client
    SC_TRY(acceptedClientSocket.close());
    //! [socketClientAcceptSnippet]
    return Result(true);
}

SC::Result SC::SocketTest::socketClientConnectSnippet()
{
    SocketDescriptor serverSocket;
    SocketServer     server(serverSocket);

    // Look for an available port
    constexpr int    tcpPort       = 5050;
    const StringView serverAddress = "::1"; // or "127.0.0.1"
    SocketIPAddress  nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(serverAddress, tcpPort));

    // Create a (TCP) socket and start listening
    SocketFlags::AddressFamily family = nativeAddress.getAddressFamily();
    SC_TRY(serverSocket.create(family));
    SC_TRY(server.bind(nativeAddress));
    SC_TRY(server.listen(1)); // Start listening (skip this for UDP sockets)

    //! [socketClientConnectSnippet]

    // ...assuming there is a socket listening at given serverAddress and tcpPort
    SocketDescriptor clientSocket;
    SocketClient     client(clientSocket);

    // Create a (TCP) socket
    SC_TRY(clientSocket.create(family));

    // [Alternatively] Create an UDP socket instead
    // SC_TRY(clientSocket.create(family, SocketFlags::SocketDgram, SocketFlags::ProtocolUdp));

    // Connect to the server
    SC_TRY(client.connect(serverAddress, tcpPort));

    // Write some data to the socket
    const int testValue = 1;
    char      buf[1]    = {testValue};
    SC_TRY(client.write({buf, sizeof(buf)}));
    buf[0]++; // change the value and write again
    SC_TRY(client.write({buf, sizeof(buf)}));

    // Close the socket
    SC_TRY(clientSocket.close());
    //! [socketClientConnectSnippet]
    return Result(true);
}

namespace SC
{
void runSocketTest(SC::TestReport& report) { SocketTest test(report); }
} // namespace SC
