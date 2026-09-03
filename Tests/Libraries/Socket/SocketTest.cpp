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
    inline void socketUDPSendToReceiveFrom(StringView address);
    inline void socketMulticast();

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
        if (test_section("udp unconnected sendTo receiveFrom"))
        {
            socketUDPSendToReceiveFrom("127.0.0.1");
            socketUDPSendToReceiveFrom("::1");
        }
        if (test_section("udp multicast"))
        {
            socketMulticast();
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

    const char utf8IPV4[] = "127.0.0.1";
    SC_TEST_EXPECT(
        address.fromAddressPort(StringSpan({utf8IPV4, sizeof(utf8IPV4) - 1}, true, StringEncoding::Utf8), 789));
    SC_TEST_EXPECT(address.getPort() == 789);

    const char utf8IPV6[] = "::1";
    SC_TEST_EXPECT(
        address.fromAddressPort(StringSpan({utf8IPV6, sizeof(utf8IPV6) - 1}, true, StringEncoding::Utf8), 987));
    SC_TEST_EXPECT(address.getPort() == 987);

    const char nonAsciiAddress[] = {
        '1', '2', '7', '.', '0', '.', '0', '.', '1', static_cast<char>(0xc3), static_cast<char>(0xa9)};
    SC_TEST_EXPECT(not address.fromAddressPort(
        StringSpan({nonAsciiAddress, sizeof(nonAsciiAddress)}, false, StringEncoding::Utf8), 123));

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

    const char utf8Localhost[] = "localhost";
    ipAddress                  = {buffer};
    SC_TEST_EXPECT(SocketDNS::resolveDNS(
        StringSpan({utf8Localhost, sizeof(utf8Localhost) - 1}, true, StringEncoding::Utf8), ipAddress));

    const char nonAsciiHost[] = {
        'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't', static_cast<char>(0xc3), static_cast<char>(0xa9)};
    ipAddress = {buffer};
    SC_TEST_EXPECT(not SocketDNS::resolveDNS(
        StringSpan({nonAsciiHost, sizeof(nonAsciiHost)}, false, StringEncoding::Utf8), ipAddress));
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

    // Test TCP_NODELAY
    SC_TEST_EXPECT(socket.setTcpNoDelay(true));
    SC_TEST_EXPECT(socket.setTcpNoDelay(false));

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
    const uint16_t              tcpPort       = report.mapPort(5050);
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
        uint16_t    port = 0;
    } params;
    params.port = tcpPort;
    SocketDescriptor clientSocket;
    SC_TEST_EXPECT(clientSocket.create(nativeAddress.getAddressFamily(), socketType, protocol));
    auto func = [&clientSocket, &params](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("func"));
        SocketClient client(clientSocket);
        params.connectRes = client.connect(serverAddress, params.port);
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

void SC::SocketTest::socketUDPSendToReceiveFrom(StringView address)
{
    const uint16_t receiverPort = report.mapPort(5052);
    const uint16_t senderPort   = report.mapPort(5053);

    SocketIPAddress receiverAddress;
    SC_TEST_EXPECT(receiverAddress.fromAddressPort(address, receiverPort));
    SocketIPAddress senderAddress;
    SC_TEST_EXPECT(senderAddress.fromAddressPort(address, senderPort));

    SocketDescriptor receiverSocket;
    SC_TEST_EXPECT(
        receiverSocket.create(receiverAddress.getAddressFamily(), SocketFlags::SocketDgram, SocketFlags::ProtocolUdp));
    SocketServer receiverServer(receiverSocket);
    SC_TEST_EXPECT(receiverServer.bind(receiverAddress));

    SocketDescriptor senderSocket;
    SC_TEST_EXPECT(
        senderSocket.create(senderAddress.getAddressFamily(), SocketFlags::SocketDgram, SocketFlags::ProtocolUdp));
    SocketServer senderServer(senderSocket);
    SC_TEST_EXPECT(senderServer.bind(senderAddress));

    SocketDescriptor invalidSocket;
    const char       invalidValue = 0;
    SC_TEST_EXPECT(not invalidSocket.sendTo({&invalidValue, 1}, receiverAddress));

    // Non-blocking receiveFrom without any pending datagram must fail softly
    char       receiveBuffer[64] = {0};
    Span<char> receivedData      = {receiveBuffer + 1, 1};

    SocketIPAddress sourceAddress;
    SC_TEST_EXPECT(sourceAddress.fromAddressPort("192.0.2.1", 1234));
    char       sourceAddressString[SocketIPAddress::MAX_ASCII_STRING_LENGTH];
    StringSpan sourceAddressSpan;

    SC_TEST_EXPECT(receiverSocket.setBlocking(false));
    SC_TEST_EXPECT(not receiverSocket.receiveFrom({receiveBuffer, sizeof(receiveBuffer)}, receivedData, sourceAddress));
    SC_TEST_EXPECT(receivedData.data() == receiveBuffer + 1 and receivedData.sizeInBytes() == 1);
    SC_TEST_EXPECT(sourceAddress.toString(sourceAddressString, sourceAddressSpan));
    SC_TEST_EXPECT(StringView(sourceAddressSpan) == StringView("192.0.2.1") and sourceAddress.getPort() == 1234);
    SC_TEST_EXPECT(receiverSocket.setBlocking(true));

    // Send a datagram from the sender to the unconnected receiver
    const char testValue = 42;
    SC_TEST_EXPECT(senderSocket.sendTo({&testValue, 1}, receiverAddress));

    SC_TEST_EXPECT(receiverSocket.receiveFrom({receiveBuffer, sizeof(receiveBuffer)}, receivedData, sourceAddress));
    SC_TEST_EXPECT(receivedData.sizeInBytes() == 1 and receiveBuffer[0] == testValue);
    SC_TEST_EXPECT(sourceAddress.toString(sourceAddressString, sourceAddressSpan));
    SC_TEST_EXPECT(StringView(sourceAddressSpan) == address);
    SC_TEST_EXPECT(sourceAddress.getPort() == senderPort);

    // And back: reply from receiver to sender, still unconnected
    const char replyValue = testValue + 1;
    SC_TEST_EXPECT(receiverSocket.sendTo({&replyValue, 1}, sourceAddress));

    SC_TEST_EXPECT(senderSocket.receiveFrom({receiveBuffer, sizeof(receiveBuffer)}, receivedData, sourceAddress));
    SC_TEST_EXPECT(receivedData.sizeInBytes() == 1 and receiveBuffer[0] == replyValue);
    SC_TEST_EXPECT(sourceAddress.getPort() == receiverPort);

    // A zero-length datagram is a successful receive, not a would-block result
    Span<const char> emptyDatagram = {receiveBuffer, 0};
    SC_TEST_EXPECT(senderSocket.sendTo(emptyDatagram, receiverAddress));
    SC_TEST_EXPECT(receiverSocket.receiveFrom({receiveBuffer, sizeof(receiveBuffer)}, receivedData, sourceAddress));
    SC_TEST_EXPECT(receivedData.sizeInBytes() == 0 and sourceAddress.getPort() == senderPort);

    // An oversized datagram is consumed but reported as an error without publishing partial outputs
    char oversizedDatagram[sizeof(receiveBuffer) + 1] = {0};
    SC_TEST_EXPECT(senderSocket.sendTo({oversizedDatagram, sizeof(oversizedDatagram)}, receiverAddress));

    Span<char> unchangedReceivedData = {receiveBuffer + 1, 1};
    receivedData                     = unchangedReceivedData;
    SC_TEST_EXPECT(sourceAddress.fromAddressPort("192.0.2.1", 1234));
    SC_TEST_EXPECT(not receiverSocket.receiveFrom({receiveBuffer, sizeof(receiveBuffer)}, receivedData, sourceAddress));
    SC_TEST_EXPECT(receivedData.data() == unchangedReceivedData.data() and
                   receivedData.sizeInBytes() == unchangedReceivedData.sizeInBytes());
    SC_TEST_EXPECT(sourceAddress.toString(sourceAddressString, sourceAddressSpan));
    SC_TEST_EXPECT(StringView(sourceAddressSpan) == StringView("192.0.2.1") and sourceAddress.getPort() == 1234);

    SC_TEST_EXPECT(senderSocket.close());
    SC_TEST_EXPECT(receiverSocket.close());
}

void SC::SocketTest::socketMulticast()
{
    const uint16_t multicastPort = report.mapPort(5051);

    SocketIPAddress defaultInterfaceAddress;
#if SC_PLATFORM_WINDOWS
    static constexpr StringView interfaceAddressString = "0.0.0.0";
#else
    static constexpr StringView interfaceAddressString = "127.0.0.1";
#endif
    SC_TEST_EXPECT(defaultInterfaceAddress.fromAddressPort(interfaceAddressString, 0));

    SocketIPAddress anyAddress;
    SC_TEST_EXPECT(anyAddress.fromAddressPort("0.0.0.0", multicastPort));

    SocketIPAddress multicastAddress;
    SC_TEST_EXPECT(multicastAddress.fromAddressPort("239.255.0.1", multicastPort));

    SocketIPAddress mismatchedInterfaceAddress;
    SC_TEST_EXPECT(mismatchedInterfaceAddress.fromAddressPort("::1", 0));

    SocketDescriptor receiverSocket;
    SC_TEST_EXPECT(
        receiverSocket.create(SocketFlags::AddressFamilyIPV4, SocketFlags::SocketDgram, SocketFlags::ProtocolUdp));
    SocketServer receiverServer(receiverSocket);
    SC_TEST_EXPECT(receiverServer.bind(anyAddress, SocketServer::BindReuseAddress::Enabled));

    SocketDescriptor senderSocket;
    SC_TEST_EXPECT(
        senderSocket.create(SocketFlags::AddressFamilyIPV4, SocketFlags::SocketDgram, SocketFlags::ProtocolUdp));

    SC_TEST_EXPECT(senderSocket.setBroadcast(true));
    SC_TEST_EXPECT(senderSocket.setBroadcast(false));
    SC_TEST_EXPECT(senderSocket.setMulticastLoopback(SocketFlags::AddressFamilyIPV4, true));
    SC_TEST_EXPECT(senderSocket.setMulticastHops(SocketFlags::AddressFamilyIPV4, 2));
    SC_TEST_EXPECT(senderSocket.setMulticastOutboundInterface(defaultInterfaceAddress));

    SocketClient senderClient(senderSocket);
    SC_TEST_EXPECT(senderClient.connect(multicastAddress));

    SocketClient receiverClient(receiverSocket);
    Span<char>   readData;
    char         readBuffer[1] = {0};

    // Before joining the multicast group, no datagram should be received.
    char preJoinValue = 11;
    SC_TEST_EXPECT(senderClient.write({&preJoinValue, 1}));
    SC_TEST_EXPECT(not receiverClient.readWithTimeout({readBuffer, sizeof(readBuffer)}, readData, 200));

    SC_TEST_EXPECT(not receiverSocket.joinMulticastGroup(multicastAddress, mismatchedInterfaceAddress));
    SC_TEST_EXPECT(not receiverSocket.leaveMulticastGroup(multicastAddress, mismatchedInterfaceAddress));

    SC_TEST_EXPECT(receiverSocket.joinMulticastGroup(multicastAddress, defaultInterfaceAddress));

    // After joining the multicast group, the receiver should obtain datagrams.
    const char joinedValue = 22;
    SC_TEST_EXPECT(senderClient.write({&joinedValue, 1}));
    SC_TEST_EXPECT(receiverClient.readWithTimeout({readBuffer, sizeof(readBuffer)}, readData, 2000));
    SC_TEST_EXPECT(readData.sizeInBytes() == 1 and readBuffer[0] == joinedValue);

    SC_TEST_EXPECT(receiverSocket.leaveMulticastGroup(multicastAddress, defaultInterfaceAddress));

    SC_TEST_EXPECT(senderSocket.close());
    SC_TEST_EXPECT(receiverSocket.close());
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
