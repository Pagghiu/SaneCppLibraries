// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_socket
//! @{

/// @brief Stable portable failures returned by the Socket library.
enum class SocketError : uint32_t
{
    NetworkingNotInitialized = 1,
    UnsupportedTextEncoding,
    InputCapacityExceeded,
    InvalidIPAddress,
    InvalidAddress,
    AddressTypeMismatch,
    AddressFamilyMismatch,
    OperationUnsupported,
    InvalidSocket,
    DestinationSocketInUse,
    InvalidShutdownType,
    SocketCreationFailed,
    SocketCloseFailed,
    DescriptorQueryFailed,
    DescriptorConfigurationFailed,
    SocketOptionFailed,
    ShutdownFailed,
    BindFailed,
    AddressInUse,
    ListenFailed,
    AcceptFailed,
    ConnectFailed,
    SendFailed,
    ReceiveFailed,
    IncompleteSend,
    DatagramTruncated,
    WouldBlock,
    TimedOut,
    DNSResolutionFailed,
    OutputCapacityExceeded,
};

/// @brief Stable operation or backend stage retained for a Socket failure.
enum class SocketErrorDetail : uint16_t
{
    None = 0,
    ParseIPv4Address,
    ParseIPv6Address,
    BuildUnixPathAddress,
    BuildUnixAbstractAddress,
    ExtractIPAddress,
    ExtractUnixAddress,
    CopyHostName,
    ResolveHostName,
    CopyResolvedAddress,
    GetSocketAddress,
    PosixCloseDescriptor,
    WindowsCloseSocket,
    PosixGetDescriptorFlags,
    PosixSetDescriptorFlags,
    WindowsGetHandleInformation,
    WindowsSetHandleInformation,
    WindowsSetBlocking,
    CreateSocket,
    SetTcpNoDelay,
    SetBroadcast,
    JoinIPv4Multicast,
    JoinIPv6Multicast,
    LeaveIPv4Multicast,
    LeaveIPv6Multicast,
    SetIPv4MulticastLoopback,
    SetIPv6MulticastLoopback,
    SetIPv4MulticastHops,
    SetIPv6MulticastHops,
    SetIPv4MulticastInterface,
    SetIPv6MulticastInterface,
    ShutdownSocket,
    SendDatagram,
    ReceiveDatagram,
    BindSocket,
    ListenSocket,
    AcceptConnection,
    ConnectSocket,
    SendStream,
    ReceiveStream,
    WaitForRead,
};

/// @brief Kind of the scalar diagnostic context stored in ResultSocket.
enum class SocketErrorContextKind : uint16_t
{
    None = 0,
    NativeError,
    ResolverError,
    RequiredBytes,
    ActualBytes,
};

/// @brief A typed scalar payload for a Socket error. Its interpretation is selected by SocketErrorContextKind.
union SocketErrorContext
{
    uint32_t nativeError;
    int32_t  resolverError;
    uint32_t requiredBytes;
    uint32_t actualBytes;

    constexpr SocketErrorContext(uint32_t value = 0) : nativeError(value) {}
};

/// @brief Stable category assigned to errors owned by Socket.
static constexpr ResultCategory SocketResultCategory = ResultCategory(8);

/// @brief Socket result retaining a stable operation detail and one scalar diagnostic context.
/// @details The composed Result is authoritative. Plain and foreign conversions clear Socket-specific context, while
/// same-domain copies retain it. Conversion to Result preserves only the category/error identity.
struct [[nodiscard]] ResultSocket
{
    Result                 result;
    SocketErrorDetail      detail      = SocketErrorDetail::None;
    SocketErrorContextKind contextKind = SocketErrorContextKind::None;
    SocketErrorContext     context     = {};

    constexpr ResultSocket(bool valid = true) : result(valid) {}
    constexpr ResultSocket(SocketError error, SocketErrorDetail detail = SocketErrorDetail::None)
        : result(Result::Error(SocketResultCategory, error)), detail(detail)
    {}
    constexpr ResultSocket(SocketError error, SocketErrorDetail detail, SocketErrorContextKind contextKind,
                           SocketErrorContext context)
        : result(Result::Error(SocketResultCategory, error)), detail(detail), contextKind(contextKind), context(context)
    {}
    constexpr ResultSocket(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultSocket(const ResultLike& other) : result(other.toResult())
    {}

    static constexpr ResultSocket withNativeError(SocketError error, SocketErrorDetail detail, uint32_t nativeError)
    {
        return {error, detail, SocketErrorContextKind::NativeError, SocketErrorContext(nativeError)};
    }

    static constexpr ResultSocket withResolverError(SocketError error, SocketErrorDetail detail, int32_t resolverError)
    {
        SocketErrorContext context;
        context.resolverError = resolverError;
        return {error, detail, SocketErrorContextKind::ResolverError, context};
    }

    static constexpr ResultSocket withRequiredBytes(SocketError error, SocketErrorDetail detail, uint32_t requiredBytes)
    {
        return {error, detail, SocketErrorContextKind::RequiredBytes, SocketErrorContext(requiredBytes)};
    }

    static constexpr ResultSocket withActualBytes(SocketError error, SocketErrorDetail detail, uint32_t actualBytes)
    {
        return {error, detail, SocketErrorContextKind::ActualBytes, SocketErrorContext(actualBytes)};
    }

    explicit constexpr operator bool() const { return static_cast<bool>(result); }
    constexpr          operator Result() const { return result; }
    constexpr Result   toResult() const { return result; }
    constexpr bool     isError(SocketError error) const { return result.isError(SocketResultCategory, error); }
};

//! @}
} // namespace SC
