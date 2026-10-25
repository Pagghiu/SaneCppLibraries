// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncUV.h"

namespace SC
{
namespace
{
int getTcpConnectStatus(AsyncUVTcp& tcp)
{
    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    if (not tcp.descriptor.get(handle, Result(false)))
    {
        return UV_ECONNREFUSED;
    }

    int socketError = 0;
#if SC_PLATFORM_WINDOWS
    int socketErrorSize = sizeof(socketError);
    if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &socketErrorSize) != 0)
#else
    socklen_t socketErrorSize = sizeof(socketError);
    if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, &socketError, &socketErrorSize) != 0)
#endif
    {
        return -1;
    }
    const int uvError = AsyncUVMapSystemErrorToUv(socketError);
    return uvError != 0 ? uvError : UV_ECONNREFUSED;
}

int bindTcpDescriptor(SocketDescriptor& descriptor, const SocketIPAddress& address, unsigned int flags)
{
    SocketDescriptor::Handle nativeSocket = SocketDescriptor::Invalid;
    if (not descriptor.get(nativeSocket, Result(false)))
    {
        return UV_EBADF;
    }

    const int reuseAddress = 1;
#if SC_PLATFORM_WINDOWS
    (void)::setsockopt(nativeSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddress),
                       sizeof(reuseAddress));
#else
    (void)::setsockopt(nativeSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));
#endif

    if (address.getAddressFamily() == SocketFlags::AddressFamilyIPV6 and (flags & UV_TCP_IPV6ONLY) != 0)
    {
        const int ipv6Only = 1;
#if SC_PLATFORM_WINDOWS
        if (::setsockopt(nativeSocket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&ipv6Only),
                         sizeof(ipv6Only)) != 0)
#else
        if (::setsockopt(nativeSocket, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6Only, sizeof(ipv6Only)) != 0)
#endif
        {
#if SC_PLATFORM_WINDOWS
            return AsyncUVMapSystemErrorToUv(WSAGetLastError());
#else
            return AsyncUVMapSystemErrorToUv(errno);
#endif
        }
    }

    const struct sockaddr* socketAddress = &address.handle.reinterpret_as<const struct sockaddr>();
    const socklen_t        addressSize   = address.sizeOfHandle();
    if (::bind(nativeSocket, socketAddress, addressSize) != 0)
    {
#if SC_PLATFORM_WINDOWS
        return AsyncUVMapSystemErrorToUv(WSAGetLastError());
#else
        return AsyncUVMapSystemErrorToUv(errno);
#endif
    }
    return 0;
}

bool isTcpDescriptorBound(const SocketDescriptor& descriptor)
{
    SocketDescriptor::Handle nativeSocket = SocketDescriptor::Invalid;
    if (not descriptor.get(nativeSocket, Result(false)))
    {
        return false;
    }

    sockaddr_storage socketAddress = {};
    socklen_t        addressSize   = sizeof(socketAddress);
    if (::getsockname(nativeSocket, reinterpret_cast<sockaddr*>(&socketAddress), &addressSize) != 0)
    {
        return false;
    }
    if (socketAddress.ss_family == AF_INET)
    {
        const sockaddr_in& address = reinterpret_cast<const sockaddr_in&>(socketAddress);
        return address.sin_port != 0;
    }
    if (socketAddress.ss_family == AF_INET6)
    {
        const sockaddr_in6& address = reinterpret_cast<const sockaddr_in6&>(socketAddress);
        return address.sin6_port != 0;
    }
    return false;
}
} // namespace

void AsyncUVTcp::makeServer()
{
    SC_ASSERT_RELEASE(isClient());
    AsyncUVFlags flags = client.asyncReceive.getUserFlags();
    client.~Client();
    placementNew(server);
    flags.flags &= ~Flag_IsClose;
    flags.flags |= Flag_IsServer;
    server.asyncAccept.setUserFlags(flags.getUserFlags());
    SC_ASSERT_RELEASE(!isClient());
}

