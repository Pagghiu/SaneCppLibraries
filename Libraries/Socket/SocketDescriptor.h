// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/StringViewData.h"
#include "../Foundation/UniqueHandle.h"

namespace SC
{
namespace detail
{
struct SocketDescriptorDefinition;
}
struct SC_COMPILER_EXPORT SocketDescriptor;
struct SocketFlags;
struct SocketIPAddress;
} // namespace SC

//! @defgroup group_socket Socket
//! @copybrief library_socket (see @ref library_socket for more details).
///
/// Socket library allows creating TCP / UDP sockets and using them as client or server and resolving DNS.
///
/// It can be used standalone if synchronous networking is preferred or as a companion to [Async](@ref library_async)
/// for creation of non-blocking socket descriptors.

//! @addtogroup group_socket
//! @{

/// @brief Definition for SocketDescriptor
#if SC_PLATFORM_WINDOWS

struct SC::detail::SocketDescriptorDefinition
{
    using Handle = size_t; // SOCKET
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = ~static_cast<Handle>(0); // INVALID_SOCKET
};

#else

struct SC::detail::SocketDescriptorDefinition
{
    using Handle = int; // fd
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = -1; // invalid fd
};

#endif

/// @brief Flags for SocketDescriptor (Blocking / Inheritable, IPVx, SocketType)
struct SC::SocketFlags
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
        ShutdownRead,  ///< Shuts down the socket for reading
        ShutdownWrite, ///< Shuts down the socket for writing
        ShutdownBoth   ///< Shuts down the socket for both reading and writing
    };

  private:
    friend struct SocketDescriptor;
    friend struct SocketDescriptor;
    friend struct SocketIPAddressInternal;
    [[nodiscard]] static AddressFamily AddressFamilyFromInt(int value);
    [[nodiscard]] static unsigned char toNative(AddressFamily family);
    [[nodiscard]] static SocketType    SocketTypeFromInt(int value);
    [[nodiscard]] static int           toNative(SocketType family);

    [[nodiscard]] static ProtocolType ProtocolTypeFromInt(int value);
    [[nodiscard]] static int          toNative(ProtocolType family);
};

/// @brief Native representation of an IP Address.
///
/// Example:
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketIpAddressSnippet
struct SC::SocketIPAddress
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
    [[nodiscard]] Result fromAddressPort(StringViewData interfaceAddress, uint16_t port);

    /// @brief Size of the native IP Address representation
    [[nodiscard]] uint32_t sizeOfHandle() const;

    /// @brief Checks if this is a valid IPV4 or IPV6 address
    [[nodiscard]] bool isValid() const;

    /// @brief Returns the text representation of this SocketIPAddress
    /// @param buffer Buffer to store the ASCII representation of the IP Address
    /// @return A sub-Span of `buffer` that has the length of actually written bytes
    /// @note The buffer must be at least `MAX_ASCII_STRING_LENGTH` bytes long
    template <size_t N>
    [[nodiscard]] StringViewData toString(char (&buffer)[N]) const
    {
        static_assert(N >= MAX_ASCII_STRING_LENGTH, "Insufficient buffer");
        return formatAddress(buffer);
    }

    /// @brief Handle to native OS representation of the IP Address
    AlignedStorage<28> handle = {};

  private:
    [[nodiscard]] StringViewData formatAddress(Span<char> buffer) const;
    friend struct SocketServer;
    friend struct SocketClient;

    struct Internal;
};

/// @brief Low-level OS socket handle.
/// It also allow querying inheritability and changing it (and blocking mode)
/// @n
/// Example (extracted from unit test):
/// @snippet Tests/Libraries/Socket/SocketTest.cpp socketCreateSnippet
struct SC::SocketDescriptor : public UniqueHandle<detail::SocketDescriptorDefinition>
{
    /// @brief Creates a new SocketDescriptor Descriptor of given family, type, protocol
    /// @param addressFamily Address family (IPV4 / IPV6)
    /// @param socketType SocketDescriptor type (Stream or Dgram)
    /// @param protocol Protocol (TCP or UDP)
    /// @param blocking If the socket should be created in blocking mode
    /// @param inheritable If the socket should be inheritable by child processes
    /// @return Valid Result if a socket with the requested options has been successfully created
    [[nodiscard]] Result create(SocketFlags::AddressFamily   addressFamily,
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
//! @}
