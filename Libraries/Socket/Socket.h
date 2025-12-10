// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Result.h"
#include "../Foundation/StringSpan.h"
#include "../Foundation/UniqueHandle.h"

namespace SC
{
//! @addtogroup group_socket
//! @{

namespace detail
{

/// @brief Definition for SocketDescriptor
#if SC_PLATFORM_WINDOWS

struct SocketDescriptorDefinition
{
    using Handle = size_t; // SOCKET
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = ~static_cast<Handle>(0); // INVALID_SOCKET
};

#else

struct SocketDescriptorDefinition
{
    using Handle = int; // fd
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = -1; // invalid fd
};

#endif
} // namespace detail

/// @brief Flags for SocketDescriptor (Blocking / Inheritable, IPVx, SocketType)
struct SocketFlags
{
    /// @brief Sets the socket as blocking / nonblocking mode
    enum BlockingType
    {
        NonBlocking, ///< SocketDescriptor is in non-blocking mode
        Blocking     ///< SocketDescriptor is in blocking mode
    };

    /// @brief Sets the socket inheritable behaviour for child processes
    enum InheritableType
    {
        NonInheritable, ///< SocketDescriptor will not be inherited by child processes
        Inheritable     ///< SocketDescriptor will be inherited by child processes
    };

    /// @brief Sets the address family of an IP Address (IPv4 or IPV6)
    enum AddressFamily
    {
        AddressFamilyIPV4, ///< IP Address is IPV4
        AddressFamilyIPV6, ///< IP Address is IPV6
    };

    /// @brief Sets the socket type, if it's a Datagram (for UDP) or Streaming (for TCP and others)
    enum SocketType
    {
        SocketStream, ///< Sets the socket type as Streaming type (for TCP and others)
        SocketDgram   ///< Sets the socket type as Streaming type (for UDP)
    };

    /// @brief Sets the socket protocol type
    enum ProtocolType
    {
        ProtocolTcp, ///< The protocol is TCP
        ProtocolUdp, ///< The protocol is UDP
    };

    /// @brief Sets the type of shutdown to perform
    enum ShutdownType
    {
        ShutdownBoth ///< Shuts down the socket for both reading and writing
    };

  private:
    friend struct SocketDescriptor;
    friend struct SocketIPAddressInternal;
    [[nodiscard]] static AddressFamily AddressFamilyFromInt(int value);
    [[nodiscard]] static unsigned char toNative(AddressFamily family);

    [[nodiscard]] static int toNative(SocketType family);
    [[nodiscard]] static int toNative(ProtocolType family);
};

/// @brief Native representation of an IP Address.
///
/// Example:
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketIpAddressSnippet
struct SC_COMPILER_EXPORT SocketIPAddress
{
    /// @brief Maximum length of the ASCII representation of an IP Address
    static constexpr int MAX_ASCII_STRING_LENGTH = 46;

    /// @brief Buffer for storing the ASCII representation of an IP Address
    using AsciiBuffer = char[MAX_ASCII_STRING_LENGTH];

    /// @brief Constructs an ip address from a given family (IPV4 or IPV6)
    /// @param addressFamily The address family
    SocketIPAddress(SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4);

    /// @brief Get Address family of this ip address (IPV4 or IPV6)
    /// @return The Ip Address Family of the given SocketDescriptor
    [[nodiscard]] SocketFlags::AddressFamily getAddressFamily() const;

    /// @brief Get port of this ip address
    /// @return The port of the given SocketIPAddress
    [[nodiscard]] uint16_t getPort() const;

    /// @brief Builds this SocketIPAddress parsing given address string and port
    /// @param interfaceAddress A valid IPV4 or IPV6 address expressed as an ASCII string
    /// @param port The port to connect to
    /// @return A valid Result if the address has been parsed successfully
    Result fromAddressPort(StringSpan interfaceAddress, uint16_t port);

    /// @brief Size of the native IP Address representation
    [[nodiscard]] uint32_t sizeOfHandle() const;