void AsyncUVTcp::makeClose()
{
    AsyncUVFlags flags = getActiveRequest().getUserFlags();
    destroy();
    placementNew(socketClose);
    flags.flags &= ~Flag_IsServer;
    flags.flags |= Flag_IsClose;
    socketClose.asyncClose.setUserFlags(flags.getUserFlags());
}

void AsyncUVTcp::destroy()
{
    if (isClose())
    {
        socketClose.~SocketClose();
    }
    else if (isClient())
    {
        client.~Client();
    }
    else
    {
        server.~Server();
    }
}

bool AsyncUVTcp::isClient() const
{
    SC_ASSERT_RELEASE(not isClose());
    AsyncUVFlags uvFlags = client.asyncReceive.getUserFlags();
    return (uvFlags.flags & Flag_IsServer) == 0;
}

bool AsyncUVTcp::isClose() const
{
    AsyncUVFlags uvFlags = client.asyncReceive.getUserFlags();
    return (uvFlags.flags & Flag_IsClose) != 0;
}

AsyncRequest& AsyncUVTcp::getActiveRequest()
{
    if (isClose())
    {
        return socketClose.asyncClose;
    }
    if (isClient())
    {
        return client.asyncReceive;
    }
    return server.asyncAccept;
}

const AsyncRequest& AsyncUVTcp::getActiveRequest() const
{
    if (isClose())
    {
        return socketClose.asyncClose;
    }
    if (isClient())
    {
        return client.asyncReceive;
    }
    return server.asyncAccept;
}

void AsyncUVTcp::setUserFlags(uint16_t userFlags) { getActiveRequest().setUserFlags(userFlags); }

void AsyncUVTcp::setFlag(uint8_t flag, bool value)
{
    AsyncUVFlags uvFlags = getActiveRequest().getUserFlags();
    if (value)
    {
        uvFlags.flags |= flag;
    }
    else
    {
        uvFlags.flags &= ~flag;
    }
    setUserFlags(uvFlags.getUserFlags());
}

bool AsyncUVTcp::getFlag(uint8_t flag) const
{
    AsyncUVFlags uvFlags = getActiveRequest().getUserFlags();
    return uvFlags.flags & flag;
}

bool isTcpSocketAlreadyOpen(uv_tcp_t* handle, uv_os_sock_t socket)
{
    bool found = false;
    AsyncUVLoop::get(handle->loop)
        .handles.forEach(
            [handle, socket, &found](uv_handle_t& otherHandle)
            {
                if (&otherHandle == reinterpret_cast<uv_handle_t*>(handle) or otherHandle.type != UV_TCP)
                {
                    return true;
                }

                const AsyncUVTcp&        otherTcp = *AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(&otherHandle));
                SocketDescriptor::Handle otherSocket;
                if (otherTcp.descriptor.get(otherSocket, Result(false)) and
                    static_cast<uv_os_sock_t>(otherSocket) == socket)
                {
                    found = true;
                    return false;
                }
                return true;
            });
    return found;
}

