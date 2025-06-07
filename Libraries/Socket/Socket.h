// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "SocketDescriptor.h"

#include "../Foundation/Span.h"
#include "../Foundation/StringViewData.h"
#include "../Time/Time.h" // Milliseconds

namespace SC
{
struct SocketNetworking;
struct SocketClient;
struct SocketServer;
struct SocketDNS;
} // namespace SC

//! @addtogroup group_socket
//! @{

/// @brief Use a SocketDescriptor as a Server (example TCP or UDP Socket Server).
///
/// Example:
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketServerSnippet
struct SC::SocketServer
{
    /// @brief Build a SocketServer from a SocketDescriptor (already created with SocketDescriptor::create)
    /// @param socket A socket descriptor created with SocketDescriptor::create to be used as server
    SocketServer(SocketDescriptor& socket) : socket(socket) {}

    /// @brief Calls SocketDescriptor::close
    /// @return The Result of SocketDescriptor::close
    [[nodiscard]] Result close();

    /// @brief Binds this socket to a given address / port combination
    /// @param nativeAddress The interface ip address and port to start listening to
    /// @return Valid Result if this socket has successfully been bound
    [[nodiscard]] Result bind(SocketIPAddress nativeAddress);

    /// @brief Start listening for incoming connections at a specific address / port combination (after bind)
    /// @param numberOfWaitingConnections How many connections can be queued before `accept`
    /// @return Valid Result if this socket has successfully been put in listening mode
    /// @note UDP socket cannot be listened. TCP socket need a successful SocketServer::bind before SocketServer::listen
    [[nodiscard]] Result listen(uint32_t numberOfWaitingConnections);

    /// @brief Accepts a new client, blocking while waiting for it
    /// @param[in] addressFamily The address family of the SocketDescriptor that will be created
    /// @param[out] newClient The SocketDescriptor that will be accepted
    /// @return Valid Result if the socket has been successfully accepted
    [[nodiscard]] Result accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient);

  private:
    SocketDescriptor& socket;
};

/// @brief Use a SocketDescriptor as a client (example a TCP or UDP socket client).
///
/// The socket client can be obtained via SC::SocketServer::accept or connected to an endpoint
/// through SC::SocketClient::connect.
///
/// Example (accepted client from server, doing a synchronous read):
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketClientAcceptSnippet
///
/// Example (connecting client to server, doing two synchronous writes):
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketClientConnectSnippet
struct SC::SocketClient
{
    /// @brief Constructs this SocketClient from a SocketDescriptor (already created with SocketDescriptor::create)
    /// @param socket A socket descriptor created with SocketDescriptor::create to be used as client
    SocketClient(const SocketDescriptor& socket) : socket(socket) {}

    /// @brief Connect to a given address and port combination
    /// @param address Address as ASCII encoded string
    /// @param port Port to start listening to
    /// @return Valid Result if this client successfully connected to the specified address and port
    /// @note Socket descriptor MUST have already been created with SocketDescriptor::create
    [[nodiscard]] Result connect(StringViewData address, uint16_t port);

    /// @brief Connect to a given address and port combination
    /// @param ipAddress Address and port to connect to
    /// @return Valid Result if this client successfully connected to the specified address and port
    [[nodiscard]] Result connect(SocketIPAddress ipAddress);

    /// @brief Writes bytes to this socket
    /// @param data Bytes to write to this socket
    /// @return Valid Result if bytes have been written successfully
    [[nodiscard]] Result write(Span<const char> data);

    /// @brief Read bytes from this socket blocking until they're actually received
    /// @param[in] data Span of memory pointing at a buffer that will receive the read data
    /// @param[out] readData A sub-Span of `data` that has the length of actually read bytes
    /// @return Valid Result if bytes have been read successfully
    [[nodiscard]] Result read(Span<char> data, Span<char>& readData);

    /// @brief Read bytes from this socket blocking until they're actually received or timeout occurs
    /// @param[in] data Span of memory pointing at a buffer that will receive the read data
    /// @param[out] readData A sub-Span of `data` that has the length of actually read bytes
    /// @param[in] timeout For how many milliseconds the read should wait before timing out
    /// @return Valid Result if bytes have been read successfully and timeout didn't occur
    [[nodiscard]] Result readWithTimeout(Span<char> data, Span<char>& readData, Time::Milliseconds timeout);

  private:
    const SocketDescriptor& socket;
};

/// @brief Synchronous DNS Resolution
///
/// Example:
/// @snippet Tests/Libraries/Socket/SocketTest.cpp resolveDNSSnippet
struct SC::SocketDNS
{
    /// @brief Resolve an host string to an ip address (blocking until DNS response arrives)
    /// @param[in] host The ASCII encoded host string (example.com)
    /// @param[out] ipAddress Host ip address (ASCII encoded and null-terminated)
    /// @return Valid Result if ip address for the passed host has been successfully resolved
    ///
    /// Example:
    /// @snippet Tests/Libraries/Socket/SocketTest.cpp resolveDNSSnippet
    [[nodiscard]] static Result resolveDNS(StringViewData host, Span<char>& ipAddress);
};

/// @brief Networking globals initialization (Winsock2 WSAStartup)
struct SC::SocketNetworking
{
    /// @brief Initializes Winsock2 on Windows (WSAStartup)
    /// @return Valid Result if Winsock2 has been successfully initialized
    [[nodiscard]] static Result initNetworking();

    /// @brief Shutdowns Winsock2 on Windows (WSAStartup)
    /// @return Valid Result if Winsock2 has been successfully shutdown
    [[nodiscard]] static Result shutdownNetworking();

    /// @brief Check if initNetworking has been previously called
    /// @return `true` if initNetworking has been previously called
    [[nodiscard]] static bool isNetworkingInited();

  private:
    struct Internal;
};

//! @}
