// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#ifndef UV_API
#define UV_API extern "C"
#endif

#include "../../../../Libraries/Async/Async.h"
#include "../../../../Libraries/Common/CompilerMacrosExport.h"
#include "LinkedListQueue.h"
#include "uv.h"

#ifndef SC_EXPORT_SIDEPROJECT_ASYNC_UV
#define SC_EXPORT_SIDEPROJECT_ASYNC_UV 0
#endif
#define SC_ASYNC_UV_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_SIDEPROJECT_ASYNC_UV)

namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(AsyncUVAssert, SC_ASYNC_UV_EXPORT);

#define SC_ASYNC_UV_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::AsyncUVAssert, e)
#define SC_ASYNC_UV_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::AsyncUVAssert, e)
#define SC_ASYNC_UV_TRUST_RESULT(expression) SC_ASYNC_UV_ASSERT_RELEASE(expression)

#ifdef SC_ASSERT_RELEASE
#undef SC_ASSERT_RELEASE
#endif
#ifdef SC_ASSERT_DEBUG
#undef SC_ASSERT_DEBUG
#endif
#ifdef SC_TRUST_RESULT
#undef SC_TRUST_RESULT
#endif
#define SC_ASSERT_RELEASE(e)        SC_ASYNC_UV_ASSERT_RELEASE(e)
#define SC_ASSERT_DEBUG(e)          SC_ASYNC_UV_ASSERT_DEBUG(e)
#define SC_TRUST_RESULT(expression) SC_ASYNC_UV_TRUST_RESULT(expression)

inline int AsyncUVMapSystemErrorToUv(int systemError)
{
    if (systemError == 0)
    {
        return 0;
    }
#if SC_PLATFORM_WINDOWS
    if (systemError == WSAEADDRNOTAVAIL)
    {
        return UV_EADDRNOTAVAIL;
    }
    if (systemError == WSAEACCES)
    {
        return UV_EACCES;
    }
    if (systemError == WSAEWOULDBLOCK)
    {
        return UV_EAGAIN;
    }
    if (systemError == WSAEINVAL)
    {
        return UV_EINVAL;
    }
    if (systemError == WSAECONNREFUSED)
    {
        return UV_ECONNREFUSED;
    }
    if (systemError == WSAECONNRESET)
    {
        return UV_ECONNRESET;
    }
    if (systemError == WSAEDESTADDRREQ)
    {
        return UV_EDESTADDRREQ;
    }
    if (systemError == WSAEHOSTUNREACH)
    {
        return UV_EHOSTUNREACH;
    }
    if (systemError == WSAEISCONN)
    {
        return UV_EISCONN;
    }
    if (systemError == WSAEMSGSIZE)
    {
        return UV_EMSGSIZE;
    }
    if (systemError == WSAENETUNREACH)
    {
        return UV_ENETUNREACH;
    }
    if (systemError == WSAENOTCONN)
    {
        return UV_ENOTCONN;
    }
#else
    if (systemError == EADDRNOTAVAIL)
    {
        return UV_EADDRNOTAVAIL;
    }
    if (systemError == EACCES)
    {
        return UV_EACCES;
    }
    if (systemError == EAGAIN)
    {
        return UV_EAGAIN;
    }
    if (systemError == EBUSY)
    {
        return UV_EBUSY;
    }
    if (systemError == ECONNREFUSED)
    {
        return UV_ECONNREFUSED;
    }
    if (systemError == ECONNRESET)
    {
        return UV_ECONNRESET;
    }
    if (systemError == EDESTADDRREQ)
    {
        return UV_EDESTADDRREQ;
    }
    if (systemError == EHOSTUNREACH)
    {
        return UV_EHOSTUNREACH;
    }
    if (systemError == EISCONN)
    {
        return UV_EISCONN;
    }
    if (systemError == EMSGSIZE)
    {
        return UV_EMSGSIZE;
    }
    if (systemError == ENETUNREACH)
    {
        return UV_ENETUNREACH;
    }
    if (systemError == ENODEV)
    {
        return UV_ENODEV;
    }
    if (systemError == ENOEXEC)
    {
        return UV_ENOEXEC;
    }
    if (systemError == ENOTCONN)
    {
        return UV_ENOTCONN;
    }
#endif
    return -1;
}