void setTcpNoSigPipe(SocketDescriptor& descriptor)
{
#if defined(SO_NOSIGPIPE)
    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (descriptor.get(socketHandle, Result(false)))
    {
        int enabled = 1;
        (void)::setsockopt(socketHandle, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
    }
#else
    (void)descriptor;
#endif
}

void AsyncUVTcp::close()
{
    makeClose();
    socketClose.asyncClose.executeOn(sequence);
    // TODO: We should shutdown before closing
    SocketDescriptor::Handle handle = static_cast<SocketDescriptor::Handle>(-1);
    SC_ASSERT_RELEASE(descriptor.get(handle, Result(true)));
    descriptor.detach();
    socketClose.asyncClose.callback = [handle](AsyncResult& res) mutable
    {
        (void)detail::SocketDescriptorDefinition::releaseHandle(handle);
        AsyncUVLoop::executeCloseHandle(res);
    };
    socketClose.asyncClose.relativeTimeout = 0_ms;
    SC_ASSERT_RELEASE(socketClose.asyncClose.start(AsyncUVLoop::getLoop(loop)));
}

void AsyncUVTcp::construct(uv_loop_t* loop, uv_tcp_t* handle)
{
    handle->type             = UV_TCP;
    handle->write_queue_size = 0;
    AsyncUVTcp* request      = AsyncUVTcp::from(handle);
    placementNew(*request, loop, handle->type);
    request->setUserFlags(AsyncUVFlags(handle->type).getUserFlags());
    AsyncUVLoop::get(loop).registerActiveHandle(request);
}

AsyncUVTcp* AsyncUVTcp::from(AsyncRequest& async) { return from(AsyncUVStream::from(async)); }
AsyncUVTcp* AsyncUVTcp::from(uv_stream_t* handle)
{
    SC_ASSERT_RELEASE(handle->type == UV_TCP);
    return reinterpret_cast<AsyncUVTcp*>(handle);
}

AsyncUVTcp* AsyncUVTcp::from(uv_tcp_t* handle)
{
    SC_ASSERT_RELEASE(handle->type == UV_TCP);
    return reinterpret_cast<AsyncUVTcp*>(handle);
}

int AsyncUVTcp::readStart(uv_stream_t* stream, uv_alloc_cb allocCallback, uv_read_cb readCallback)
{
    if (isClient() == false)
        return UV_ENOTCONN;
    if (not client.asyncReceive.isFree())
        return UV_EALREADY;
    setFlag(Flag_ReadStopped, false);
    client.alloc_cb              = allocCallback;
    client.read_cb               = readCallback;
    client.asyncReceive.callback = [this](AsyncSocketReceive::Result& result)
    {
        uv_stream_t* stream = reinterpret_cast<uv_stream_t*>(this);
        if (getFlag(Flag_ReadStopped) or close_cb != nullptr)
        {
            return;
        }
        Span<char> outData;
        if (result.completionData.disconnected)
        {
            uv_buf_t eofBuf            = client.readBufferFromAlloc ? client.currentReadBuffer : uv_buf_t{nullptr, 0};
            client.currentReadBuffer   = {};
            client.readBufferFromAlloc = false;
            client.read_cb(stream, UV_EOF, &eofBuf);
        }
        else if (result.get(outData))
        {
            uv_buf_t outBuf = {};
            if (client.readBufferFromAlloc)
            {
                outBuf = uv_buf_init(outData.data(), static_cast<unsigned int>(outData.sizeInBytes()));
            }
            else
            {
                client.alloc_cb(reinterpret_cast<uv_handle_t*>(stream), 64 * 1024, &outBuf);
                if (outBuf.base == nullptr or outBuf.len < outData.sizeInBytes())
                {
                    client.read_cb(stream, UV_ENOBUFS, &outBuf);
                    return;
                }
                ::memcpy(outBuf.base, outData.data(), outData.sizeInBytes());
                outBuf.len = static_cast<unsigned int>(outData.sizeInBytes());
            }
            client.currentReadBuffer   = {};
            client.readBufferFromAlloc = false;
            client.read_cb(stream, static_cast<ssize_t>(outBuf.len), &outBuf);
            if (close_cb == nullptr and not getFlag(Flag_ReadStopped))
            {
                // Arm the next read using user-provided storage, so regular reads are not constrained
                // to the bootstrap byte buffer used only for the first activation.
                uv_buf_t nextBuf = {nullptr, 0};
                client.alloc_cb(reinterpret_cast<uv_handle_t*>(stream), 64 * 1024, &nextBuf);
                if (nextBuf.base == nullptr or nextBuf.len == 0)
                {
                    client.read_cb(stream, UV_ENOBUFS, &nextBuf);
                    return;
                }
                client.currentReadBuffer   = nextBuf;
                client.readBufferFromAlloc = true;
                result.getAsync().buffer   = {nextBuf.base, nextBuf.len};
                result.reactivateRequest(true);
            }
        }
        else
        {
            if (close_cb == nullptr)
            {
                client.read_cb(stream, UV_EINVAL, nullptr);
            }
        }
    };
    AsyncEventLoop& eventLoop  = AsyncUVLoop::getLoop(stream->loop);
    client.currentReadBuffer   = {};
    client.readBufferFromAlloc = false;
    if (not client.asyncReceive.start(eventLoop, descriptor, client.bootstrapReadBytes))
    {
        return UV_EINVAL;
    }
    return 0;
}

int AsyncUVTcp::readStop()
{
    if (isClient() == false)
        return UV_EINVAL;
    setFlag(Flag_ReadStopped, true);
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);

    Result res = client.asyncReceive.stop(eventLoop);
    if (not res)
    {
        return UV_EINVAL;
    }
    return 0;
}

