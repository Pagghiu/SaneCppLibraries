// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../../../Libraries/Socket/Socket.h"
#include "AsyncUV.h"
#include <stddef.h>
#include <string.h>

namespace SC
{
namespace
{
ptrdiff_t getAsyncReceiveOffsetForUdp()
{
    static const ptrdiff_t offset = []()
    {
        AsyncUVUdp udp(nullptr, UV_UDP);
        return reinterpret_cast<char*>(&udp.asyncReceive) - reinterpret_cast<char*>(&udp);
    }();
    return offset;
}

ptrdiff_t getAsyncCloseOffsetForUdp()
{
    static const ptrdiff_t offset = []()
    {
        AsyncUVUdp udp(nullptr, UV_UDP);
        return reinterpret_cast<char*>(&udp.asyncClose) - reinterpret_cast<char*>(&udp);
    }();
    return offset;
}

int mapUdpSendStatus()
{
#if SC_PLATFORM_WINDOWS
    const int systemError = WSAGetLastError();
#else
    const int systemError = errno;
#endif
    const int status = AsyncUVMapSystemErrorToUv(systemError);
#if defined(SC_ASYNC_UV_TEST_DIAGNOSTICS)
    fprintf(stderr, "AsyncUV UDP send error: native=%d uv=%d (%s)\n", systemError, status, uv_err_name(status));
#endif
    return status;
}

int getCurrentUdpError()
{
#if SC_PLATFORM_WINDOWS
    return AsyncUVMapSystemErrorToUv(WSAGetLastError());
#else
    return AsyncUVMapSystemErrorToUv(errno);
#endif
}

socklen_t getSocketAddressSize(const struct sockaddr* addr)
{
    return addr->sa_family == AF_INET6 ? static_cast<socklen_t>(sizeof(sockaddr_in6))
                                       : static_cast<socklen_t>(sizeof(sockaddr_in));
}

unsigned int getUdpMaxPayload(const struct sockaddr* addr)
{
    static constexpr unsigned int MaxUdpPayloadIPV4 = 65507;
    static constexpr unsigned int MaxUdpPayloadIPV6 = 65527;
    return addr->sa_family == AF_INET6 ? MaxUdpPayloadIPV6 : MaxUdpPayloadIPV4;
}

int getUdpDescriptor(uv_udp_t* handle, SocketDescriptor::Handle& socketHandle)
{
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }
    AsyncUVUdp& uvObject = *AsyncUVUdp::from(handle);
    if (not uvObject.descriptor.get(socketHandle, Result(false)))
    {
        return UV_EBADF;
    }
    return 0;
}

int getUdpAddressFamily(uv_udp_t* handle, SocketFlags::AddressFamily& addressFamily)
{
    AsyncUVUdp& uvObject = *AsyncUVUdp::from(handle);
    if (not uvObject.descriptor.isValid())
    {
        return UV_EBADF;
    }
    if (not uvObject.descriptor.getAddressFamily(addressFamily))
    {
        return UV_EBADF;
    }
    return 0;
}

bool isUdpSocketAlreadyOpen(uv_udp_t* handle, uv_os_sock_t socket)
{
    bool found = false;
    AsyncUVLoop::get(handle->loop)
        .handles.forEach(
            [handle, socket, &found](uv_handle_t& otherHandle)
            {
                if (&otherHandle == reinterpret_cast<uv_handle_t*>(handle) or otherHandle.type != UV_UDP)
                {
                    return true;
                }

                const AsyncUVUdp&        otherUdp = *AsyncUVUdp::from(reinterpret_cast<uv_udp_t*>(&otherHandle));
                SocketDescriptor::Handle otherSocket;
                if (otherUdp.descriptor.get(otherSocket, Result(false)) and
                    static_cast<uv_os_sock_t>(otherSocket) == socket)
                {
                    found = true;
                    return false;
                }
                return true;
            });
    return found;
}

int setUnicastTtl(SocketDescriptor::Handle socketHandle, SocketFlags::AddressFamily addressFamily, int ttl)
{
    if (ttl < 1 or ttl > 255)
    {
        return UV_EINVAL;
    }
    if (addressFamily == SocketFlags::AddressFamilyIPV4)
    {
#if SC_PLATFORM_WINDOWS
        DWORD nativeTtl = static_cast<DWORD>(ttl);
        return ::setsockopt(socketHandle, IPPROTO_IP, IP_TTL, reinterpret_cast<const char*>(&nativeTtl),
                            sizeof(nativeTtl)) == 0
                   ? 0
                   : UV_EINVAL;
#else
        return ::setsockopt(socketHandle, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0 ? 0 : UV_EINVAL;
#endif
    }

#if SC_PLATFORM_WINDOWS
    DWORD nativeTtl = static_cast<DWORD>(ttl);
    return ::setsockopt(socketHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, reinterpret_cast<const char*>(&nativeTtl),
                        sizeof(nativeTtl)) == 0
               ? 0
               : UV_EINVAL;
#else
    return ::setsockopt(socketHandle, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl)) == 0 ? 0 : UV_EINVAL;
#endif
}

int parseUdpAddress(const char* addressText, SocketIPAddress& address)
{
    if (addressText == nullptr)
    {
        return UV_EINVAL;
    }
    const StringSpan text({addressText, ::strlen(addressText)}, true, StringEncoding::Ascii);
    return address.fromAddressPort(text, 0) ? 0 : UV_EINVAL;
}

int parseUdpInterfaceAddress(const char* interfaceText, SocketFlags::AddressFamily addressFamily,
                             SocketIPAddress& interfaceAddress)
{
    if (interfaceText != nullptr)
    {
        const int status = parseUdpAddress(interfaceText, interfaceAddress);
        if (status != 0)
        {
            return status;
        }
        return interfaceAddress.getAddressFamily() == addressFamily ? 0 : UV_EINVAL;
    }

    const char* defaultInterface = addressFamily == SocketFlags::AddressFamilyIPV4 ? "0.0.0.0" : "::";
    return parseUdpAddress(defaultInterface, interfaceAddress);
}

int sendUdpBuffers(SocketDescriptor::Handle nativeSocket, Span<const char> buffers[], unsigned int nBuffers,
                   unsigned int pendingBytes, const struct sockaddr* addr)
{
    char         storage[65527];
    const char*  sendData = "";
    unsigned int offset   = 0;

    if (nBuffers == 1)
    {
        sendData = buffers[0].data() == nullptr ? "" : buffers[0].data();
    }
    else
    {
        for (unsigned int idx = 0; idx < nBuffers; ++idx)
        {
            if (buffers[idx].sizeInBytes() > 0)
            {
                ::memcpy(storage + offset, buffers[idx].data(), buffers[idx].sizeInBytes());
            }
            offset += static_cast<unsigned int>(buffers[idx].sizeInBytes());
        }
        sendData = storage;
    }

    if (addr != nullptr)
    {
        const socklen_t addrLength = getSocketAddressSize(addr);
#if SC_PLATFORM_WINDOWS
        const int sent = ::sendto(nativeSocket, sendData, static_cast<int>(pendingBytes), 0, addr, addrLength);
#else
        const ssize_t sent = ::sendto(nativeSocket, sendData, pendingBytes, 0, addr, addrLength);
#endif
        return sent < 0 ? mapUdpSendStatus() : static_cast<int>(sent);
    }

#if SC_PLATFORM_WINDOWS
    const int sent = ::send(nativeSocket, sendData, static_cast<int>(pendingBytes), 0);
#else
    const ssize_t sent = ::send(nativeSocket, sendData, pendingBytes, 0);
#endif
    return sent < 0 ? mapUdpSendStatus() : static_cast<int>(sent);
}
} // namespace

