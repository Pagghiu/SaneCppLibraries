// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncUV.h"
#if !SC_PLATFORM_WINDOWS
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace SC
{
uv_handle_t* AsyncUVHandle::from(AsyncRequest& async)
{
    AsyncUVFlags uvFlags = async.getUserFlags();
    switch (uvFlags.getType())
    {
    case UV_ASYNC:
    case UV_SIGNAL:
    case UV_TIMER: {
        return reinterpret_cast<uv_handle_t*>(reinterpret_cast<char*>(&async) - sizeof(AsyncUVHandle));
    }
    case UV_TCP: {
        return reinterpret_cast<uv_handle_t*>(reinterpret_cast<char*>(&async) - sizeof(AsyncUVStream));
    }
    case UV_UDP: {
        return reinterpret_cast<uv_handle_t*>(reinterpret_cast<char*>(&async) - sizeof(AsyncUVHandle));
    }
    default: break;
    }
    SC_ASSERT_RELEASE(false);
    return nullptr;
}
AsyncUVHandle* AsyncUVHandle::from(uv_handle_t* handle) { return reinterpret_cast<AsyncUVHandle*>(handle); }

AsyncRequest& AsyncUVHandle::getRequest(uv_handle_t* handle)
{
    switch (handle->type)
    {
    case UV_TIMER: return AsyncUVTimer::from(reinterpret_cast<uv_timer_t*>(handle))->timeout;
    case UV_ASYNC: return AsyncUVAsync::from(reinterpret_cast<uv_async_t*>(handle))->wakeUp;
    case UV_SIGNAL: return AsyncUVSignal::from(reinterpret_cast<uv_signal_t*>(handle))->signal;
    case UV_TCP: return AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(handle))->getActiveRequest();
    case UV_UDP: return AsyncUVUdp::from(reinterpret_cast<uv_udp_t*>(handle))->getActiveRequest();
    default: break;
    }
    SC_ASSERT_RELEASE(false);
}

void AsyncUVHandle::destruct(uv_handle_t* handle)
{
    AsyncUVLoop::get(handle->loop).unregisterActiveHandle(reinterpret_cast<AsyncUVHandle*>(handle));
    switch (handle->type)
    {
    case UV_TIMER: AsyncUVTimer::from(reinterpret_cast<uv_timer_t*>(handle))->~AsyncUVTimer(); break;
    case UV_ASYNC: AsyncUVAsync::from(reinterpret_cast<uv_async_t*>(handle))->~AsyncUVAsync(); break;
    case UV_SIGNAL: AsyncUVSignal::from(reinterpret_cast<uv_signal_t*>(handle))->~AsyncUVSignal(); break;
    case UV_TCP: AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(handle))->~AsyncUVTcp(); break;
    case UV_UDP: AsyncUVUdp::from(reinterpret_cast<uv_udp_t*>(handle))->~AsyncUVUdp(); break;
    default: SC_ASSERT_RELEASE(false); break;
    }
}

AsyncUVAsync* AsyncUVAsync::from(uv_async_t* handle)
{
    SC_ASSERT_RELEASE(handle->type == UV_ASYNC);
    return reinterpret_cast<AsyncUVAsync*>(handle);
}
int AsyncUVAsync::construct(uv_loop_t* loop, uv_async_t* handle, uv_async_cb async_cb)
{
    handle->type          = UV_ASYNC;
    AsyncUVAsync* request = AsyncUVAsync::from(handle);
    placementNew(*request, loop, handle->type);
    request->wakeUp.setUserFlags(AsyncUVFlags(handle->type).getUserFlags());

    request->wakeUp.callback = [async_cb, handle](AsyncLoopWakeUp::Result& result)
    {
        if (async_cb)
        {
            async_cb(handle);
        }
    };
    if (request->wakeUp.start(AsyncUVLoop::getLoop(loop)))
    {
        AsyncUVLoop::get(loop).registerActiveHandle(request);
        return 0;
    }
    else
    {
        return -1;
    }
}

} // namespace SC

UV_API void uv_close(uv_handle_t* handle, uv_close_cb close_cb)
{
    using namespace SC;
    AsyncUVLoop::get(handle->loop).closeHandle(handle, close_cb);
}

UV_API void uv_ref(uv_handle_t* handle)
{
    using namespace SC;
    switch (handle->type)
    {
    case UV_PREPARE:
    case UV_CHECK:
    case UV_IDLE: {
        // TODO: Implement
    }
    break;
    case UV_TIMER:
    case UV_ASYNC:
    case UV_SIGNAL:
    case UV_TCP:
    case UV_UDP: {
        AsyncRequest& request = AsyncUVHandle::getRequest(handle);
        AsyncUVLoop::getLoop(handle->loop).includeInActiveCount(request);
        return;
    }
    default: break;
    }
    SC_ASSERT_RELEASE(false);
}