int AsyncUVTcp::accept(uv_stream_t* stream)
{
    if (isClient() == true)
        return UV_EINVAL;
    SocketDescriptor clientDescriptor = move(server.acceptedClient);
    if (not clientDescriptor.isValid())
    {
        return UV_EINVAL;
    }
    AsyncUVTcp::construct(loop, reinterpret_cast<uv_tcp_t*>(stream));
    AsyncUVTcp& clientTcp = *AsyncUVTcp::from(stream);
    setTcpNoSigPipe(clientDescriptor);
    clientTcp.descriptor = move(clientDescriptor);
    if (not getFlag(AsyncUVTcp::Flag_InsideAcceptCb))
    {
        AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);
        if (not server.asyncAccept.start(eventLoop, descriptor))
        {
            return UV_EINVAL;
        }
    }
    return 0;
}

int AsyncUVTcp::listen(int backlog, uv_connection_cb cb)
{
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);
    if (isClose())
    {
        return UV_EINVAL;
    }
    if (not descriptor.isValid())
    {
        SocketDescriptor newDescriptor;
        if (not eventLoop.createAsyncTCPSocket(SocketFlags::AddressFamilyIPV4, newDescriptor))
        {
            return UV_EINVAL;
        }

        SocketIPAddress address;
        if (not address.fromAddressPort("0.0.0.0", 0))
        {
            return UV_EINVAL;
        }
        const int bindStatus = bindTcpDescriptor(newDescriptor, address, 0);
        if (bindStatus != 0)
        {
            return bindStatus;
        }
        descriptor = move(newDescriptor);
    }
    bool alreadyServer = not isClient();
    if (not alreadyServer)
    {
        makeServer();
    }

    server.asyncAccept.callback = [this, cb](AsyncSocketAccept::Result& asyncResult)
    {
        if (asyncResult.isValid() and asyncResult.moveTo(server.acceptedClient))
        {
            setFlag(AsyncUVTcp::Flag_InsideAcceptCb, true);
            cb(reinterpret_cast<uv_stream_t*>(this), 0);
            setFlag(AsyncUVTcp::Flag_InsideAcceptCb, false);
            // Reactivate if uv_accept has been called, and the server was not closed in the callback.
            if (close_cb == nullptr and not server.acceptedClient.isValid())
            {
                asyncResult.reactivateRequest(true);
            }
        }
        else
        {
            cb(reinterpret_cast<uv_stream_t*>(this), -1);
        }
    };
    Result res = SocketServer(descriptor).listen(static_cast<uint32_t>(backlog));
    if (not res)
    {
        return UV_EINVAL;
    }

    if (alreadyServer)
    {
        return 0;
    }

    res = server.asyncAccept.start(eventLoop, descriptor);
    if (not res)
    {
        return UV_EINVAL;
    }
    return 0;
}

