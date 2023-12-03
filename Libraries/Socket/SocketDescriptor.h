// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../File/FileDescriptor.h"
#include "../Foundation/OpaqueObject.h"
#include "../Foundation/Result.h"
#include "../System/Time.h" // Milliseconds

namespace SC
{
struct String;
template <typename T>
struct Vector;
} // namespace SC

namespace SC
{
struct SocketDescriptor;
struct SocketDescriptorDefinition;
struct SocketFlags;
struct SocketIPAddress;
} // namespace SC

//! @defgroup group_socket Socket
//! @copybrief library_socket
//!
//! See @ref library_socket library page for more details.<br>

//! @addtogroup group_socket
//! @{

/// @brief Definition for SC::SocketDescriptor
#if SC_PLATFORM_WINDOWS

struct SC::SocketDescriptorDefinition
{
    using Handle = size_t; // SOCKET
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = ~static_cast<Handle>(0); // INVALID_SOCKET
};

#else

struct SC::SocketDescriptorDefinition
{
    using Handle = int; // fd
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = -1; // invalid fd
};

#endif

/// @brief Flags for SC::SocketDescriptor (Blocking / Inheritable, IPVx, SocketType)
struct SC::SocketFlags
{
    /// @brief Sets the socket as blocking / nonblocking mode
    enum BlockingType
    {
        NonBlocking, ///< Socket is in non-blocking mode
        Blocking     ///< Socket is in blocking mode
    };

    /// @brief Sets the socket inheritable behaviour for child processes
    enum InheritableType
    {
        NonInheritable, ///< Socket will not be inherited by child processes
        Inheritable     ///< Socket will be inherited by child processes
    };

    /// @brief Sets the addresss family of an IP Address (IPv4 or IPV6)
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

  private:
    friend struct SocketDescriptor;
    friend struct NetworkingInternal;
    [[nodiscard]] static AddressFamily AddressFamilyFromInt(int value);
    [[nodiscard]] static unsigned char toNative(AddressFamily family);
    [[nodiscard]] static SocketType    SocketTypeFromInt(int value);
    [[nodiscard]] static int           toNative(SocketType family);

    [[nodiscard]] static ProtocolType ProtocolTypeFromInt(int value);
    [[nodiscard]] static int          toNative(ProtocolType family);
};

/// @brief Native representation of an IP Address
struct SC::SocketIPAddress
{
    /// @brief Constructs an ip address from a given family (IPV4 or IPV6)
    /// @param addressFamily The address family
    SocketIPAddress(SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4)
        : addressFamily(addressFamily)
    {}

    /// @brief Get Address family of this ip address (IPV4 or IPV6)
    /// @return The Ip Address Family of the given Socket
    [[nodiscard]] SocketFlags::AddressFamily getAddressFamily() { return addressFamily; }

    /// @brief Builds this SocketIPAddress parsing given address string and port
    /// @param interfaceAddress A valid IPV4 or IPV6 address expressed as a string
    /// @param port The port to connect to
    /// @return A valid Result if the address has been parsed successfully
    [[nodiscard]] Result fromAddressPort(StringView interfaceAddress, uint16_t port);

    friend struct SocketServer;
    friend struct SocketClient;
    // TODO: maybe we should only save a binary address instead of native structs (IPV6 would need 16 bytes)
    uint32_t           sizeOfHandle() const;
    AlignedStorage<28> handle;

  private:
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
};

/// @brief An actual OS Socket Descriptor
struct SC::SocketDescriptor : public UniqueHandle<SocketDescriptorDefinition>
{
    /// @brief Creates a new Socket Descriptor of given family, type, protocol
    /// @param addressFamily Address family (IPV4 / IPV6)
    /// @param socketType Socket type (Stream or Dgram)
    /// @param protocol Protocol (TCP or UDP)
    /// @param blocking If the socket should be created in blocking mode
    /// @param inheritable If the socket shoudl be inheritable by child processess
    /// @return Valid Result if a socket with the requested options has been successfully created
    [[nodiscard]] Result create(SocketFlags::AddressFamily   addressFamily,
                                SocketFlags::SocketType      socketType  = SocketFlags::SocketStream,
                                SocketFlags::ProtocolType    protocol    = SocketFlags::ProtocolTcp,
                                SocketFlags::BlockingType    blocking    = SocketFlags::Blocking,
                                SocketFlags::InheritableType inheritable = SocketFlags::NonInheritable);