    /// @brief Checks if this is a valid IPV4 or IPV6 address
    [[nodiscard]] bool isValid() const;

    /// @brief Returns the text representation of this SocketIPAddress
    /// @param inputSpan Buffer to store the ASCII representation of the IP Address
    /// @param outputSpan A sub-Span of `inputSpan` that has the length of actually written bytes
    /// @note The buffer must be at least `MAX_ASCII_STRING_LENGTH` bytes long
    [[nodiscard]] bool toString(Span<char> inputSpan, StringSpan& outputSpan) const;

    /// @brief Handle to native OS representation of the IP Address
    AlignedStorage<28> handle = {};

  private:
    friend struct SocketServer;
    friend struct SocketClient;

    struct Internal;
};

/// @brief Low-level OS socket handle.
/// It also allow querying inheritability and changing it (and blocking mode)
/// @n
/// Example (extracted from unit test):
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketCreateSnippet
struct SC_COMPILER_EXPORT SocketDescriptor : public UniqueHandle<detail::SocketDescriptorDefinition>
{
    /// @brief Creates a new SocketDescriptor Descriptor of given family, type, protocol
    /// @param addressFamily Address family (IPV4 / IPV6)
    /// @param socketType SocketDescriptor type (Stream or Dgram)
    /// @param protocol Protocol (TCP or UDP)
    /// @param blocking If the socket should be created in blocking mode
    /// @param inheritable If the socket should be inheritable by child processes
    /// @return Valid Result if a socket with the requested options has been successfully created
    Result create(SocketFlags::AddressFamily   addressFamily,
                  SocketFlags::SocketType      socketType  = SocketFlags::SocketStream,
                  SocketFlags::ProtocolType    protocol    = SocketFlags::ProtocolTcp,
                  SocketFlags::BlockingType    blocking    = SocketFlags::Blocking,
                  SocketFlags::InheritableType inheritable = SocketFlags::NonInheritable);

    /// @brief Check if socket is inheritable by child processes
    /// @param[out] value if set to `true` indicates that this socket is inheritable by child processes
    /// @return Valid Result if the inheritable status for this socket has been queried successfully
    Result isInheritable(bool& value) const;

    /// @brief Changes the inheritable flag for this socket
    /// @param value `true` if this socket should be made inheritable, `false` for non-inheritable
    /// @return Valid Result if it has been possible changing the inheritable status of this socket
    Result setInheritable(bool value);

    /// @brief Changes the blocking flag for this socket (if IO reads / writes should be blocking or not)
    /// @param value `true` if this socket should be made blocking, `false` for non-blocking
    /// @return Valid Result if it has been possible changing the blocking status of this socket
    Result setBlocking(bool value);

    /// @brief Get address family (IPV4 / IPV6) of this socket
    /// @param[out] addressFamily The address family of this socket (if Result is valid)
    /// @return Valid Result the address family for this socket has been queried successfully
    Result getAddressFamily(SocketFlags::AddressFamily& addressFamily) const;

    /// @brief Shuts down the socket for reading, writing, or both
    /// @param shutdownType The type of shutdown to perform
    /// @return Valid Result if the socket has been successfully shut down
    Result shutdown(SocketFlags::ShutdownType shutdownType);
};

/// @brief Use a SocketDescriptor as a Server (example TCP or UDP Socket Server).
///
/// Example:
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketServerSnippet
struct SocketServer
{
    /// @brief Build a SocketServer from a SocketDescriptor (already created with SocketDescriptor::create)
    /// @param socket A socket descriptor created with SocketDescriptor::create to be used as server
    SocketServer(SocketDescriptor& socket) : socket(socket) {}

    /// @brief Calls SocketDescriptor::close
    /// @return The Result of SocketDescriptor::close
    Result close();

    /// @brief Binds this socket to a given address / port combination
    /// @param nativeAddress The interface ip address and port to start listening to
    /// @return Valid Result if this socket has successfully been bound
    Result bind(SocketIPAddress nativeAddress);