UV_API void uv_unref(uv_handle_t* handle)
{
    using namespace SC;

    switch (handle->type)
    {
    case UV_PREPARE:
    case UV_CHECK:
    case UV_IDLE: {
        // TODO: Implement
    }
    break;
    case UV_TIMER:
    case UV_ASYNC:
    case UV_SIGNAL:
    case UV_TCP:
    case UV_UDP: {
        AsyncRequest& request = AsyncUVHandle::getRequest(handle);
        AsyncUVLoop::getLoop(handle->loop).excludeFromActiveCount(request);
        return;
    }
    default: SC_ASSERT_RELEASE(false); break;
    }
}

UV_API int uv_has_ref(const uv_handle_t* handle)
{
    using namespace SC;

    switch (handle->type)
    {
    case UV_PREPARE:
    case UV_CHECK:
    case UV_IDLE: {
        // TODO: Implement
    }
    break;
    case UV_TIMER:
    case UV_ASYNC:
    case UV_SIGNAL:
    case UV_TCP:
    case UV_UDP: {
        const AsyncRequest& request = AsyncUVHandle::getRequest(const_cast<uv_handle_t*>(handle));
        return AsyncEventLoop::isExcludedFromActiveCount(request);
    }
    default: break;
    }

    SC_ASSERT_RELEASE(false);
    return 0;
}
int uv_timer_is_restarting(uv_timer_t* handle);

UV_API int uv_is_active(const uv_handle_t* handle)
{
    using namespace SC;
    const AsyncRequest& async = AsyncUVHandle::getRequest(const_cast<uv_handle_t*>(handle));
    if (async.isFree())
    {
        return 0;
    }
    if (uv_is_closing(handle))
    {
        return 0;
    }
    if (async.isCancelling())
    {
        if (handle->type == UV_TIMER)
        {
            return uv_timer_is_restarting((uv_timer_t*)handle);
        }
        return 0;
    }
    return 1;
}

UV_API int uv_is_closing(const uv_handle_t* handle) { return handle->close_cb ? 1 : 0; }

UV_API int uv_fileno(const uv_handle_t* handle, uv_os_fd_t* fd)
{
    using namespace SC;
    if (handle == nullptr or fd == nullptr)
    {
        return UV_EINVAL;
    }
    if (uv_is_closing(handle))
    {
        return UV_EBADF;
    }

    const SocketDescriptor* descriptor = nullptr;
    if (handle->type == UV_TCP)
    {
        descriptor = &AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(const_cast<uv_handle_t*>(handle)))->descriptor;
    }
    else if (handle->type == UV_UDP)
    {
        descriptor = &AsyncUVUdp::from(reinterpret_cast<uv_udp_t*>(const_cast<uv_handle_t*>(handle)))->descriptor;
    }
    else if (handle->type == UV_NAMED_PIPE or handle->type == UV_TTY)
    {
        if (handle->u.fd < 0)
        {
            return UV_EBADF;
        }
        *fd = static_cast<uv_os_fd_t>(handle->u.fd);
        return 0;
    }
    else
    {
        return UV_EINVAL;
    }

    SocketDescriptor::Handle socketHandle = SocketDescriptor::Invalid;
    if (not descriptor->get(socketHandle, Result(false)))
    {
        return UV_EBADF;
    }
    *fd = static_cast<uv_os_fd_t>(socketHandle);
    return 0;
}

UV_API uv_handle_type uv_guess_handle(uv_file file)
{
#if SC_PLATFORM_WINDOWS
    (void)file;
    return UV_UNKNOWN_HANDLE;
#else
    if (file < 0)
    {
        return UV_UNKNOWN_HANDLE;
    }
    if (::isatty(file))
    {
        return UV_TTY;
    }

    struct stat fileStat;
    if (::fstat(file, &fileStat) != 0)
    {
        return UV_UNKNOWN_HANDLE;
    }
    if (S_ISFIFO(fileStat.st_mode))
    {
        return UV_NAMED_PIPE;
    }
    if (S_ISSOCK(fileStat.st_mode))
    {
        int       socketType = 0;
        socklen_t length     = sizeof(socketType);
        if (::getsockopt(file, SOL_SOCKET, SO_TYPE, &socketType, &length) == 0)
        {
            return socketType == SOCK_STREAM ? UV_TCP : UV_UDP;
        }
        return UV_TCP;
    }
    return UV_FILE;
#endif
}