uv_udp_t* AsyncUVUdp::from(AsyncRequest& async)
{
    AsyncUVFlags uvFlags = async.getUserFlags();
    if (uvFlags.getType() == UV_UDP)
    {
        return reinterpret_cast<uv_udp_t*>(reinterpret_cast<char*>(&async) - getAsyncReceiveOffsetForUdp());
    }
    if (async.getType() == AsyncRequest::Type::LoopTimeout)
    {
        return reinterpret_cast<uv_udp_t*>(reinterpret_cast<char*>(&async) - getAsyncCloseOffsetForUdp());
    }
    SC_ASSERT_RELEASE(false);
    return nullptr;
}
AsyncUVUdp* AsyncUVUdp::from(uv_udp_t* handle)
{
    SC_ASSERT_RELEASE(handle->type == UV_UDP);
    return reinterpret_cast<AsyncUVUdp*>(handle);
}

void AsyncUVUdp::construct(uv_loop_t* loop, uv_udp_t* handle)
{
    handle->type            = UV_UDP;
    handle->send_queue_size = 0;
    AsyncUVUdp* request     = AsyncUVUdp::from(handle);
    placementNew(*request, loop, handle->type);
    request->asyncReceive.setUserFlags(AsyncUVFlags(handle->type).getUserFlags());
    AsyncUVLoop::get(loop).registerActiveHandle(request);
}

AsyncRequest& AsyncUVUdp::getActiveRequest() { return asyncReceive; }

int AsyncUVUdp::recvStart(uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb)
{
    uv_buf_t buf = {0, 0};
    // TODO: Figure out max datagram size
    alloc_cb(reinterpret_cast<uv_handle_t*>(this), 64 * 1024, &buf);
    allocCallback         = alloc_cb;
    recvCallback          = recv_cb;
    asyncReceive.callback = [this](AsyncSocketReceive::Result& result)
    {
        Span<char> outData;
        uv_udp_t*  handle = reinterpret_cast<uv_udp_t*>(this);
        // TODO: Fill sockaddr and flags
        struct sockaddr* addr  = 0;
        unsigned int     flags = 0;
        if (result.completionData.disconnected)
        {
            recvCallback(handle, UV_EOF, NULL, addr, flags);
        }
        else if (result.get(outData))
        {
            uv_buf_t        buf = uv_buf_init(outData.data(), static_cast<unsigned int>(outData.sizeInBytes()));
            SocketIPAddress copiedAddress = result.getSourceAddress();
            addr                          = &copiedAddress.handle.reinterpret_as<struct sockaddr>();
            recvCallback(handle, static_cast<ssize_t>(buf.len), &buf, addr, flags);
            // TODO: Should we monitor if uv_udp_recv_stop was called?
            if (close_cb == nullptr and handle->type == UV_UDP)
            {
                uv_buf_t nextBuf = {0, 0};
                allocCallback(reinterpret_cast<uv_handle_t*>(this), 64 * 1024, &nextBuf);
                if (nextBuf.base != nullptr and nextBuf.len > 0)
                {
                    static_cast<AsyncSocketReceiveFrom&>(result.async).buffer = {nextBuf.base, nextBuf.len};
                    result.reactivateRequest(true);
                }
            }
        }
        else
        {
            if (close_cb == nullptr)
            {
                recvCallback(handle, UV_EINVAL, nullptr, addr, flags);
            }
        }
    };
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);
    if (not asyncReceive.start(eventLoop, descriptor, {buf.base, buf.len}))
    {
        // TODO: We probably should call read_cb to let user de-allocate alloc_cb memory
        return UV_EINVAL;
    }
    return 0;
}

