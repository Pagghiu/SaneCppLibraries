// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../../../Libraries/Common/CompilerOffsetOf.h"
#include "../../../../Libraries/Time/Time.h"
#include "AsyncUV.h"

#if !SC_PLATFORM_WINDOWS
#include <unistd.h>
#endif

namespace SC
{
namespace
{
static constexpr uint64_t AsyncUVLoopStorageMagic = 0x534355564C4F4F50ULL;
static constexpr size_t   AsyncUVLoopStorageBytes = sizeof(((uv_loop_t*)0)->sc_opaque);

static AsyncUVLoop* loopStorageFrom(uv_loop_t* uvLoop) { return reinterpret_cast<AsyncUVLoop*>(uvLoop->sc_opaque + 1); }

static const AsyncUVLoop* loopStorageFrom(const uv_loop_t* uvLoop)
{
    return reinterpret_cast<const AsyncUVLoop*>(uvLoop->sc_opaque + 1);
}
} // namespace

static_assert(alignof(AsyncUVLoop) <= alignof(uint64_t), "AsyncUVLoop alignment must fit uv_loop_t::sc_opaque");
static_assert(sizeof(AsyncUVLoop) + sizeof(uint64_t) <= AsyncUVLoopStorageBytes,
              "AsyncUVLoop does not fit uv_loop_t::sc_opaque");

bool AsyncUVLoop::isConstructed(const uv_loop_t* uvLoop)
{
    SC_ASSERT_RELEASE(uvLoop != nullptr);
    return uvLoop->sc_opaque[0] == AsyncUVLoopStorageMagic;
}

AsyncUVLoop& AsyncUVLoop::get(uv_loop_t* uvLoop)
{
    SC_ASSERT_RELEASE(uvLoop != nullptr);
    SC_ASSERT_RELEASE(isConstructed(uvLoop));
    return *loopStorageFrom(uvLoop);
}

const AsyncUVLoop& AsyncUVLoop::get(const uv_loop_t* uvLoop)
{
    SC_ASSERT_RELEASE(uvLoop != nullptr);
    SC_ASSERT_RELEASE(isConstructed(uvLoop));
    return *loopStorageFrom(uvLoop);
}

void AsyncUVLoop::construct(uv_loop_t* uvLoop)
{
    SC_ASSERT_RELEASE(uvLoop != nullptr);
    void* loopData = uvLoop->data;
    memset(uvLoop, 0, sizeof(uv_loop_t));
    uvLoop->data         = loopData;
    uvLoop->sc_opaque[0] = AsyncUVLoopStorageMagic;
    placementNew(*loopStorageFrom(uvLoop));
}

void AsyncUVLoop::destruct(uv_loop_t* uvLoop)
{
    SC_ASSERT_RELEASE(uvLoop != nullptr);
    SC_ASSERT_RELEASE(isConstructed(uvLoop));
    void* loopData = uvLoop->data;
    loopStorageFrom(uvLoop)->~AsyncUVLoop();
    memset(uvLoop, 0, sizeof(uv_loop_t));
    uvLoop->data = loopData;
}

AsyncUVLoop& AsyncUVLoop::get(AsyncEventLoop& eventLoop)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF
    static_assert(SC_COMPILER_OFFSETOF(AsyncUVLoop, eventLoop) == 0, "SC_COMPILER_OFFSETOF(AsyncUVLoop, eventLoop)");
    SC_COMPILER_WARNING_POP_OFFSETOF
    return *reinterpret_cast<AsyncUVLoop*>(reinterpret_cast<char*>(&eventLoop));
}

AsyncEventLoop& AsyncUVLoop::getLoop(uv_loop_t* uvLoop) { return AsyncUVLoop::get(uvLoop).eventLoop; }

const AsyncEventLoop& AsyncUVLoop::getLoop(const uv_loop_t* uvLoop) { return AsyncUVLoop::get(uvLoop).eventLoop; }
AsyncUVLoop::AsyncUVLoop()
{
    handleCloseFn  = &executeCloseHandle;
    streamCloseFn  = &executeCloseStream;
    udpCloseFn     = &executeCloseUDP;
    timerRestartFn = &executeRestartTimer;
}
void AsyncUVLoop::executeCloseHandle(SC::AsyncResult& res)
{
    uv_handle_t* handle = AsyncUVHandle::from(res.async);
    handle->close_cb(handle);
    AsyncUVHandle::destruct(handle);
}