int AsyncUVTcp::write(uv_write_t* req, const uv_buf_t bufs[], unsigned int nbufs, uv_write_cb cb)
{
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);
    if (not isClient())
    {
        return UV_EPIPE;
    }
    if (nbufs == 0)
    {
        return UV_EINVAL;
    }
    if (nbufs > AsyncUVWrite::MaxInlineBuffers)
    {
        return UV_ENOBUFS;
    }
    if (not descriptor.isValid())
    {
        return UV_EBADF;
    }
    if (activeConnect != nullptr)
    {
        AsyncUVWrite::init(loop, req, reinterpret_cast<uv_stream_t*>(this));
        if (cb != nullptr)
        {
            cb(req, UV_ECANCELED);
        }
        return 0;
    }

    AsyncUVWrite::init(loop, req, reinterpret_cast<uv_stream_t*>(this));
    AsyncUVWrite& object = AsyncUVWrite::from(req);

    object.nBuffers = nbufs;

    for (uint32_t idx = 0; idx < nbufs; ++idx)
    {
        object.buffers[idx] = {bufs[idx].base, bufs[idx].len};
    }

    object.asyncSend.callback = [req, cb](AsyncSocketSend::Result& asyncResult)
    {
        AsyncUVWrite& object = AsyncUVWrite::from(req);
        req->handle->write_queue_size -= object.pendingBytes;
        object.nBuffers     = 0;
        object.pendingBytes = 0;
        int status          = asyncResult.isValid() ? 0 : -1;
        if (cb != nullptr)
        {
            cb(req, status);
        }
    };
    object.asyncSend.executeOn(sequence);

    Result res = object.asyncSend.start(eventLoop, descriptor, {object.buffers, object.nBuffers});
    if (not res)
    {
        return UV_EINVAL;
    }
    unsigned int pendingBytes = 0;
    for (uint32_t idx = 0; idx < nbufs; ++idx)
    {
        pendingBytes += bufs[idx].len;
    }
    object.pendingBytes = pendingBytes;
    reinterpret_cast<uv_stream_t*>(this)->write_queue_size += pendingBytes;
    return 0;
}

int AsyncUVTcp::connect(uv_connect_t* req, const struct sockaddr* addr, uv_connect_cb cb)
{
    if (activeConnect)
    {
        return UV_EINVAL;
    }
    SocketIPAddress address;
    ::memcpy(&address.handle, addr, addr->sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in));
    ::memset(req, 0, sizeof(uv_connect_t));
    req->type   = UV_CONNECT;
    req->cb     = cb; // not needed but why not
    req->handle = reinterpret_cast<uv_stream_t*>(this);

    AsyncUVConnect::init(loop, req);
    AsyncUVConnect& connect = AsyncUVConnect::from(req);

    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(loop);

    if (not address.isValid())
    {
        return UV_EINVAL;
    }

    SocketDescriptor newDescriptor;
    if (descriptor.isValid())
    {
        SocketFlags::AddressFamily existingFamily;
        if (not descriptor.getAddressFamily(existingFamily) or existingFamily != address.getAddressFamily())
        {
            return UV_EINVAL;
        }
        newDescriptor = move(descriptor);
    }
    else
    {
        Result res = eventLoop.createAsyncTCPSocket(address.getAddressFamily(), newDescriptor);
        if (not res)
        {
            return UV_EINVAL;
        }
        if (getFlag(Flag_NoDelay) and not newDescriptor.setTcpNoDelay(true))
        {
            return UV_EINVAL;
        }
    }
    setTcpNoSigPipe(newDescriptor);
    connect.asyncConnect.executeOn(sequence);
    bool   synchronousResult = false;
    Result syncResult(true);
    connect.asyncConnect.callback = [&](AsyncSocketConnect::Result& asyncResult)
    {
        synchronousResult = true;
        syncResult        = asyncResult.isValid();
    };
    Result res = newDescriptor.get(connect.asyncConnect.handle, Result(false));
    if (not res)
    {
        return UV_EINVAL;
    }
    activeConnect                = &connect;
    connect.asyncConnect.address = address;

    res = eventLoop.start(connect.asyncConnect);
    if (not res)
    {
        activeConnect = nullptr;
        return UV_EINVAL;
    }
    if (synchronousResult)
    {
        activeConnect = nullptr;
        if (syncResult)
        {
            descriptor = move(newDescriptor);
            // TODO: This should be called on next run loop
            cb(req, 0);
        }
        else
        {
            return UV_EINVAL;
        }
    }
    else
    {
        descriptor                    = move(newDescriptor);
        connect.asyncConnect.callback = [cb, req](AsyncSocketConnect::Result& asyncResult)
        {
            AsyncUVTcp& tcp   = *AsyncUVTcp::from(req->handle);
            tcp.activeConnect = nullptr;
            if (asyncResult.isValid())
            {
                cb(req, 0);
            }
            else
            {
                cb(req, getTcpConnectStatus(tcp));
            }
        };
    }
    return 0;
}