int AsyncUVUdp::recvStop()
{
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);
    if (not asyncReceive.isFree())
    {
        Result res = asyncReceive.stop(eventLoop);
        if (not res)
        {
            return UV_EINVAL;
        }
    }
    return 0;
}

int AsyncUVUdp::send(uv_udp_send_t* req, const uv_buf_t bufs[], unsigned int nbufs, const struct sockaddr* addr,
                     uv_udp_send_cb send_cb)
{
    uv_udp_t*       handle    = reinterpret_cast<uv_udp_t*>(this);
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);
    if (close_cb != nullptr)
    {
        return UV_EINVAL;
    }
    if (nbufs == 0)
    {
        return UV_EINVAL;
    }
    if (nbufs > AsyncUVUdpSend::MaxInlineBuffers)
    {
        return UV_ENOBUFS;
    }

    AsyncUVUdpSend::init(loop, req, handle, send_cb);
    AsyncUVUdpSend& object = AsyncUVUdpSend::from(req);

    object.nBuffers     = nbufs;
    object.pendingBytes = 0;

    for (uint32_t idx = 0; idx < nbufs; ++idx)
    {
        object.buffers[idx] = {bufs[idx].base, bufs[idx].len};
        object.pendingBytes += bufs[idx].len;
    }

    bool connected = false;
    int  status    = isConnected(connected);
    if (status != 0)
    {
        return status;
    }
    if (addr != nullptr and connected)
    {
        return UV_EISCONN;
    }
    if (addr == nullptr and not connected)
    {
        return UV_EDESTADDRREQ;
    }

    SocketIPAddress address;
    if (addr != nullptr)
    {
        ::memcpy(&address.handle, addr, getSocketAddressSize(addr));
    }

    auto queueSendCompletion = [&]() -> int
    {
        pendingSendCallbacks++;
        handle->send_queue_size += object.pendingBytes;
        object.asyncError.callback = [req](AsyncLoopTimeout::Result&)
        {
            AsyncUVUdpSend& object = AsyncUVUdpSend::from(req);
            AsyncUVUdp&     udp    = *AsyncUVUdp::from(req->handle);
            udp.pendingSendCallbacks--;
            req->handle->send_queue_size -= object.pendingBytes;
            object.nBuffers     = 0;
            object.pendingBytes = 0;
            req->cb(req, object.pendingStatus);
            if (udp.closePending and udp.pendingSendCallbacks == 0)
            {
                udp.closePending = false;
                udp.close();
            }
        };
        if (not object.asyncError.start(eventLoop, 0_ms))
        {
            pendingSendCallbacks--;
            handle->send_queue_size -= object.pendingBytes;
            return UV_EINVAL;
        }
        return 0;
    };

    const unsigned int maxPayload = addr == nullptr ? 65527 : getUdpMaxPayload(addr);
    if (object.pendingBytes > maxPayload)
    {
        object.pendingStatus = UV_EMSGSIZE;
        return queueSendCompletion();
    }

    if (not descriptor.isValid())
    {
        if (not eventLoop.createAsyncUDPSocket(address.getAddressFamily(), descriptor))
        {
            return UV_EINVAL;
        }
    }

    // Capture immediate send errors before event-loop work can overwrite the native error.
    // Datagrams still awaiting backpressure handling retain the native send sequence's ordering.
    if (addr == nullptr or pendingAsyncSends == 0)
    {
        SocketDescriptor::Handle nativeSocket;
        if (not descriptor.get(nativeSocket, Result(false)))
        {
            return UV_EINVAL;
        }

        object.pendingStatus = sendUdpBuffers(nativeSocket, object.buffers, object.nBuffers, object.pendingBytes, addr);
        if (object.pendingStatus >= 0)
        {
            object.pendingStatus = 0;
        }
        if (addr == nullptr or object.pendingStatus != UV_EAGAIN)
        {
            return queueSendCompletion();
        }
    }

    object.asyncSend.callback = [req](AsyncSocketSendTo::Result& asyncResult)
    {
        AsyncUVUdpSend& object = AsyncUVUdpSend::from(req);
        AsyncUVUdp&     udp    = *AsyncUVUdp::from(req->handle);
        udp.pendingAsyncSends--;
        udp.pendingSendCallbacks--;
        req->handle->send_queue_size -= object.pendingBytes;
        object.nBuffers     = 0;
        object.pendingBytes = 0;
        int status          = asyncResult.isValid() ? 0 : mapUdpSendStatus();
        req->cb(req, status);
        if (udp.closePending and udp.pendingSendCallbacks == 0)
        {
            udp.closePending = false;
            udp.close();
        }
    };
    object.asyncSend.executeOn(sequence);
    Result res(false);
    pendingAsyncSends++;
    pendingSendCallbacks++;
    handle->send_queue_size += object.pendingBytes;
    if (object.nBuffers == 1)
    {
        res = object.asyncSend.start(eventLoop, descriptor, address, object.buffers[0]);
    }
    else
    {
        // TODO: This code path doesn't work, comment above to figure out what's wrong
        res = object.asyncSend.start(eventLoop, descriptor, address, {object.buffers, object.nBuffers});
    }
    if (not res)
    {
        pendingAsyncSends--;
        pendingSendCallbacks--;
        handle->send_queue_size -= object.pendingBytes;
        return UV_EINVAL;
    }
    return 0;
}