    /// @brief Start listening for incoming connections at a specific address / port combination (after bind)
    /// @param numberOfWaitingConnections How many connections can be queued before `accept`
    /// @return Valid Result if this socket has successfully been put in listening mode
    /// @note UDP socket cannot be listened. TCP socket need a successful SocketServer::bind before SocketServer::listen
    Result listen(uint32_t numberOfWaitingConnections);

    /// @brief Accepts a new client, blocking while waiting for it
    /// @param[in] addressFamily The address family of the SocketDescriptor that will be created
    /// @param[out] newClient The SocketDescriptor that will be accepted
    /// @return Valid Result if the socket has been successfully accepted
    Result accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient);

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
struct SocketClient
{
    /// @brief Constructs this SocketClient from a SocketDescriptor (already created with SocketDescriptor::create)
    /// @param socket A socket descriptor created with SocketDescriptor::create to be used as client
    SocketClient(const SocketDescriptor& socket) : socket(socket) {}

    /// @brief Connect to a given address and port combination
    /// @param address Address as ASCII encoded string
    /// @param port Port to start listening to
    /// @return Valid Result if this client successfully connected to the specified address and port
    /// @note Socket descriptor MUST have already been created with SocketDescriptor::create
    Result connect(StringSpan address, uint16_t port);

    /// @brief Connect to a given address and port combination
    /// @param ipAddress Address and port to connect to
    /// @return Valid Result if this client successfully connected to the specified address and port
    Result connect(SocketIPAddress ipAddress);

    /// @brief Writes bytes to this socket
    /// @param data Bytes to write to this socket
    /// @return Valid Result if bytes have been written successfully
    Result write(Span<const char> data);

    /// @brief Read bytes from this socket blocking until they're actually received
    /// @param[in] data Span of memory pointing at a buffer that will receive the read data
    /// @param[out] readData A sub-Span of `data` that has the length of actually read bytes
    /// @return Valid Result if bytes have been read successfully
    Result read(Span<char> data, Span<char>& readData);

    /// @brief Read bytes from this socket blocking until they're actually received or timeout occurs
    /// @param[in] data Span of memory pointing at a buffer that will receive the read data
    /// @param[out] readData A sub-Span of `data` that has the length of actually read bytes
    /// @param[in] timeout For how many milliseconds the read should wait before timing out
    /// @return Valid Result if bytes have been read successfully and timeout didn't occur
    Result readWithTimeout(Span<char> data, Span<char>& readData, int64_t timeout);

  private:
    const SocketDescriptor& socket;
};

/// @brief Synchronous DNS Resolution
///
/// Example:
/// @snippet Tests/Libraries/Socket/SocketTest.cpp resolveDNSSnippet
struct SocketDNS
{
    /// @brief Resolve an host string to an ip address (blocking until DNS response arrives)
    /// @param[in] host The ASCII encoded host string (example.com)
    /// @param[out] ipAddress Host ip address (ASCII encoded and null-terminated)
    /// @return Valid Result if ip address for the passed host has been successfully resolved
    ///
    /// Example:
    /// @snippet Tests/Libraries/Socket/SocketTest.cpp resolveDNSSnippet
    [[nodiscard]] static Result resolveDNS(StringSpan host, Span<char>& ipAddress);
};

/// @brief Networking globals initialization (Winsock2 WSAStartup)
struct SocketNetworking
{
    /// @brief Initializes Winsock2 on Windows (WSAStartup)
    /// @return Valid Result if Winsock2 has been successfully initialized
    [[nodiscard]] static Result initNetworking();

    /// @brief Shutdowns Winsock2 on Windows (WSAStartup)
    static void shutdownNetworking();

    /// @brief Check if initNetworking has been previously called
    /// @return `true` if initNetworking has been previously called
    [[nodiscard]] static bool isNetworkingInited();

  private:
    struct Internal;
};

//! @}
} // namespace SC