int AsyncUVTcp::setNoDelay(int enable)
{
    if (not descriptor.isValid())
    {
        setFlag(Flag_NoDelay, enable != 0);
        return 0;
    }
    if (not descriptor.setTcpNoDelay(enable != 0))
    {
        return UV_EINVAL;
    }
    setFlag(Flag_NoDelay, enable != 0);
    return 0;
}

int AsyncUVTcp::open(uv_os_sock_t socket)
{
    if (socket == static_cast<uv_os_sock_t>(SocketDescriptor::Invalid))
    {
        return UV_EBADF;
    }
    if (descriptor.isValid())
    {
        return UV_EBUSY;
    }
    if (isTcpSocketAlreadyOpen(reinterpret_cast<uv_tcp_t*>(this), socket))
    {
        return UV_EEXIST;
    }

    SocketDescriptor newDescriptor;
    if (not newDescriptor.assign(static_cast<SocketDescriptor::Handle>(socket)))
    {
        return UV_EINVAL;
    }
    if (not newDescriptor.setBlocking(false))
    {
        newDescriptor.detach();
        return UV_EINVAL;
    }
    setTcpNoSigPipe(newDescriptor);
    descriptor = move(newDescriptor);
    return 0;
}

int AsyncUVTcp::shutdown(uv_shutdown_t* req, uv_shutdown_cb cb)
{
    if (not isClient())
    {
        return UV_ENOTCONN;
    }
    if (cb == nullptr)
    {
        return UV_EINVAL;
    }
    if (getFlag(Flag_Shutdown))
    {
        return UV_EALREADY;
    }

    SocketDescriptor::Handle nativeSocket = SocketDescriptor::Invalid;
    if (not descriptor.get(nativeSocket, Result(false)))
    {
        return UV_EBADF;
    }

#if SC_PLATFORM_WINDOWS
    if (::shutdown(nativeSocket, SD_SEND) != 0)
    {
        return AsyncUVMapSystemErrorToUv(WSAGetLastError());
    }
#else
    if (::shutdown(nativeSocket, SHUT_WR) != 0)
    {
        return AsyncUVMapSystemErrorToUv(errno);
    }
#endif

    AsyncUVShutdown::init(loop, req, reinterpret_cast<uv_stream_t*>(this));
    req->cb = cb;
    setFlag(Flag_Shutdown, true);

    AsyncUVShutdown& shutdownRequest              = AsyncUVShutdown::from(req);
    shutdownRequest.asyncShutdown.relativeTimeout = 0_ms;
    shutdownRequest.asyncShutdown.callback        = [req](AsyncLoopTimeout::Result&) { req->cb(req, 0); };
    return shutdownRequest.asyncShutdown.start(AsyncUVLoop::getLoop(loop)) ? 0 : UV_EINVAL;
}

} // namespace SC

UV_API int uv_tcp_init(uv_loop_t* uvLoop, uv_tcp_t* handle)
{
    using namespace SC;
    AsyncUVTcp::construct(uvLoop, handle);
    return 0;
}