struct AsyncUVFlags
{
    uint8_t handleType = UV_UNKNOWN_HANDLE;
    uint8_t flags      = 0;

    AsyncUVFlags(uv_handle_type type) { handleType = type; }
    AsyncUVFlags(uint16_t userFlags) { ::memcpy(this, &userFlags, sizeof(uint16_t)); }

    uint16_t getUserFlags() const
    {
        uint16_t userFlags;
        ::memcpy(&userFlags, this, sizeof(uint16_t));
        return userFlags;
    }

    uv_handle_type getType() const { return static_cast<uv_handle_type>(handleType); }
};

// Layout of this structure corresponds to first fields of uv_handle_t
struct AsyncUVHandle
{
    // public
    void* data;
    // read-only
    uv_loop_t*     loop;
    uv_handle_type type;
    // private
    uv_close_cb      close_cb;
    struct uv__queue handle_queue;
    union
    {
        int   fd;
        void* reserved[4];
    } u;

    AsyncUVHandle(uv_loop_t* loop, uv_handle_type type) : data(nullptr), loop(loop), type(type), close_cb(nullptr) {}

    static AsyncUVHandle* from(uv_handle_t* handle);
    static uv_handle_t*   from(AsyncRequest& request);

    static void destruct(uv_handle_t* handle);

    static AsyncRequest& getRequest(uv_handle_t* handle);
};

struct AsyncUVTimer : public AsyncUVHandle
{
    using AsyncUVHandle::AsyncUVHandle;
    AsyncLoopTimeout timeout;

    static AsyncUVTimer* from(uv_timer_t* handle);
    static uv_timer_t*   from(AsyncRequest& request);

    static void construct(uv_loop_t* loop, uv_timer_t* handle);

    int start(uv_timer_cb cb, uint64_t timeout, uint64_t repeat);
    int stop();
    int isRestarting() const;
    struct RepeatCallback;

  private:
    void restart(uv_timer_cb cb, uint64_t repeat);
};
static_assert(sizeof(AsyncUVTimer) <= sizeof(uv_timer_t), "timer");

struct AsyncUVAsync : public AsyncUVHandle
{
    using AsyncUVHandle::AsyncUVHandle;

    AsyncLoopWakeUp wakeUp;

    static AsyncUVAsync* from(uv_async_t* handle);

    static int construct(uv_loop_t* loop, uv_async_t* handle, uv_async_cb async_cb);
};
static_assert(sizeof(AsyncUVAsync) <= sizeof(uv_async_t), "async");

struct AsyncUVSignal : public AsyncUVHandle
{
    using AsyncUVHandle::AsyncUVHandle;

    uv_signal_cb signal_cb = nullptr;
    int          signum    = 0;

    AsyncSignal signal;

    static AsyncUVSignal* from(uv_signal_t* handle);

    static void construct(uv_loop_t* loop, uv_signal_t* handle);

    int start(uv_signal_cb cb, int signum, bool oneShot);
    int stop();
};
static_assert(sizeof(AsyncUVSignal) <= sizeof(uv_signal_t), "signal");

struct AsyncUVStream : public AsyncUVHandle
{
    using AsyncUVHandle::AsyncUVHandle;

    size_t      write_queue_size = 0;
    uv_alloc_cb alloc_cb         = nullptr;
    uv_read_cb  read_cb          = nullptr;

    static uv_stream_t* from(AsyncRequest& async);
};

struct AsyncUVConnect;
struct AsyncUVShutdown;

struct AsyncUVTcp : public AsyncUVStream
{
    static constexpr uint8_t Flag_IsServer       = 1 << 0;
    static constexpr uint8_t Flag_IsClose        = 1 << 1;
    static constexpr uint8_t Flag_InsideAcceptCb = 1 << 2;
    static constexpr uint8_t Flag_ReadStopped    = 1 << 3;
    static constexpr uint8_t Flag_NoDelay        = 1 << 4;
    static constexpr uint8_t Flag_Shutdown       = 1 << 5;