void AsyncUVLoop::executeCloseStream(SC::AsyncResult& res)
{
    using namespace SC;
    AsyncUVTcp::from(res.async)->close(); // will end up in executeCloseHandle later
}

void AsyncUVLoop::executeCloseUDP(SC::AsyncResult& res)
{
    using namespace SC;
    AsyncUVUdp* udp = reinterpret_cast<AsyncUVUdp*>(AsyncUVUdp::from(res.async));
    udp->close(&res.eventLoop); // will end up in executeCloseHandle later
}

void AsyncUVLoop::executeRestartTimer(SC::AsyncResult& res)
{
    SC::AsyncLoopTimeout& async = static_cast<SC::AsyncLoopTimeout&>(res.async);
    SC_ASSERT_RELEASE(async.start(res.eventLoop));
}

void AsyncUVLoop::registerActiveHandle(AsyncUVHandle* handle)
{
    handles.queueBack(*reinterpret_cast<uv_handle_t*>(handle));
}

void AsyncUVLoop::unregisterActiveHandle(AsyncUVHandle* handle)
{
    handles.remove(*reinterpret_cast<uv_handle_t*>(handle));
}

void AsyncUVLoop::closeAsyncRequest(uv_handle_t* handle, uv_close_cb close_cb)
{
    using namespace SC;
    AsyncRequest& async = AsyncUVHandle::getRequest(handle);
    if (close_cb)
    {
        handle->close_cb = close_cb;
    }
    else
    {
        handle->close_cb = [](uv_handle_t*) {};
    }
    if (async.isFree())
    {
        uv_close_cb backupCloseCb = handle->close_cb;
        AsyncUVHandle::destruct(handle);
        uv_timer_t* uvTimer = reinterpret_cast<uv_timer_t*>(handle);
        uv_timer_init(handle->loop, uvTimer);
        AsyncUVTimer* request     = AsyncUVTimer::from(uvTimer);
        handle->close_cb          = backupCloseCb;
        request->timeout.callback = &AsyncUVLoop::executeCloseHandle;
        SC_ASSERT_RELEASE(request->timeout.start(eventLoop, {0}));
    }
    else
    {
        SC_ASSERT_RELEASE(async.stop(eventLoop, &handleCloseFn));
    }
}

void AsyncUVLoop::closeAsyncStream(uv_stream_t* stream, uv_close_cb close_cb)
{
    AsyncUVTcp& tcpStream = *AsyncUVTcp::from(stream);
    SC_ASSERT_RELEASE(stream->close_cb == nullptr);
    if (close_cb)
    {
        stream->close_cb = close_cb;
    }
    else
    {
        stream->close_cb = [](uv_handle_t*) {};
    }
    AsyncRequest& request = tcpStream.getActiveRequest();
    if (request.isFree())
    {
        tcpStream.close();
    }
    else
    {
        SC_ASSERT_RELEASE(request.stop(eventLoop, &streamCloseFn));
    }
}

void AsyncUVLoop::closeAsyncUDP(uv_udp_t* handle, uv_close_cb close_cb)
{
    AsyncUVUdp& udpHandle = *AsyncUVUdp::from(handle);
    if (handle->close_cb != nullptr)
    {
        // libuv ignores duplicate close requests once closing has started.
        return;
    }
    if (close_cb)
    {
        handle->close_cb = close_cb;
    }
    else
    {
        handle->close_cb = [](uv_handle_t*) {};
    }

    AsyncRequest& request = udpHandle.getActiveRequest();
    if (request.isFree())
    {
        udpHandle.close(&eventLoop);
    }
    else
    {
        if (not request.stop(eventLoop, &udpCloseFn))
        {
            udpHandle.close(&eventLoop);
        }
    }
}