UV_API int uv_tcp_init_ex(uv_loop_t* uvLoop, uv_tcp_t* handle, unsigned int flags)
{
    using namespace SC;
    SocketFlags::AddressFamily addressFamily;
    switch (flags)
    {
    case AF_INET: addressFamily = SocketFlags::AddressFamilyIPV4; break;
    case AF_INET6: addressFamily = SocketFlags::AddressFamilyIPV6; break;
    default: return UV_EINVAL;
    }

    uv_tcp_init(uvLoop, handle);

    AsyncUVTcp&      uvObject = *AsyncUVTcp::from(handle);
    SocketDescriptor descriptor;
    Result           res = AsyncUVLoop::getLoop(uvLoop).createAsyncTCPSocket(addressFamily, descriptor);
    if (not res)
    {
        return UV_EINVAL;
    }
    uvObject.descriptor = move(descriptor);
    return 0;
}

UV_API int uv_tcp_bind(uv_tcp_t* handle, const struct sockaddr* addr, unsigned int flags)
{
    using namespace SC;
    if (handle == nullptr or addr == nullptr or uv_is_closing(reinterpret_cast<const uv_handle_t*>(handle)))
    {
        return UV_EINVAL;
    }
    if (addr->sa_family != AF_INET and addr->sa_family != AF_INET6)
    {
        return UV_EINVAL;
    }
    if ((flags & ~(UV_TCP_IPV6ONLY | UV_TCP_REUSEPORT)) != 0)
    {
        return UV_EINVAL;
    }
    if ((flags & UV_TCP_IPV6ONLY) != 0 and addr->sa_family != AF_INET6)
    {
        return UV_EINVAL;
    }
#if !SC_PLATFORM_LINUX
    if ((flags & UV_TCP_REUSEPORT) != 0)
    {
        return UV_ENOTSUP;
    }
#else
    (void)flags;
#endif
    SocketIPAddress address;
    ::memcpy(&address.handle, addr, addr->sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in));

#ifdef SIN6_LEN
    if (address.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
    {
        address.handle.reinterpret_as<sockaddr_in>().sin_len = sizeof(sockaddr_in);
    }
    else
    {
        address.handle.reinterpret_as<sockaddr_in6>().sin6_len = sizeof(sockaddr_in6);
    }
#endif

    AsyncUVTcp& uvObject = *AsyncUVTcp::from(handle);

    AsyncEventLoop&  loop = AsyncUVLoop::getLoop(handle->loop);
    SocketDescriptor descriptor;
    Result           res = Result(true);
    if (uvObject.descriptor.isValid())
    {
        SocketFlags::AddressFamily existingFamily;
        if (not uvObject.descriptor.getAddressFamily(existingFamily))
        {
            return UV_EINVAL;
        }
        if (existingFamily != address.getAddressFamily())
        {
            return UV_EINVAL;
        }
        if (isTcpDescriptorBound(uvObject.descriptor))
        {
            return UV_EINVAL;
        }
        descriptor = move(uvObject.descriptor);
    }
    else
    {
        res = loop.createAsyncTCPSocket(address.getAddressFamily(), descriptor);
    }
    if (not res)
    {
        return UV_EINVAL;
    }

    const int bindStatus = bindTcpDescriptor(descriptor, address, flags);
    if (bindStatus != 0)
    {
        return bindStatus;
    }
    uvObject.descriptor = move(descriptor);
    return 0;
}

UV_API int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle, const struct sockaddr* addr, uv_connect_cb cb)
{
    using namespace SC;
    return AsyncUVTcp::from(handle)->connect(req, addr, cb);
}

UV_API int uv_tcp_open(uv_tcp_t* handle, uv_os_sock_t sock)
{
    using namespace SC;
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }
    return AsyncUVTcp::from(handle)->open(sock);
}