int AsyncUVUdp::trySend(const uv_buf_t bufs[], unsigned int nbufs, const struct sockaddr* addr)
{
    if (close_cb != nullptr or nbufs == 0 or bufs == nullptr)
    {
        return UV_EINVAL;
    }

    bool connected = false;
    int  status    = isConnected(connected);
    if (status != 0)
    {
        return status;
    }
    if (addr != nullptr and connected)
    {
        return UV_EISCONN;
    }
    if (addr == nullptr and not connected)
    {
        return UV_EDESTADDRREQ;
    }

    const unsigned int maxPayload   = addr == nullptr ? 65527 : getUdpMaxPayload(addr);
    unsigned int       pendingBytes = 0;
    for (unsigned int idx = 0; idx < nbufs; ++idx)
    {
        if (bufs[idx].base == nullptr and bufs[idx].len > 0)
        {
            return UV_EINVAL;
        }
        if (bufs[idx].len > maxPayload or pendingBytes > maxPayload - bufs[idx].len)
        {
            return UV_EMSGSIZE;
        }
        pendingBytes += bufs[idx].len;
    }

    SocketIPAddress address;
    if (addr != nullptr)
    {
        ::memcpy(&address.handle, addr, getSocketAddressSize(addr));
    }

    if (not descriptor.isValid() and addr != nullptr)
    {
        AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);
        Result          res       = eventLoop.createAsyncUDPSocket(address.getAddressFamily(), descriptor);
        if (not res)
        {
            return UV_EINVAL;
        }
    }

    SocketDescriptor::Handle nativeSocket;
    if (not descriptor.get(nativeSocket, Result(false)))
    {
        return UV_EINVAL;
    }

    Span<const char> buffers[AsyncUVUdpSend::MaxInlineBuffers];
    if (nbufs > AsyncUVUdpSend::MaxInlineBuffers)
    {
        return UV_ENOBUFS;
    }
    for (unsigned int idx = 0; idx < nbufs; ++idx)
    {
        buffers[idx] = {bufs[idx].base, bufs[idx].len};
    }

    return sendUdpBuffers(nativeSocket, buffers, nbufs, pendingBytes, addr);
}

int AsyncUVUdp::isConnected(bool& connected) const
{
    connected = false;
    if (not descriptor.isValid())
    {
        return 0;
    }

    SocketDescriptor::Handle nativeSocket;
    if (not descriptor.get(nativeSocket, Result(false)))
    {
        return UV_EBADF;
    }

    struct sockaddr_storage peerAddress;
    socklen_t               peerAddressLength = static_cast<socklen_t>(sizeof(peerAddress));
    if (::getpeername(nativeSocket, reinterpret_cast<struct sockaddr*>(&peerAddress), &peerAddressLength) == 0)
    {
        connected = peerAddressLength > 0;
        return 0;
    }

#if SC_PLATFORM_WINDOWS
    const int systemError = WSAGetLastError();
    if (systemError == WSAENOTCONN)
#else
    if (errno == ENOTCONN)
#endif
    {
        return 0;
    }
    return getCurrentUdpError();
}

void AsyncUVUdp::close(AsyncEventLoop* eventLoopOverride)
{
    if (pendingSendCallbacks != 0)
    {
        closePending = true;
        return;
    }
    AsyncEventLoop* eventLoop = eventLoopOverride;
    if (eventLoop == nullptr and loop != nullptr and AsyncUVLoop::isConstructed(loop))
    {
        eventLoop = &AsyncUVLoop::getLoop(loop);
    }

    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (descriptor.isValid())
    {
        if (not descriptor.get(socketHandle, Result(false)))
        {
            socketHandle = SocketDescriptor::Invalid;
        }
        descriptor.detach();
    }

    asyncClose.callback = [this, socketHandle](AsyncResult&) mutable
    {
        if (socketHandle != SocketDescriptor::Invalid)
        {
            (void)detail::SocketDescriptorDefinition::releaseHandle(socketHandle);
        }
        uv_handle_t* handle = reinterpret_cast<uv_handle_t*>(this);
        handle->close_cb(handle);
        AsyncUVHandle::destruct(handle);
    };

    if (eventLoop == nullptr or not asyncClose.start(*eventLoop, {0}))
    {
        if (socketHandle != SocketDescriptor::Invalid)
        {
            (void)detail::SocketDescriptorDefinition::releaseHandle(socketHandle);
        }
        uv_handle_t* handle = reinterpret_cast<uv_handle_t*>(this);
        handle->close_cb(handle);
        AsyncUVHandle::destruct(handle);
    }
}