    struct Client
    {
        SC::AsyncSocketReceive asyncReceive;

        uv_alloc_cb alloc_cb = nullptr;
        uv_read_cb  read_cb  = nullptr;
        char        bootstrapReadBytes[512];
        uv_buf_t    currentReadBuffer   = {};
        bool        readBufferFromAlloc = false;
    };
    struct Server
    {
        SC::AsyncSocketAccept asyncAccept;
        SC::SocketDescriptor  acceptedClient;
    };
    struct SocketClose
    {
        SC::AsyncLoopTimeout asyncClose;
    };
    union
    {
        Client      client = {};
        Server      server;
        SocketClose socketClose;
    };
    SC::SocketDescriptor descriptor;
    SC::AsyncSequence    sequence;
    AsyncUVConnect*      activeConnect = nullptr;

    AsyncUVTcp(uv_loop_t* loop, uv_handle_type type) : AsyncUVStream(loop, type) {}

    ~AsyncUVTcp() { destroy(); }

    static AsyncUVTcp* from(AsyncRequest& async);
    static AsyncUVTcp* from(uv_stream_t* handle);
    static AsyncUVTcp* from(uv_tcp_t* handle);

    static void construct(uv_loop_t* loop, uv_tcp_t* handle);

    AsyncRequest&       getActiveRequest();
    const AsyncRequest& getActiveRequest() const;

    // Operations
    int readStart(uv_stream_t* stream, uv_alloc_cb allocCallback, uv_read_cb readCallback);
    int readStop();
    int accept(uv_stream_t* client);
    int listen(int backlog, uv_connection_cb cb);
    int write(uv_write_t* req, const uv_buf_t bufs[], unsigned int nbufs, uv_write_cb cb);
    int connect(uv_connect_t* req, const struct sockaddr* addr, uv_connect_cb cb);
    int setNoDelay(int enable);
    int open(uv_os_sock_t socket);
    int shutdown(uv_shutdown_t* req, uv_shutdown_cb cb);

    void close();

  private:
    void setUserFlags(uint16_t userFlags);
    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;
    void makeServer();
    void makeClose();
    bool isClient() const;
    bool isClose() const;
    void destroy();
};
static_assert(sizeof(AsyncUVTcp) <= sizeof(uv_tcp_t), "tcp");

struct AsyncUVUdp : public AsyncUVHandle
{
    using AsyncUVHandle::AsyncUVHandle;

    size_t send_queue_size = 0;

    static uv_udp_t*   from(AsyncRequest& async);
    static AsyncUVUdp* from(uv_udp_t* handle);

    static void construct(uv_loop_t* loop, uv_udp_t* handle);

    AsyncRequest& getActiveRequest();

    int  recvStart(uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb);
    int  recvStop();
    int  send(uv_udp_send_t* req, const uv_buf_t bufs[], unsigned int nbufs, const struct sockaddr* addr,
              uv_udp_send_cb send_cb);
    int  trySend(const uv_buf_t bufs[], unsigned int nbufs, const struct sockaddr* addr);
    int  isConnected(bool& connected) const;
    void close(AsyncEventLoop* eventLoopOverride = nullptr);

    AsyncSocketReceiveFrom asyncReceive;
    AsyncLoopTimeout       asyncClose;
    SocketDescriptor       descriptor;
    AsyncSequence          sequence;

    uv_alloc_cb    allocCallback = nullptr;
    uv_udp_recv_cb recvCallback  = nullptr;

    unsigned int pendingAsyncSends    = 0;
    unsigned int pendingSendCallbacks = 0;

    bool closePending = false;
};
static_assert(sizeof(AsyncUVUdp) < sizeof(uv_udp_t), "uv_udp_t");

struct AsyncUVWrite
{
    static constexpr unsigned int MaxInlineBuffers = 8;

    AsyncSocketSend  asyncSend;
    Span<const char> buffers[MaxInlineBuffers] = {};
    unsigned int     nBuffers                  = 0;
    unsigned int     pendingBytes              = 0;

    AsyncUVWrite()  = default;
    ~AsyncUVWrite() = default;