    /// @brief Check if socket is inheritable by child processes
    /// @param[out] value if set to `true` indicates that this socket is inheritable by child processes
    /// @return Valid Result if the inheritable status for this socket has been queried successfully
    [[nodiscard]] Result isInheritable(bool& value) const;

    /// @brief Changes the inheritable flag for this socket
    /// @param value `true` if this socket should be made inheritable, `false` for non-inheritable
    /// @return Valid Result if it has been possible changing the inheritable status of this socket
    [[nodiscard]] Result setInheritable(bool value);

    /// @brief Changes the blocking flag for this socket (if IO reads / writes should be blockign or not)
    /// @param value `true` if this socket should be made blocking, `false` for non-blocking
    /// @return Valid Result if it has been possible changing the blocking status of this socket
    [[nodiscard]] Result setBlocking(bool value);

    /// @brief Get address family (IPV4 / IPV6) of this socket
    /// @param[out] addressFamily The address family of this socket (if Result is valid)
    /// @return Valid Result the address family for this socket has been queried successfully
    [[nodiscard]] Result getAddressFamily(SocketFlags::AddressFamily& addressFamily) const;
};

namespace SC
{
struct SocketClient;
struct SocketServer;
struct DNSResolver;
} // namespace SC

/// @brief Use a SocketDescriptor as a Server (example TCP Socket Server)
struct SC::SocketServer
{
    /// @brief Build a SocketServer from a SocketDescriptor (already created with SocketDescriptor::create)
    /// @param socket A socket descriptor created with SocketDescriptor::create to be used as server
    SocketServer(SocketDescriptor& socket) : socket(socket) {}

    /// @brief Calls SocketDescriptor::close
    /// @return The Result of SocketDescriptor::close
    [[nodiscard]] Result close();

    /// @brief Start listening for incoming connections at a specific address / port combination
    /// @param nativeAddress The interface ip address and port to start listening to
    /// @param numberOfWaitingConnections How many connections can be queued before `accept`
    /// @return Valid Result if this socket has successfully been put in listening mode (bind + listen)
    [[nodiscard]] Result listen(SocketIPAddress nativeAddress, uint32_t numberOfWaitingConnections = 511);

    /// @brief Accepts a new client, blocking while waiting for it
    /// @param[in] addressFamily The address family of the SocketDescriptor that will be created
    /// @param[out] newClient The SocketDescriptor that will be accepted
    /// @return Valid Result if the socket has been successfully accepted
    [[nodiscard]] Result accept(SocketFlags::AddressFamily addressFamily, SocketDescriptor& newClient);

  private:
    SocketDescriptor& socket;
};

/// @brief Use a SocketDescriptor as a client (example TCP Socket Client)
struct SC::SocketClient
{
    /// @brief Constructs this SocketClient from a SocketDescriptor (already created with SocketDescriptor::create)
    /// @param socket A socket descriptor created with SocketDescriptor::create to be used as client
    SocketClient(SocketDescriptor& socket) : socket(socket) {}

    /// @brief Calls SocketDescriptor::close
    /// @return The Result of SocketDescriptor::close
    [[nodiscard]] Result close();

    /// @brief Connect to a given address and port combination
    /// @param address Address as string
    /// @param port Port to start listening to
    /// @return Valid Result if this client successfully connected to the specified address and port
    [[nodiscard]] Result connect(StringView address, uint16_t port);

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
    /// @param[in] timeout For how many millisenconds the read should wait before timing out
    /// @return Valid Result if bytes have been read successfully and timeout didn't occurr
    [[nodiscard]] Result readWithTimeout(Span<char> data, Span<char>& readData, Time::Milliseconds timeout);

  private:
    SocketDescriptor& socket;
};

/// @brief Blocking DNS resolver to convert a string to an ip address
struct SC::DNSResolver
{
    /// @brief Resolve an host string to an ip address (blocking until DNS response arrives)
    /// @param[in] host The host string (example.com)
    /// @param[out] ipAddress The ip address of the given host string
    /// @return Valid Result if ip address for the passed host has been successfully resolved
    [[nodiscard]] static Result resolve(StringView host, String& ipAddress);
};

//! @}