void AsyncUVUdpSend::init(uv_loop_t* loop, uv_udp_send_t* req, uv_udp_t* handle, uv_udp_send_cb send_cb)
{
    ::memset(req, 0, sizeof(uv_udp_send_t));
    req->type   = UV_UDP_SEND;
    req->handle = handle;
    req->cb     = send_cb;
    placementNew(from(req));
}

AsyncUVUdpSend& AsyncUVUdpSend::from(uv_udp_send_t* req) { return *reinterpret_cast<AsyncUVUdpSend*>(req->sc_opaque); }

} // namespace SC

UV_API int uv_udp_init(uv_loop_t* uvLoop, uv_udp_t* handle)
{
    using namespace SC;
    AsyncUVUdp::construct(uvLoop, handle);
    return 0;
}

UV_API int uv_udp_init_ex(uv_loop_t* uvLoop, uv_udp_t* handle, unsigned int flags)
{
    using namespace SC;
    const unsigned int supportedFlags = UV_UDP_RECVMMSG;
    const unsigned int domain         = flags & ~supportedFlags;

    SocketFlags::AddressFamily addressFamily;
    switch (domain)
    {
    case AF_UNSPEC: uv_udp_init(uvLoop, handle); return 0;
    case AF_INET: addressFamily = SocketFlags::AddressFamilyIPV4; break;
    case AF_INET6: addressFamily = SocketFlags::AddressFamilyIPV6; break;
    default: return UV_EINVAL;
    }

    uv_udp_init(uvLoop, handle);

    AsyncUVUdp&      uvObject = *AsyncUVUdp::from(handle);
    SocketDescriptor descriptor;
    Result           res = AsyncUVLoop::getLoop(uvLoop).createAsyncUDPSocket(addressFamily, descriptor);
    if (not res)
    {
        return UV_EINVAL;
    }
    uvObject.descriptor = move(descriptor);
    return 0;
}

UV_API int uv_udp_open(uv_udp_t* handle, uv_os_sock_t sock)
{
    using namespace SC;
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }
    if (sock == static_cast<uv_os_sock_t>(SocketDescriptor::Invalid))
    {
        return UV_EBADF;
    }

    AsyncUVUdp& uvObject = *AsyncUVUdp::from(handle);
    if (uvObject.descriptor.isValid())
    {
        return UV_EBUSY;
    }
    if (isUdpSocketAlreadyOpen(handle, sock))
    {
        return UV_EEXIST;
    }

    SocketDescriptor descriptor;
    if (not descriptor.assign(static_cast<SocketDescriptor::Handle>(sock)))
    {
        return UV_EINVAL;
    }
    if (not descriptor.setBlocking(false))
    {
        descriptor.detach();
        return UV_EINVAL;
    }
    uvObject.descriptor = move(descriptor);
    return 0;
}

UV_API int uv_udp_bind(uv_udp_t* handle, const struct sockaddr* addr, unsigned int flags)
{
    using namespace SC;
    const unsigned int supportedFlags = UV_UDP_IPV6ONLY | UV_UDP_REUSEADDR | UV_UDP_LINUX_RECVERR;
    if ((flags & UV_UDP_REUSEPORT) != 0)
    {
        return UV_ENOTSUP;
    }
    if ((flags & ~supportedFlags) != 0)
    {
        return UV_EINVAL;
    }

    SocketIPAddress address;
    ::memcpy(&address.handle, addr, addr->sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in));
    AsyncUVUdp& uvObject = *AsyncUVUdp::from(handle);

    AsyncEventLoop&  loop = AsyncUVLoop::getLoop(handle->loop);
    SocketDescriptor descriptor;
    Result           res = Result(true);
    if (uvObject.descriptor.isValid())
    {
        SocketFlags::AddressFamily existingFamily;
        if (uvObject.descriptor.getAddressFamily(existingFamily) and existingFamily != address.getAddressFamily())
        {
            return UV_EINVAL;
        }
        descriptor = move(uvObject.descriptor);
    }
    else
    {
        res = loop.createAsyncUDPSocket(address.getAddressFamily(), descriptor);
    }
    if (not res)
    {
        return UV_EINVAL;
    }

    SocketServer::BindStatus       bindStatus   = SocketServer::BindStatus::None;
    SocketServer::BindReuseAddress reuseAddress = (flags & UV_UDP_REUSEADDR) != 0
                                                      ? SocketServer::BindReuseAddress::Enabled
                                                      : SocketServer::BindReuseAddress::Disabled;

    if (reuseAddress == SocketServer::BindReuseAddress::Enabled)
    {
#if defined(SO_REUSEPORT)
        SocketDescriptor::Handle nativeSocket;
        if (not descriptor.get(nativeSocket, Result(false)))
        {
            return UV_EINVAL;
        }
        const int enableReusePort = 1;
#if SC_PLATFORM_WINDOWS
        (void)::setsockopt(nativeSocket, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&enableReusePort),
                           static_cast<int>(sizeof(enableReusePort)));
#else
        (void)::setsockopt(nativeSocket, SOL_SOCKET, SO_REUSEPORT, &enableReusePort, sizeof(enableReusePort));
#endif
#endif
    }

    if ((flags & UV_UDP_IPV6ONLY) != 0 and address.getAddressFamily() == SocketFlags::AddressFamilyIPV6)
    {
        SocketDescriptor::Handle nativeSocket;
        if (not descriptor.get(nativeSocket, Result(false)))
        {
            return UV_EINVAL;
        }
        const int enableIpv6Only = 1;
#if SC_PLATFORM_WINDOWS
        if (::setsockopt(nativeSocket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&enableIpv6Only),
                         static_cast<int>(sizeof(enableIpv6Only))) != 0)