    AsyncUVWrite(const AsyncUVWrite&)            = delete;
    AsyncUVWrite& operator=(const AsyncUVWrite&) = delete;

    static void init(uv_loop_t* loop, uv_write_t* req, uv_stream_t* handle);

    static AsyncUVWrite& from(uv_write_t* req);
};
static_assert(sizeof(AsyncUVWrite) <= sizeof(((uv_write_t*)nullptr)->sc_opaque), "uv_write_t");

struct AsyncUVShutdown
{
    AsyncLoopTimeout asyncShutdown;

    AsyncUVShutdown()  = default;
    ~AsyncUVShutdown() = default;

    AsyncUVShutdown(const AsyncUVShutdown&)            = delete;
    AsyncUVShutdown& operator=(const AsyncUVShutdown&) = delete;

    static void init(uv_loop_t* loop, uv_shutdown_t* req, uv_stream_t* handle);

    static AsyncUVShutdown& from(uv_shutdown_t* req);
};
static_assert(sizeof(AsyncUVShutdown) <= sizeof(((uv_shutdown_t*)nullptr)->sc_opaque), "uv_shutdown_t");

struct AsyncUVUdpSend
{
    static constexpr unsigned int MaxInlineBuffers = 8;

    AsyncSocketSendTo asyncSend;
    AsyncLoopTimeout  asyncError;
    Span<const char>  buffers[MaxInlineBuffers] = {};
    unsigned int      nBuffers                  = 0;
    unsigned int      pendingBytes              = 0;
    int               pendingStatus             = 0;

    AsyncUVUdpSend()  = default;
    ~AsyncUVUdpSend() = default;

    AsyncUVUdpSend(const AsyncUVUdpSend&)            = delete;
    AsyncUVUdpSend& operator=(const AsyncUVUdpSend&) = delete;

    static void init(uv_loop_t* loop, uv_udp_send_t* req, uv_udp_t* handle, uv_udp_send_cb send_cb);

    static AsyncUVUdpSend& from(uv_udp_send_t* req);
};
static_assert(sizeof(AsyncUVUdpSend) <= sizeof(((uv_udp_send_t*)nullptr)->sc_opaque), "uv_udp_send_t");

struct AsyncUVRandom
{
    AsyncLoopTimeout asyncRandom;
    void*            buffer = nullptr;
    size_t           length = 0;
    uv_random_cb     cb     = nullptr;

    AsyncUVRandom()  = default;
    ~AsyncUVRandom() = default;

    AsyncUVRandom(const AsyncUVRandom&)            = delete;
    AsyncUVRandom& operator=(const AsyncUVRandom&) = delete;

    static void init(uv_random_t* req);

    static AsyncUVRandom& from(uv_random_t* req);
};
static_assert(sizeof(AsyncUVRandom) <= sizeof(((uv_random_t*)nullptr)->sc_opaque), "uv_random_t");

struct AsyncUVGetNameInfo
{
    AsyncLoopTimeout asyncGetNameInfo;
    int              status = 0;

    AsyncUVGetNameInfo()  = default;
    ~AsyncUVGetNameInfo() = default;

    AsyncUVGetNameInfo(const AsyncUVGetNameInfo&)            = delete;
    AsyncUVGetNameInfo& operator=(const AsyncUVGetNameInfo&) = delete;

    static void init(uv_getnameinfo_t* req);

    static AsyncUVGetNameInfo& from(uv_getnameinfo_t* req);
};
static_assert(sizeof(AsyncUVGetNameInfo) <= sizeof(((uv_getnameinfo_t*)nullptr)->sc_opaque), "uv_getnameinfo_t");

struct AsyncUVGetAddrInfo
{
    AsyncLoopTimeout asyncGetAddrInfo;
    int              status = 0;

    AsyncUVGetAddrInfo()  = default;
    ~AsyncUVGetAddrInfo() = default;

    AsyncUVGetAddrInfo(const AsyncUVGetAddrInfo&)            = delete;
    AsyncUVGetAddrInfo& operator=(const AsyncUVGetAddrInfo&) = delete;