void AsyncUVLoop::closeHandle(uv_handle_t* handle, uv_close_cb close_cb)
{
    using namespace SC;

    switch (handle->type)
    {
    case UV_PREPARE: {
        prepareHandles.remove(*reinterpret_cast<uv_prepare_t*>(handle));
        if (close_cb)
        {
            close_cb(handle);
        }
    }
    break;
    case UV_CHECK: {
        prepareHandles.remove(*reinterpret_cast<uv_prepare_t*>(handle));
        if (close_cb)
        {
            close_cb(handle);
        }
    }
    break;
    case UV_IDLE: {
        idleHandles.remove(*reinterpret_cast<uv_idle_t*>(handle));
        if (close_cb)
        {
            close_cb(handle);
        }
        if (idleHandles.isEmpty())
        {
            (void)idleTimeout.stop(eventLoop);
        }
    }
    break;
    case UV_TIMER:
    case UV_ASYNC:
    case UV_SIGNAL: {
        static_assert(sizeof(AsyncUVTimer) <= sizeof(uv_timer_t), "AsyncUVTimer > uv_timer_t");
        static_assert(sizeof(AsyncUVTimer) <= sizeof(uv_async_t), "AsyncUVTimer > uv_async_t");
        static_assert(sizeof(AsyncUVSignal) <= sizeof(uv_signal_t), "AsyncUVSignal > uv_signal_t");
        closeAsyncRequest(handle, close_cb);
    }
    break;
    case UV_TCP: {
        static_assert(sizeof(AsyncUVTimer) <= sizeof(uv_stream_t), "AsyncUVTimer > uv_stream_t");
        closeAsyncStream(reinterpret_cast<uv_stream_t*>(handle), close_cb);
    }
    break;
    case UV_UDP: {
        static_assert(sizeof(AsyncUVTimer) <= sizeof(uv_udp_t), "AsyncUVTimer > uv_udp_t");
        closeAsyncUDP(reinterpret_cast<uv_udp_t*>(handle), close_cb);
    }
    break;
    case UV_NAMED_PIPE:
    case UV_TTY: {
        if (handle->close_cb != nullptr)
        {
            return;
        }
        handle->close_cb = close_cb != nullptr ? close_cb : [](uv_handle_t*) {};
        if (handle->u.fd >= 0)
        {
#if SC_PLATFORM_WINDOWS
            // Pipe stubs are not backed by Windows file descriptors yet.
#else
            (void)::close(handle->u.fd);
#endif
            handle->u.fd = -1;
        }
        handle->close_cb(handle);
    }
    break;
    default: {
        SC_ASSERT_RELEASE(false);
    }
    break;
    }
}
} // namespace SC
static uv_loop_t* globalLibUVLoop = nullptr;
UV_API uv_loop_t* uv_default_loop(void)
{
    using namespace SC;
    static uv_loop_t uvLoop;
    if (globalLibUVLoop == nullptr)
    {
        if (uv_loop_init(&uvLoop) == -1)
        {
            return nullptr;
        }
        globalLibUVLoop = &uvLoop;
    }
    return globalLibUVLoop;
}

UV_API void uv_library_shutdown(void)
{
    if (globalLibUVLoop)
    {
        (void)uv_loop_close(globalLibUVLoop);
    }
    globalLibUVLoop = nullptr;
}

SC::AsyncEventLoopListeners globalEventLoopListeners;

UV_API int uv_loop_init(uv_loop_t* uvLoop)
{
    using namespace SC;
    AsyncUVLoop::construct(uvLoop);
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(uvLoop);

    Result res = eventLoop.create();

    globalEventLoopListeners.beforeBlockingPoll = [](SC::AsyncEventLoop& eventLoop)
    {
        using namespace SC;
        AsyncUVLoop& asyncUVLoop = SC::AsyncUVLoop::get(eventLoop);
        asyncUVLoop.prepareHandles.forEach(
            [&asyncUVLoop](uv_prepare_t& prepare)
            {
                prepare.prepare_cb(&prepare);
                return true;
            });
    };

    globalEventLoopListeners.afterBlockingPoll = [](SC::AsyncEventLoop& eventLoop)
    {
        using namespace SC;
        AsyncUVLoop& asyncUVLoop = SC::AsyncUVLoop::get(eventLoop);
        asyncUVLoop.checkHandles.forEach(
            [&asyncUVLoop](uv_check_t& check)
            {
                check.check_cb(&check);
                return true;
            });
    };

    eventLoop.setListeners(&globalEventLoopListeners);
    eventLoop.updateTime(); // libuv requires non zero time for inited loop
    return res ? 0 : -1;
}