UV_API const char* uv_handle_type_name(uv_handle_type type)
{
    switch (type)
    {
    case UV_ASYNC: return "async";
    case UV_CHECK: return "check";
    case UV_FS_EVENT: return "fs_event";
    case UV_FS_POLL: return "fs_poll";
    case UV_HANDLE: return "handle";
    case UV_IDLE: return "idle";
    case UV_NAMED_PIPE: return "pipe";
    case UV_POLL: return "poll";
    case UV_PREPARE: return "prepare";
    case UV_PROCESS: return "process";
    case UV_STREAM: return "stream";
    case UV_TCP: return "tcp";
    case UV_TIMER: return "timer";
    case UV_TTY: return "tty";
    case UV_UDP: return "udp";
    case UV_SIGNAL: return "signal";
    case UV_FILE: return "file";
    default: return nullptr;
    }
}

UV_API size_t uv_handle_size(uv_handle_type type)
{
    switch (type)
    {
    case UV_ASYNC: return sizeof(uv_async_t);
    case UV_CHECK: return sizeof(uv_check_t);
    case UV_IDLE: return sizeof(uv_idle_t);
    case UV_NAMED_PIPE: return sizeof(uv_pipe_t);
    case UV_PREPARE: return sizeof(uv_prepare_t);
    case UV_SIGNAL: return sizeof(uv_signal_t);
    case UV_TCP: return sizeof(uv_tcp_t);
    case UV_TIMER: return sizeof(uv_timer_t);
    case UV_TTY: return sizeof(uv_tty_t);
    case UV_UDP: return sizeof(uv_udp_t);
    default: return 0;
    }
}

UV_API uv_handle_type uv_handle_get_type(const uv_handle_t* handle)
{
    return handle != nullptr ? handle->type : UV_UNKNOWN_HANDLE;
}

UV_API uv_loop_t* uv_handle_get_loop(const uv_handle_t* handle) { return handle != nullptr ? handle->loop : nullptr; }

UV_API void* uv_handle_get_data(const uv_handle_t* handle) { return handle != nullptr ? handle->data : nullptr; }

UV_API void uv_handle_set_data(uv_handle_t* handle, void* data)
{
    if (handle != nullptr)
    {
        handle->data = data;
    }
}

UV_API const char* uv_req_type_name(uv_req_type type)
{
    switch (type)
    {
    case UV_REQ: return "req";
    case UV_CONNECT: return "connect";
    case UV_WRITE: return "write";
    case UV_SHUTDOWN: return "shutdown";
    case UV_UDP_SEND: return "udp_send";
    case UV_FS: return "fs";
    case UV_WORK: return "work";
    case UV_GETADDRINFO: return "getaddrinfo";
    case UV_GETNAMEINFO: return "getnameinfo";
    case UV_RANDOM: return "random";
    default: return nullptr;
    }
}

UV_API size_t uv_req_size(uv_req_type type)
{
    switch (type)
    {
    case UV_CONNECT: return sizeof(uv_connect_t);
    case UV_WRITE: return sizeof(uv_write_t);
    case UV_SHUTDOWN: return sizeof(uv_shutdown_t);
    case UV_UDP_SEND: return sizeof(uv_udp_send_t);
    case UV_FS: return sizeof(uv_fs_t);
    case UV_WORK: return sizeof(uv_work_t);
    case UV_GETADDRINFO: return sizeof(uv_getaddrinfo_t);
    case UV_GETNAMEINFO: return sizeof(uv_getnameinfo_t);
    case UV_RANDOM: return sizeof(uv_random_t);
    case UV_REQ: return sizeof(uv_req_t);
    default: return 0;
    }
}