#else
        if (::setsockopt(nativeSocket, IPPROTO_IPV6, IPV6_V6ONLY, &enableIpv6Only, sizeof(enableIpv6Only)) != 0)
#endif
        {
            return UV_EINVAL;
        }
    }

    res = SocketServer(descriptor).bind(address, reuseAddress, &bindStatus);
    if (not res)
    {
        if (bindStatus == SocketServer::BindStatus::AddressInUse)
        {
            return UV_EADDRINUSE;
        }
        return UV_EINVAL;
    }

    uvObject.descriptor = move(descriptor);
    return 0;
}

UV_API int uv_udp_getsockname(const uv_udp_t* handle, struct sockaddr* name, int* namelen)
{
    using namespace SC;
    if (handle == nullptr or name == nullptr or namelen == nullptr)
    {
        return UV_EINVAL;
    }
    const AsyncUVUdp&        uvObject     = *AsyncUVUdp::from(const_cast<uv_udp_t*>(handle));
    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (not uvObject.descriptor.get(socketHandle, Result(false)))
    {
        return UV_EBADF;
    }
    socklen_t length = static_cast<socklen_t>(*namelen);
    if (::getsockname(socketHandle, name, &length) != 0)
    {
#if SC_PLATFORM_WINDOWS
        return AsyncUVMapSystemErrorToUv(WSAGetLastError());
#else
        return AsyncUVMapSystemErrorToUv(errno);
#endif
    }
    *namelen = static_cast<int>(length);
    return 0;
}

UV_API int uv_udp_connect(uv_udp_t* handle, const struct sockaddr* addr)
{
    using namespace SC;
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }

    AsyncUVUdp& uvObject = *AsyncUVUdp::from(handle);
    bool        connected;
    int         status = uvObject.isConnected(connected);
    if (status != 0)
    {
        return status;
    }

    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (addr == nullptr)
    {
        if (not connected)
        {
            return UV_ENOTCONN;
        }
        if (not uvObject.descriptor.get(socketHandle, Result(false)))
        {
            return UV_EBADF;
        }

#if SC_PLATFORM_WINDOWS
        struct sockaddr_storage disconnectedAddress;
        ::memset(&disconnectedAddress, 0, sizeof(disconnectedAddress));
        const int nativeStatus = ::connect(socketHandle, reinterpret_cast<const struct sockaddr*>(&disconnectedAddress),
                                           static_cast<int>(sizeof(disconnectedAddress)));
#else
        struct sockaddr disconnectedAddress;
        ::memset(&disconnectedAddress, 0, sizeof(disconnectedAddress));
        disconnectedAddress.sa_family = AF_UNSPEC;
        const int nativeStatus        = ::connect(socketHandle, &disconnectedAddress, sizeof(disconnectedAddress));
#endif
#if SC_PLATFORM_APPLE
        if (nativeStatus != 0 and (errno == EAFNOSUPPORT or errno == EINVAL))
        {
            return 0;
        }
#endif
        return nativeStatus == 0 ? 0 : getCurrentUdpError();
    }

    SocketFlags::AddressFamily addressFamily;
    if (addr->sa_family == AF_INET)
    {
        addressFamily = SocketFlags::AddressFamilyIPV4;
    }
    else if (addr->sa_family == AF_INET6)
    {
        addressFamily = SocketFlags::AddressFamilyIPV6;
    }
    else
    {
        return UV_EINVAL;
    }

    if (connected)
    {
        return UV_EISCONN;
    }

    if (not uvObject.descriptor.isValid())
    {
        AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(handle->loop);
        Result          res       = eventLoop.createAsyncUDPSocket(addressFamily, uvObject.descriptor);
        if (not res)
        {
            return UV_EINVAL;
        }
    }
    if (not uvObject.descriptor.get(socketHandle, Result(false)))
    {
        return UV_EBADF;
    }

    const socklen_t addrLength = getSocketAddressSize(addr);
#if SC_PLATFORM_WINDOWS
    const int nativeStatus = ::connect(socketHandle, addr, static_cast<int>(addrLength));
#else
    const int nativeStatus = ::connect(socketHandle, addr, addrLength);
#endif
    return nativeStatus == 0 ? 0 : getCurrentUdpError();
}

UV_API int uv_udp_getpeername(const uv_udp_t* handle, struct sockaddr* name, int* namelen)
{
    using namespace SC;
    if (handle == nullptr or name == nullptr or namelen == nullptr)
    {
        return UV_EINVAL;
    }
    const AsyncUVUdp&        uvObject     = *AsyncUVUdp::from(const_cast<uv_udp_t*>(handle));
    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (not uvObject.descriptor.get(socketHandle, Result(false)))
    {
        return UV_EBADF;
    }
    socklen_t length = static_cast<socklen_t>(*namelen);
    if (::getpeername(socketHandle, name, &length) != 0)
    {
        return getCurrentUdpError();
    }
    *namelen = static_cast<int>(length);
    return 0;
}

