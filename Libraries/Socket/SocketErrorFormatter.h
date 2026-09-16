// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "SocketError.h"

namespace SC
{
namespace detail
{
inline bool appendSocketErrorDetail(ResultErrorFormatter& formatter, SocketErrorDetail detail)
{
    switch (detail)
    {
    case SocketErrorDetail::None: return true;
    case SocketErrorDetail::ParseIPv4Address: formatter.append("parse IPv4 address"); break;
    case SocketErrorDetail::ParseIPv6Address: formatter.append("parse IPv6 address"); break;
    case SocketErrorDetail::BuildUnixPathAddress: formatter.append("build Unix-domain path address"); break;
    case SocketErrorDetail::BuildUnixAbstractAddress: formatter.append("build Unix-domain abstract address"); break;
    case SocketErrorDetail::ExtractIPAddress: formatter.append("extract IP address"); break;
    case SocketErrorDetail::ExtractUnixAddress: formatter.append("extract Unix-domain address"); break;
    case SocketErrorDetail::CopyHostName: formatter.append("copy host name"); break;
    case SocketErrorDetail::ResolveHostName: formatter.append("resolve host name"); break;
    case SocketErrorDetail::CopyResolvedAddress: formatter.append("copy resolved address"); break;
    case SocketErrorDetail::GetSocketAddress: formatter.append("query socket address"); break;
    case SocketErrorDetail::PosixCloseDescriptor: formatter.append("POSIX close socket descriptor"); break;
    case SocketErrorDetail::WindowsCloseSocket: formatter.append("Windows close socket"); break;
    case SocketErrorDetail::PosixGetDescriptorFlags: formatter.append("POSIX query descriptor flags"); break;
    case SocketErrorDetail::PosixSetDescriptorFlags: formatter.append("POSIX set descriptor flags"); break;
    case SocketErrorDetail::WindowsGetHandleInformation: formatter.append("Windows query handle information"); break;
    case SocketErrorDetail::WindowsSetHandleInformation: formatter.append("Windows set handle information"); break;
    case SocketErrorDetail::WindowsSetBlocking: formatter.append("Windows set blocking mode"); break;
    case SocketErrorDetail::CreateSocket: formatter.append("create socket"); break;
    case SocketErrorDetail::SetTcpNoDelay: formatter.append("set TCP no-delay option"); break;
    case SocketErrorDetail::SetBroadcast: formatter.append("set broadcast option"); break;
    case SocketErrorDetail::JoinIPv4Multicast: formatter.append("join IPv4 multicast group"); break;
    case SocketErrorDetail::JoinIPv6Multicast: formatter.append("join IPv6 multicast group"); break;
    case SocketErrorDetail::LeaveIPv4Multicast: formatter.append("leave IPv4 multicast group"); break;
    case SocketErrorDetail::LeaveIPv6Multicast: formatter.append("leave IPv6 multicast group"); break;
    case SocketErrorDetail::SetIPv4MulticastLoopback: formatter.append("set IPv4 multicast loopback"); break;
    case SocketErrorDetail::SetIPv6MulticastLoopback: formatter.append("set IPv6 multicast loopback"); break;
    case SocketErrorDetail::SetIPv4MulticastHops: formatter.append("set IPv4 multicast hops"); break;
    case SocketErrorDetail::SetIPv6MulticastHops: formatter.append("set IPv6 multicast hops"); break;
    case SocketErrorDetail::SetIPv4MulticastInterface: formatter.append("set IPv4 multicast interface"); break;
    case SocketErrorDetail::SetIPv6MulticastInterface: formatter.append("set IPv6 multicast interface"); break;
    case SocketErrorDetail::ShutdownSocket: formatter.append("shut down socket"); break;
    case SocketErrorDetail::SendDatagram: formatter.append("send datagram"); break;
    case SocketErrorDetail::ReceiveDatagram: formatter.append("receive datagram"); break;
    case SocketErrorDetail::BindSocket: formatter.append("bind socket"); break;
    case SocketErrorDetail::ListenSocket: formatter.append("listen on socket"); break;
    case SocketErrorDetail::AcceptConnection: formatter.append("accept connection"); break;
    case SocketErrorDetail::ConnectSocket: formatter.append("connect socket"); break;
    case SocketErrorDetail::SendStream: formatter.append("send stream data"); break;
    case SocketErrorDetail::ReceiveStream: formatter.append("receive stream data"); break;
    case SocketErrorDetail::WaitForRead: formatter.append("wait for readable socket"); break;
    default: return false;
    }
    return true;
}

inline bool appendSocketError(ResultErrorFormatter& formatter, SocketError error)
{
    switch (error)
    {
    case SocketError::NetworkingNotInitialized: formatter.append("Networking is not initialized"); break;
    case SocketError::UnsupportedTextEncoding: formatter.append("Text encoding is not supported"); break;
    case SocketError::InputCapacityExceeded: formatter.append("Input capacity exceeded"); break;
    case SocketError::InvalidIPAddress: formatter.append("IP address is invalid"); break;
    case SocketError::InvalidAddress: formatter.append("Socket address is invalid"); break;
    case SocketError::AddressTypeMismatch: formatter.append("Socket address type does not match"); break;
    case SocketError::AddressFamilyMismatch: formatter.append("Socket address families do not match"); break;
    case SocketError::OperationUnsupported: formatter.append("Socket operation is unsupported"); break;
    case SocketError::InvalidSocket: formatter.append("Socket descriptor is invalid"); break;
    case SocketError::DestinationSocketInUse: formatter.append("Destination socket is already in use"); break;
    case SocketError::InvalidShutdownType: formatter.append("Shutdown type is invalid"); break;
    case SocketError::SocketCreationFailed: formatter.append("Failed to create socket"); break;
    case SocketError::SocketCloseFailed: formatter.append("Failed to close socket"); break;
    case SocketError::DescriptorQueryFailed: formatter.append("Failed to query socket descriptor"); break;
    case SocketError::DescriptorConfigurationFailed: formatter.append("Failed to configure socket descriptor"); break;
    case SocketError::SocketOptionFailed: formatter.append("Failed to configure socket option"); break;
    case SocketError::ShutdownFailed: formatter.append("Failed to shut down socket"); break;
    case SocketError::BindFailed: formatter.append("Failed to bind socket"); break;
    case SocketError::AddressInUse: formatter.append("Socket address is already in use"); break;
    case SocketError::ListenFailed: formatter.append("Failed to listen on socket"); break;
    case SocketError::AcceptFailed: formatter.append("Failed to accept connection"); break;
    case SocketError::ConnectFailed: formatter.append("Failed to connect socket"); break;
    case SocketError::SendFailed: formatter.append("Failed to send socket data"); break;
    case SocketError::ReceiveFailed: formatter.append("Failed to receive socket data"); break;
    case SocketError::IncompleteSend: formatter.append("Socket send was incomplete"); break;
    case SocketError::DatagramTruncated: formatter.append("Datagram was truncated"); break;
    case SocketError::WouldBlock: formatter.append("Socket operation would block"); break;
    case SocketError::TimedOut: formatter.append("Socket operation timed out"); break;
    case SocketError::DNSResolutionFailed: formatter.append("Failed to resolve host name"); break;
    case SocketError::OutputCapacityExceeded: formatter.append("Output capacity exceeded"); break;
    default: return false;
    }
    return true;
}

inline ResultErrorFormat formatSocketErrorWithContext(SocketError error, SocketErrorDetail detail,
                                                      SocketErrorContextKind contextKind, SocketErrorContext context,
                                                      Span<char> output)
{
    ResultErrorFormatter formatter(output);
    if (not appendSocketError(formatter, error))
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    if (detail != SocketErrorDetail::None)
    {
        formatter.append(" (detail: ");
        if (not appendSocketErrorDetail(formatter, detail))
            return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
        formatter.append(")");
    }
    switch (contextKind)
    {
    case SocketErrorContextKind::None: break;
    case SocketErrorContextKind::NativeError:
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(context.nativeError));
        formatter.append(")");
        break;
    case SocketErrorContextKind::ResolverError:
        formatter.append(" (resolver error: ");
        if (context.resolverError < 0)
        {
            formatter.append("-");
            formatter.append(static_cast<uint64_t>(-static_cast<int64_t>(context.resolverError)));
        }
        else
            formatter.append(static_cast<uint64_t>(context.resolverError));
        formatter.append(")");
        break;
    case SocketErrorContextKind::RequiredBytes:
        formatter.append(" (required capacity: ");
        formatter.append(static_cast<uint64_t>(context.requiredBytes));
        formatter.append(" bytes)");
        break;
    case SocketErrorContextKind::ActualBytes:
        formatter.append(" (actual bytes: ");
        formatter.append(static_cast<uint64_t>(context.actualBytes));
        formatter.append(")");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}
} // namespace detail

inline ResultErrorFormat formatSocketError(SocketError error, Span<char> output)
{
    return detail::formatSocketErrorWithContext(error, SocketErrorDetail::None, SocketErrorContextKind::None, {},
                                                output);
}

inline ResultErrorFormat formatSocketError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != SocketResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatSocketError(static_cast<SocketError>(result.errorValue()), output);
}

inline ResultErrorFormat formatSocketError(ResultSocket result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != SocketResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatSocketErrorWithContext(static_cast<SocketError>(result.result.errorValue()), result.detail,
                                                result.contextKind, result.context, output);
}
} // namespace SC