    static void init(uv_getaddrinfo_t* req);

    static AsyncUVGetAddrInfo& from(uv_getaddrinfo_t* req);
};
static_assert(sizeof(AsyncUVGetAddrInfo) <= sizeof(((uv_getaddrinfo_t*)nullptr)->sc_opaque), "uv_getaddrinfo_t");

struct AsyncUVWork
{
    AsyncLoopTimeout asyncWork;
    uv_work_cb       workCb      = nullptr;
    uv_after_work_cb afterWorkCb = nullptr;

    AsyncUVWork()  = default;
    ~AsyncUVWork() = default;

    AsyncUVWork(const AsyncUVWork&)            = delete;
    AsyncUVWork& operator=(const AsyncUVWork&) = delete;

    static void init(uv_work_t* req);

    static AsyncUVWork& from(uv_work_t* req);
};
static_assert(sizeof(AsyncUVWork) <= sizeof(((uv_work_t*)nullptr)->sc_opaque), "uv_work_t");

struct AsyncUVConnect
{
    AsyncSocketConnect asyncConnect;

    AsyncUVConnect()  = default;
    ~AsyncUVConnect() = default;

    AsyncUVConnect(const AsyncUVConnect&)            = delete;
    AsyncUVConnect& operator=(const AsyncUVConnect&) = delete;

    static void init(uv_loop_t* loop, uv_connect_t* req);

    static AsyncUVConnect& from(uv_connect_t* req);
};
static_assert(sizeof(AsyncUVConnect) <= sizeof(((uv_connect_t*)nullptr)->sc_opaque), "uv_connect_t");

struct AsyncUVLoop
{
    AsyncEventLoop eventLoop;

    LinkedListQueue<uv_prepare_t, uv__queue, &uv_prepare_t::handle_queue> prepareHandles;
    LinkedListQueue<uv_check_t, uv__queue, &uv_check_t::handle_queue>     checkHandles;
    LinkedListQueue<uv_idle_t, uv__queue, &uv_idle_t::handle_queue>       idleHandles;
    LinkedListQueue<uv_handle_t, uv__queue, &uv_handle_t::handle_queue>   handles;

    AsyncLoopTimeout idleTimeout;

    AsyncUVLoop();

    static void construct(uv_loop_t* uvLoop);
    static void destruct(uv_loop_t* uvLoop);
    static bool isConstructed(const uv_loop_t* uvLoop);

    static AsyncEventLoop&       getLoop(uv_loop_t* uvLoop);
    static const AsyncEventLoop& getLoop(const uv_loop_t* uvLoop);

    static AsyncUVLoop& get(uv_loop_t* uvLoop);
    static AsyncUVLoop& get(AsyncEventLoop& eventLoop);

    static const AsyncUVLoop& get(const uv_loop_t* uvLoop);

    void closeHandle(uv_handle_t* handle, uv_close_cb close_cb);

  private:
    friend struct AsyncUVHandle;
    friend struct AsyncUVTcp;
    friend struct AsyncUVTimer;
    friend struct AsyncUVAsync;
    friend struct AsyncUVSignal;
    friend struct AsyncUVUdp;
    SC::Function<void(SC::AsyncResult&)> handleCloseFn;
    SC::Function<void(SC::AsyncResult&)> streamCloseFn;
    SC::Function<void(SC::AsyncResult&)> udpCloseFn;
    SC::Function<void(SC::AsyncResult&)> timerRestartFn;

    void registerActiveHandle(AsyncUVHandle* handle);
    void unregisterActiveHandle(AsyncUVHandle* handle);

    void closeAsyncRequest(uv_handle_t* handle, uv_close_cb close_cb);
    void closeAsyncStream(uv_stream_t* stream, uv_close_cb close_cb);
    void closeAsyncUDP(uv_udp_t* handle, uv_close_cb close_cb);

    static void executeCloseHandle(SC::AsyncResult& res);
    static void executeCloseStream(SC::AsyncResult& res);
    static void executeCloseUDP(SC::AsyncResult& res);
    static void executeRestartTimer(SC::AsyncResult& res);
};

} // namespace SC