UV_API int uv_pipe(int fds[2], int read_flags, int write_flags)
{
    (void)read_flags;
    (void)write_flags;
    if (fds == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    return UV_ENOSYS;
#else
    return ::pipe(fds) == 0 ? 0 : UV_EINVAL;
#endif
}

UV_API int uv_pipe_init(uv_loop_t* loop, uv_pipe_t* handle, int ipc)
{
    (void)ipc;
    if (loop == nullptr or handle == nullptr)
    {
        return UV_EINVAL;
    }
    memset(handle, 0, sizeof(uv_pipe_t));
    handle->loop = loop;
    handle->type = UV_NAMED_PIPE;
    handle->u.fd = -1;
    return 0;
}

UV_API int uv_pipe_bind(uv_pipe_t* handle, const char* name)
{
    (void)name;
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    return UV_ENOSYS;
#else
    int fds[2] = {-1, -1};
    if (::pipe(fds) != 0)
    {
        return UV_EINVAL;
    }
    handle->u.fd = fds[0];
    (void)::close(fds[1]);
    return 0;
#endif
}

UV_API int uv_pipe_open(uv_pipe_t* handle, uv_file file)
{
    if (handle == nullptr or file < 0)
    {
        return UV_EINVAL;
    }
    handle->u.fd = file;
    return 0;
}

UV_API void uv_pipe_pending_instances(uv_pipe_t* handle, int count)
{
    (void)handle;
    (void)count;
}

UV_API int uv_tty_init(uv_loop_t* loop, uv_tty_t* handle, uv_file fd, int readable)
{
    if (loop == nullptr or handle == nullptr or fd < 0)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    (void)readable;
    return UV_ENOSYS;
#else
    if (::isatty(fd) == 0)
    {
        return UV_EINVAL;
    }

    int       readableFlag = readable ? 1 : 0;
    int       writableFlag = readable ? 0 : 1;
    const int statusFlags  = ::fcntl(fd, F_GETFL, 0);
    if (statusFlags >= 0)
    {
        const int accessMode = statusFlags & O_ACCMODE;
        readableFlag         = (accessMode == O_RDONLY or accessMode == O_RDWR) ? 1 : 0;
        writableFlag         = (accessMode == O_WRONLY or accessMode == O_RDWR) ? 1 : 0;
    }

    memset(handle, 0, sizeof(uv_tty_t));
    handle->loop     = loop;
    handle->type     = UV_TTY;
    handle->u.fd     = fd;
    handle->readable = readableFlag;
    handle->writable = writableFlag;
    handle->flags    = 0x100000;
    return 0;
#endif
}

UV_API int uv_tty_set_mode(uv_tty_t* handle, uv_tty_mode_t mode)
{
    if (handle == nullptr)
    {
        return UV_EINVAL;
    }
    if (mode != UV_TTY_MODE_NORMAL and mode != UV_TTY_MODE_RAW and mode != UV_TTY_MODE_IO)
    {
        return UV_EINVAL;
    }
    return 0;
}

UV_API int uv_tty_reset_mode(void) { return 0; }

UV_API int uv_tty_get_winsize(uv_tty_t* handle, int* width, int* height)
{
    if (handle == nullptr or width == nullptr or height == nullptr or handle->u.fd < 0)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    return UV_ENOSYS;
#else
    struct winsize ttySize;
    if (::ioctl(handle->u.fd, TIOCGWINSZ, &ttySize) != 0)
    {
        return SC::AsyncUVMapSystemErrorToUv(errno);
    }
    *width  = ttySize.ws_col;
    *height = ttySize.ws_row;
    return 0;
#endif
}

UV_API int uv_is_readable(const uv_stream_t* handle)
{
    using namespace SC;
    if (handle == nullptr or uv_is_closing(reinterpret_cast<const uv_handle_t*>(handle)))
    {
        return 0;
    }
    if (handle->type == UV_TTY)
    {
        const uv_tty_t* tty = reinterpret_cast<const uv_tty_t*>(handle);
        return tty->readable != 0 ? 1 : 0;
    }
    if (handle->type != UV_TCP)
    {
        return 0;
    }
    const AsyncUVTcp&  tcp   = *AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(const_cast<uv_stream_t*>(handle)));
    const AsyncUVFlags flags = tcp.getActiveRequest().getUserFlags();
    return (flags.flags & AsyncUVTcp::Flag_IsServer) == 0 ? 1 : 0;
}

UV_API int uv_is_writable(const uv_stream_t* handle)
{
    using namespace SC;
    if (handle == nullptr or uv_is_closing(reinterpret_cast<const uv_handle_t*>(handle)))
    {
        return 0;
    }
    if (handle->type == UV_TTY)
    {
        const uv_tty_t* tty = reinterpret_cast<const uv_tty_t*>(handle);
        return tty->writable != 0 ? 1 : 0;
    }
    if (handle->type != UV_TCP)
    {
        return 0;
    }
    const AsyncUVTcp&  tcp   = *AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(const_cast<uv_stream_t*>(handle)));
    const AsyncUVFlags flags = tcp.getActiveRequest().getUserFlags();
    if ((flags.flags & AsyncUVTcp::Flag_IsServer) != 0)
    {
        return 0;
    }
    return (flags.flags & AsyncUVTcp::Flag_Shutdown) == 0 ? 1 : 0;
}

//-----------------------------------------------------------------------------------------------------------
// uv_async
//-----------------------------------------------------------------------------------------------------------

UV_API int uv_async_init(uv_loop_t* uvLoop, uv_async_t* handle, uv_async_cb async_cb)
{
    using namespace SC;
    return AsyncUVAsync::construct(uvLoop, handle, async_cb);
}

UV_API int uv_async_send(uv_async_t* async)
{
    using namespace SC;
    AsyncEventLoop&  eventLoop = AsyncUVLoop::getLoop(async->loop);
    AsyncLoopWakeUp& request   = AsyncUVAsync::from(async)->wakeUp;
    return request.wakeUp(eventLoop) ? 0 : -1;
}