UV_API int uv_tcp_nodelay(uv_tcp_t* handle, int enable)
{
    using namespace SC;
    return AsyncUVTcp::from(handle)->setNoDelay(enable);
}

UV_API int uv_tcp_keepalive(uv_tcp_t* handle, int enable, unsigned int delay)
{
    using namespace SC;
    if (enable != 0 and delay == 0)
    {
        return UV_EINVAL;
    }

    AsyncUVTcp& uvObject = *AsyncUVTcp::from(handle);
    if (not uvObject.descriptor.isValid())
    {
        return UV_EBADF;
    }

    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (not uvObject.descriptor.get(socketHandle, Result(false)))
    {
        return UV_EBADF;
    }

    int active = enable != 0 ? 1 : 0;
#if SC_PLATFORM_WINDOWS
    if (::setsockopt(socketHandle, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&active), sizeof(active)) !=
        0)
#else
    if (::setsockopt(socketHandle, SOL_SOCKET, SO_KEEPALIVE, &active, sizeof(active)) != 0)
#endif
    {
        return UV_EINVAL;
    }
    return 0;
}

UV_API int uv_tcp_close_reset(uv_tcp_t* handle, uv_close_cb close_cb)
{
    using namespace SC;
    if (handle == nullptr or uv_is_closing(reinterpret_cast<const uv_handle_t*>(handle)))
    {
        return UV_EINVAL;
    }

    AsyncUVTcp& uvObject = *AsyncUVTcp::from(handle);
    if (not uvObject.descriptor.isValid())
    {
        return UV_ENOTCONN;
    }

    const AsyncUVFlags flags = uvObject.getActiveRequest().getUserFlags();
    if ((flags.flags & AsyncUVTcp::Flag_IsServer) != 0)
    {
        return UV_ENOTCONN;
    }
    if ((flags.flags & AsyncUVTcp::Flag_Shutdown) != 0)
    {
        return UV_EINVAL;
    }

    SocketDescriptor::Handle nativeSocket = SocketDescriptor::Invalid;
    if (not uvObject.descriptor.get(nativeSocket, Result(false)))
    {
        return UV_ENOTCONN;
    }

    struct linger lingerOption = {};
    lingerOption.l_onoff       = 1;
    lingerOption.l_linger      = 0;
#if SC_PLATFORM_WINDOWS
    if (::setsockopt(nativeSocket, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&lingerOption),
                     sizeof(lingerOption)) != 0)
#else
    if (::setsockopt(nativeSocket, SOL_SOCKET, SO_LINGER, &lingerOption, sizeof(lingerOption)) != 0)
#endif
    {
#if SC_PLATFORM_WINDOWS
        return AsyncUVMapSystemErrorToUv(WSAGetLastError());
#else
        return AsyncUVMapSystemErrorToUv(errno);
#endif
    }
    uv_close(reinterpret_cast<uv_handle_t*>(handle), close_cb);
    return 0;
}

UV_API int uv_tcp_getsockname(const uv_tcp_t* handle, struct sockaddr* name, int* namelen)
{
    using namespace SC;
    if (handle == nullptr or name == nullptr or namelen == nullptr)
    {
        return UV_EINVAL;
    }
    const AsyncUVTcp&        uvObject     = *AsyncUVTcp::from(const_cast<uv_tcp_t*>(handle));
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

UV_API int uv_tcp_getpeername(const uv_tcp_t* handle, struct sockaddr* name, int* namelen)
{
    using namespace SC;
    if (handle == nullptr or name == nullptr or namelen == nullptr)
    {
        return UV_EINVAL;
    }
    const AsyncUVTcp&        uvObject     = *AsyncUVTcp::from(const_cast<uv_tcp_t*>(handle));
    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (not uvObject.descriptor.get(socketHandle, Result(false)))
    {
        return UV_EBADF;
    }
    socklen_t length = static_cast<socklen_t>(*namelen);
    if (::getpeername(socketHandle, name, &length) != 0)
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