UV_API int uv_udp_set_broadcast(uv_udp_t* handle, int on)
{
    using namespace SC;
    SocketDescriptor::Handle socketHandle;
    const int                status = getUdpDescriptor(handle, socketHandle);
    if (status != 0)
    {
        return status;
    }
    return AsyncUVUdp::from(handle)->descriptor.setBroadcast(on != 0) ? 0 : UV_EINVAL;
}

UV_API int uv_udp_set_ttl(uv_udp_t* handle, int ttl)
{
    using namespace SC;
    SocketDescriptor::Handle   socketHandle;
    SocketFlags::AddressFamily addressFamily;
    int                        status = getUdpDescriptor(handle, socketHandle);
    if (status != 0)
    {
        return status;
    }
    status = getUdpAddressFamily(handle, addressFamily);
    if (status != 0)
    {
        if (setUnicastTtl(socketHandle, SocketFlags::AddressFamilyIPV4, ttl) == 0)
        {
            return 0;
        }
        if (setUnicastTtl(socketHandle, SocketFlags::AddressFamilyIPV6, ttl) == 0)
        {
            return 0;
        }
        return UV_EINVAL;
    }
    return setUnicastTtl(socketHandle, addressFamily, ttl);
}

UV_API int uv_udp_set_multicast_loop(uv_udp_t* handle, int on)
{
    using namespace SC;
    SocketDescriptor::Handle socketHandle;
    int                      status = getUdpDescriptor(handle, socketHandle);
    if (status != 0)
    {
        return status;
    }

    SocketFlags::AddressFamily addressFamily;
    status = getUdpAddressFamily(handle, addressFamily);
    if (status != 0)
    {
        AsyncUVUdp& uvObject = *AsyncUVUdp::from(handle);
        if (uvObject.descriptor.setMulticastLoopback(SocketFlags::AddressFamilyIPV4, on != 0))
        {
            return 0;
        }
        if (uvObject.descriptor.setMulticastLoopback(SocketFlags::AddressFamilyIPV6, on != 0))
        {
            return 0;
        }
        return UV_EINVAL;
    }
    return AsyncUVUdp::from(handle)->descriptor.setMulticastLoopback(addressFamily, on != 0) ? 0 : UV_EINVAL;
}

UV_API int uv_udp_set_multicast_ttl(uv_udp_t* handle, int ttl)
{
    using namespace SC;
    if (ttl < 0 or ttl > 255)
    {
        return UV_EINVAL;
    }
    SocketDescriptor::Handle socketHandle;
    int                      status = getUdpDescriptor(handle, socketHandle);
    if (status != 0)
    {
        return status;
    }

    SocketFlags::AddressFamily addressFamily;
    status = getUdpAddressFamily(handle, addressFamily);
    if (status != 0)
    {
        AsyncUVUdp& uvObject = *AsyncUVUdp::from(handle);
        if (uvObject.descriptor.setMulticastHops(SocketFlags::AddressFamilyIPV4, ttl))
        {
            return 0;
        }
        if (uvObject.descriptor.setMulticastHops(SocketFlags::AddressFamilyIPV6, ttl))
        {
            return 0;
        }
        return UV_EINVAL;
    }
    return AsyncUVUdp::from(handle)->descriptor.setMulticastHops(addressFamily, ttl) ? 0 : UV_EINVAL;
}

UV_API int uv_udp_set_multicast_interface(uv_udp_t* handle, const char* interface_addr)
{
    using namespace SC;
    SocketDescriptor::Handle socketHandle;
    const int                status = getUdpDescriptor(handle, socketHandle);
    if (status != 0)
    {
        return status;
    }

    SocketIPAddress interfaceAddress;
    if (interface_addr != nullptr)
    {
        if (parseUdpAddress(interface_addr, interfaceAddress) != 0)
        {
            return UV_EINVAL;
        }
    }
    else
    {
        SocketFlags::AddressFamily addressFamily;
        const int                  familyStatus = getUdpAddressFamily(handle, addressFamily);
        if (familyStatus != 0)
        {
            return familyStatus;
        }
        if (parseUdpInterfaceAddress(nullptr, addressFamily, interfaceAddress) != 0)
        {
            return UV_EINVAL;
        }
    }

    if (not AsyncUVUdp::from(handle)->descriptor.setMulticastOutboundInterface(interfaceAddress))
    {
        return UV_EINVAL;
    }
    return 0;
}

