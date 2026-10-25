// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../../../Libraries/Socket/Socket.h"
#include "AsyncUV.h"

namespace SC
{
uv_stream_t* AsyncUVStream::from(AsyncRequest& async)
{
    AsyncUVFlags uvFlags = async.getUserFlags();
    switch (uvFlags.getType())
    {
    case UV_TCP: {
        return reinterpret_cast<uv_stream_t*>(reinterpret_cast<char*>(&async) - sizeof(AsyncUVStream));
    }
    default: break;
    }
    SC_ASSERT_RELEASE(false);
    return nullptr;
}

void AsyncUVWrite::init(uv_loop_t* loop, uv_write_t* req, uv_stream_t* handle)
{
    ::memset(req, 0, sizeof(uv_write_t));
    req->type   = UV_WRITE;
    req->handle = handle;
    placementNew(from(req));
}

AsyncUVWrite& AsyncUVWrite::from(uv_write_t* req) { return *reinterpret_cast<AsyncUVWrite*>(req->sc_opaque); }

void AsyncUVConnect::init(uv_loop_t* loop, uv_connect_t* req) { placementNew(from(req)); }

AsyncUVConnect& AsyncUVConnect::from(uv_connect_t* req) { return *reinterpret_cast<AsyncUVConnect*>(req->sc_opaque); }

void AsyncUVShutdown::init(uv_loop_t* loop, uv_shutdown_t* req, uv_stream_t* handle)
{
    ::memset(req, 0, sizeof(uv_shutdown_t));
    req->type   = UV_SHUTDOWN;
    req->handle = handle;
    placementNew(from(req));
}

AsyncUVShutdown& AsyncUVShutdown::from(uv_shutdown_t* req)
{
    return *reinterpret_cast<AsyncUVShutdown*>(req->sc_opaque);
}

} // namespace SC

UV_API int uv_write(uv_write_t* req, uv_stream_t* handle, const uv_buf_t bufs[], unsigned int nbufs, uv_write_cb cb)
{
    using namespace SC;
    if (req == nullptr or handle == nullptr)
    {
        return UV_EINVAL;
    }
    if (uv_is_closing(reinterpret_cast<const uv_handle_t*>(handle)))
    {
        return UV_ENOTCONN;
    }
    switch (handle->type)
    {
    case UV_TCP: return AsyncUVTcp::from(handle)->write(req, bufs, nbufs, cb);
    default: return UV_EINVAL;
    }
}

UV_API int uv_shutdown(uv_shutdown_t* req, uv_stream_t* handle, uv_shutdown_cb cb)
{
    using namespace SC;
    if (req == nullptr or handle == nullptr)
    {
        return UV_EINVAL;
    }
    if (uv_is_closing(reinterpret_cast<const uv_handle_t*>(handle)))
    {
        return UV_ENOTCONN;
    }
    switch (handle->type)
    {
    case UV_TCP: return AsyncUVTcp::from(handle)->shutdown(req, cb);
    default: return UV_EINVAL;
    }
}

UV_API int uv_try_write(uv_stream_t* stream, const uv_buf_t bufs[], unsigned int nbufs)
{
    using namespace SC;
    if (stream == nullptr or bufs == nullptr or nbufs == 0)
    {
        return UV_EINVAL;
    }
    if (stream->type != UV_TCP)
    {
        return UV_EINVAL;
    }
    if (stream->write_queue_size != 0)
    {
        return UV_EAGAIN;
    }

    unsigned int pendingBytes = 0;
    for (unsigned int idx = 0; idx < nbufs; ++idx)
    {
        if (bufs[idx].base == nullptr and bufs[idx].len > 0)
        {
            return UV_EINVAL;
        }
        if (bufs[idx].len > 65536 or pendingBytes > 65536 - bufs[idx].len)
        {
            return UV_ENOBUFS;
        }
        pendingBytes += bufs[idx].len;
    }
    if (pendingBytes == 0)
    {
        return 0;
    }

    char        storage[65536];
    const char* sendData = bufs[0].base;
    if (nbufs != 1)
    {
        unsigned int offset = 0;
        for (unsigned int idx = 0; idx < nbufs; ++idx)
        {
            if (bufs[idx].len > 0)
            {
                ::memcpy(storage + offset, bufs[idx].base, bufs[idx].len);
            }
            offset += bufs[idx].len;
        }
        sendData = storage;
    }

    SocketDescriptor::Handle nativeSocket;
    if (not AsyncUVTcp::from(stream)->descriptor.get(nativeSocket, Result(false)))
    {
        return UV_EBADF;
    }

#if SC_PLATFORM_WINDOWS
    const int sent = ::send(nativeSocket, sendData, static_cast<int>(pendingBytes), 0);
    return sent < 0 ? AsyncUVMapSystemErrorToUv(WSAGetLastError()) : sent;
#else
    const ssize_t sent = ::send(nativeSocket, sendData, pendingBytes, 0);
    return sent < 0 ? AsyncUVMapSystemErrorToUv(errno) : static_cast<int>(sent);
#endif
}

UV_API int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb)
{
    using namespace SC;
    switch (stream->type)
    {
    case UV_TCP: return AsyncUVTcp::from(stream)->listen(backlog, cb);
    case UV_NAMED_PIPE: return stream->u.fd >= 0 ? 0 : UV_EBADF;
    default: return UV_EINVAL;
    }
}

UV_API int uv_accept(uv_stream_t* server, uv_stream_t* client)
{
    using namespace SC;
    switch (server->type)
    {
    case UV_TCP: return AsyncUVTcp::from(server)->accept(client); break;
    default: return UV_EINVAL;
    }
}

UV_API int uv_read_start(uv_stream_t* stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb)
{
    using namespace SC;
    if (stream == nullptr or alloc_cb == nullptr or read_cb == nullptr)
    {
        return UV_EINVAL;
    }
    switch (stream->type)
    {
    case UV_TCP: return AsyncUVTcp::from(stream)->readStart(stream, alloc_cb, read_cb);
    default: return UV_EINVAL;
    }
}

UV_API int uv_read_stop(uv_stream_t* stream)
{
    using namespace SC;
    switch (stream->type)
    {
    case UV_TCP: return AsyncUVTcp::from(stream)->readStop(); break;
    default: return UV_EINVAL;
    }
}

UV_API size_t uv_stream_get_write_queue_size(const uv_stream_t* stream) { return stream->write_queue_size; }

UV_API int uv_stream_set_blocking(uv_stream_t* stream, int blocking)
{
    using namespace SC;
    if (stream == nullptr)
    {
        return UV_EINVAL;
    }
    if (stream->type != UV_TCP)
    {
        return UV_EINVAL;
    }

    AsyncUVTcp& uvObject = *AsyncUVTcp::from(stream);
    if (not uvObject.descriptor.isValid())
    {
        return UV_EBADF;
    }
    return uvObject.descriptor.setBlocking(blocking != 0) ? 0 : UV_EINVAL;
}

UV_API uv_buf_t uv_buf_init(char* base, unsigned int len)
{
    uv_buf_t buf;
    buf.base = base;
    buf.len  = len;
    return buf;
}