UV_API int uv_loop_close(uv_loop_t* uvLoop)
{
    using namespace SC;
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(uvLoop);
    if (eventLoop.getNumberOfActiveRequests() + eventLoop.getNumberOfSubmittedRequests() > 0)
    {
        return UV_EBUSY;
    }
    Result res = eventLoop.close();
    AsyncUVLoop::destruct(uvLoop);
    if (uvLoop == globalLibUVLoop)
    {
        globalLibUVLoop = nullptr;
    }
    return res ? 0 : -1;
}

UV_API int uv_run(uv_loop_t* uvLoop, uv_run_mode mode)
{
    using namespace SC;
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(uvLoop);
    Result          res(false);
    switch (mode)
    {
    case UV_RUN_DEFAULT: res = eventLoop.run(); break;
    case UV_RUN_ONCE: res = eventLoop.runOnce(); break;
    case UV_RUN_NOWAIT: res = eventLoop.runNoWait(); break;
    }
    if (res)
    {
        return eventLoop.getNumberOfActiveRequests() + eventLoop.getNumberOfSubmittedRequests();
    }
    else
    {
        return -1;
    }
}

UV_API void uv_stop(uv_loop_t* uvLoop)
{
    using namespace SC;
    AsyncEventLoop& eventLoop = AsyncUVLoop::getLoop(uvLoop);
    eventLoop.interrupt();
}

UV_API void uv_sleep(unsigned int ms) { SC::Thread::Sleep(ms); }

UV_API uint64_t uv_hrtime(void)
{
    SC::Time::HighResolutionCounter clock;
    return static_cast<uint64_t>(clock.snap().toNanoseconds().ns);
}

UV_API uint64_t uv_now(const uv_loop_t* uvLoop)
{
    using namespace SC;
    return static_cast<uint64_t>(AsyncUVLoop::getLoop(uvLoop).getLoopTime().milliseconds);
}

UV_API void uv_update_time(uv_loop_t* uvLoop)
{
    using namespace SC;
    AsyncUVLoop::getLoop(uvLoop).updateTime();
}

UV_EXTERN int uv_loop_alive(const uv_loop_t* uvLoop)
{
    using namespace SC;
    const AsyncEventLoop& loop             = AsyncUVLoop::getLoop(uvLoop);
    const int             numberOfRequests = loop.getNumberOfActiveRequests() + loop.getNumberOfSubmittedRequests();
    return numberOfRequests > 0 ? 1 : 0;
}

UV_API int uv_loop_configure(uv_loop_t*, uv_loop_option option, ...)
{
    if (option != UV_LOOP_BLOCK_SIGNAL)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    return UV_ENOSYS;
#else
    return 0;
#endif
}

UV_EXTERN int uv_backend_timeout(const uv_loop_t* uvLoop)
{
    using namespace SC;
    AsyncLoopTimeout* async = AsyncUVLoop::getLoop(uvLoop).findEarliestLoopTimeout();
    if (async != nullptr)
    {
        return static_cast<int>(async->relativeTimeout.milliseconds);
    }
    else
    {
        return -1;
    }
}

UV_API void uv_walk(uv_loop_t* uvLoop, uv_walk_cb cb, void* userData)
{
    using namespace SC;
    AsyncUVLoop& loop = AsyncUVLoop::get(uvLoop);
    loop.handles.forEach(
        [cb, userData](uv_handle_t& handle)
        {
            cb(&handle, userData);
            return true;
        });
}

UV_API size_t uv_loop_size(void) { return sizeof(uv_loop_t); }

UV_API void uv_loop_set_data(uv_loop_t* loop, void* data)
{
    if (loop != nullptr)
    {
        loop->data = data;
    }
}

UV_API void* uv_loop_get_data(const uv_loop_t* loop) { return loop != nullptr ? loop->data : nullptr; }