UV_API int uv_udp_set_membership(uv_udp_t* handle, const char* multicast_addr, const char* interface_addr,
                                 uv_membership membership)
{
    using namespace SC;
    if (membership != UV_JOIN_GROUP and membership != UV_LEAVE_GROUP)
    {
        return UV_EINVAL;
    }

    SocketDescriptor::Handle socketHandle;
    const int                status = getUdpDescriptor(handle, socketHandle);
    if (status != 0)
    {
        return status;
    }

    SocketIPAddress multicastAddress;
    int             parseStatus = parseUdpAddress(multicast_addr, multicastAddress);
    if (parseStatus != 0)
    {
        return parseStatus;
    }

    SocketIPAddress interfaceAddress;
    parseStatus = parseUdpInterfaceAddress(interface_addr, multicastAddress.getAddressFamily(), interfaceAddress);
    if (parseStatus != 0)
    {
        return parseStatus;
    }

    const int option = membership == UV_JOIN_GROUP ? 1 : 0;
    if (multicastAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
        mreq.imr_interface = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
#if SC_PLATFORM_WINDOWS
        const int nativeOption = option != 0 ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;
        const int result = ::setsockopt(socketHandle, IPPROTO_IP, nativeOption, reinterpret_cast<const char*>(&mreq),
                                        static_cast<int>(sizeof(mreq)));
#else
        const int nativeOption = option != 0 ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;
        const int result       = ::setsockopt(socketHandle, IPPROTO_IP, nativeOption, &mreq, sizeof(mreq));
#endif
        return result == 0 ? 0 : getCurrentUdpError();
    }

    struct ipv6_mreq mreq;
    mreq.ipv6mr_multiaddr = multicastAddress.handle.reinterpret_as<struct sockaddr_in6>().sin6_addr;
    mreq.ipv6mr_interface = 0;
#if SC_PLATFORM_WINDOWS
    const int nativeOption = option != 0 ? IPV6_ADD_MEMBERSHIP : IPV6_DROP_MEMBERSHIP;
    const int result = ::setsockopt(socketHandle, IPPROTO_IPV6, nativeOption, reinterpret_cast<const char*>(&mreq),
                                    static_cast<int>(sizeof(mreq)));
#else
    const int nativeOption = option != 0 ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP;
    const int result       = ::setsockopt(socketHandle, IPPROTO_IPV6, nativeOption, &mreq, sizeof(mreq));
#endif
    return result == 0 ? 0 : getCurrentUdpError();
}

UV_API int uv_udp_set_source_membership(uv_udp_t* handle, const char* multicast_addr, const char* interface_addr,
                                        const char* source_addr, uv_membership membership)
{
    using namespace SC;
    if (membership != UV_JOIN_GROUP and membership != UV_LEAVE_GROUP)
    {
        return UV_EINVAL;
    }

    SocketDescriptor::Handle socketHandle;
    const int                status = getUdpDescriptor(handle, socketHandle);
    if (status != 0)
    {
        return status;
    }

    SocketIPAddress multicastAddress;
    int             parseStatus = parseUdpAddress(multicast_addr, multicastAddress);
    if (parseStatus != 0)
    {
        return parseStatus;
    }

    SocketIPAddress interfaceAddress;
    parseStatus = parseUdpInterfaceAddress(interface_addr, multicastAddress.getAddressFamily(), interfaceAddress);
    if (parseStatus != 0)
    {
        return parseStatus;
    }

    SocketIPAddress sourceAddress;
    parseStatus = parseUdpAddress(source_addr, sourceAddress);
    if (parseStatus != 0 or sourceAddress.getAddressFamily() != multicastAddress.getAddressFamily())
    {
        return UV_EINVAL;
    }

    if (multicastAddress.getAddressFamily() != SocketFlags::AddressFamilyIPV4)
    {
        return UV_ENOSYS;
    }

#if defined(IP_ADD_SOURCE_MEMBERSHIP) && defined(IP_DROP_SOURCE_MEMBERSHIP)
    struct ip_mreq_source mreq;
    mreq.imr_multiaddr  = multicastAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
    mreq.imr_interface  = interfaceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;
    mreq.imr_sourceaddr = sourceAddress.handle.reinterpret_as<struct sockaddr_in>().sin_addr;

    const int option = membership == UV_JOIN_GROUP ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP;
#if SC_PLATFORM_WINDOWS
    const int result = ::setsockopt(socketHandle, IPPROTO_IP, option, reinterpret_cast<const char*>(&mreq),
                                    static_cast<int>(sizeof(mreq)));
#else
    const int result = ::setsockopt(socketHandle, IPPROTO_IP, option, &mreq, sizeof(mreq));
#endif
    return result == 0 ? 0 : getCurrentUdpError();
#else
    return UV_ENOSYS;
#endif
}

UV_API int uv_udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb)
{
    return SC::AsyncUVUdp::from(handle)->recvStart(alloc_cb, recv_cb);
}

UV_API int uv_udp_send(uv_udp_send_t* req, uv_udp_t* handle, const uv_buf_t bufs[], unsigned int nbufs,
                       const struct sockaddr* addr, uv_udp_send_cb send_cb)
{
    return SC::AsyncUVUdp::from(handle)->send(req, bufs, nbufs, addr, send_cb);
}

UV_API int uv_udp_try_send(uv_udp_t* handle, const uv_buf_t bufs[], unsigned int nbufs, const struct sockaddr* addr)
{
    return SC::AsyncUVUdp::from(handle)->trySend(bufs, nbufs, addr);
}

UV_API int uv_udp_try_send2(uv_udp_t* handle, unsigned int count, uv_buf_t* bufs[], unsigned int nbufs[],
                            struct sockaddr* addrs[], unsigned int flags)
{
    if (flags != 0 or count == 0 or bufs == nullptr or nbufs == nullptr or addrs == nullptr)
    {
        return UV_EINVAL;
    }
    for (unsigned int idx = 0; idx < count; ++idx)
    {
        const int sent = SC::AsyncUVUdp::from(handle)->trySend(bufs[idx], nbufs[idx], addrs[idx]);
        if (sent < 0)
        {
            return sent;
        }
    }
    return static_cast<int>(count);
}

UV_API int uv_udp_using_recvmmsg(const uv_udp_t*) { return 0; }

UV_API int uv_udp_recv_stop(uv_udp_t* handle) { return SC::AsyncUVUdp::from(handle)->recvStop(); }
