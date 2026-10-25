// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../../../Libraries/Testing/Limits.h"
#include "AsyncUV.h"
namespace SC
{

AsyncUVTimer* AsyncUVTimer::from(uv_timer_t* handle)
{
    SC_ASSERT_RELEASE(handle->type == UV_TIMER);
    return reinterpret_cast<AsyncUVTimer*>(handle);
}
uv_timer_t* AsyncUVTimer::from(AsyncRequest& request)
{
    SC_ASSERT_RELEASE(request.getType() == AsyncRequest::Type::LoopTimeout);
    return reinterpret_cast<uv_timer_t*>(reinterpret_cast<char*>(&request) - sizeof(AsyncUVHandle));
}
void AsyncUVTimer::construct(uv_loop_t* loop, uv_timer_t* handle)
{
    handle->type          = UV_TIMER;
    AsyncUVTimer* request = AsyncUVTimer::from(handle);
    placementNew(*request, loop, handle->type);
    request->timeout.setUserFlags(AsyncUVFlags(handle->type).getUserFlags());
    AsyncUVLoop::get(loop).registerActiveHandle(request);
}

struct AsyncUVTimer::RepeatCallback
{
    static constexpr uint64_t MaxTimeout = static_cast<decltype(SC::Time::Milliseconds::ms)>(SC::MaxValue());

    uv_timer_cb cb;
    uint64_t    repeat;

    void operator()(SC::AsyncLoopTimeout::Result& result)
    {
        using namespace SC;
        if (repeat > 0)
        {
            result.reactivateRequest(true);
        }
        AsyncUVTimer::from(AsyncUVTimer::from(result.async))->restart(cb, repeat);
    }
};

int AsyncUVTimer::start(uv_timer_cb cb, uint64_t timeoutMs, uint64_t repeat)
{
    AsyncLoopTimeout& request = timeout;

    if (timeoutMs >= RepeatCallback::MaxTimeout)
    {
        request.relativeTimeout.milliseconds = RepeatCallback::MaxTimeout;
    }
    else
    {
        request.relativeTimeout = Time::Milliseconds(static_cast<int64_t>(timeoutMs));
    }
    request.callback    = RepeatCallback{cb, repeat};
    Result       res    = Result(false);
    AsyncUVLoop& uvLoop = AsyncUVLoop::get(loop);
    if (request.isFree())
    {
        res = request.start(AsyncUVLoop::getLoop(loop));
    }
    else
    {
        res = request.stop(uvLoop.eventLoop, &uvLoop.timerRestartFn);
    }
    return res ? 0 : UV_EINVAL;
}

int AsyncUVTimer::stop()
{
    if (timeout.isFree())
    {
        return 0;
    }
    else
    {
        return timeout.stop(AsyncUVLoop::getLoop(loop)) ? 0 : -1;
    }
}

void AsyncUVTimer::restart(uv_timer_cb cb, uint64_t repeat)
{
    AsyncLoopTimeout& request = timeout;
    if (repeat > 0)
    {
        if (repeat >= RepeatCallback::MaxTimeout)
        {
            request.relativeTimeout.milliseconds = RepeatCallback::MaxTimeout;
        }
        else
        {
            request.relativeTimeout = Time::Milliseconds(static_cast<int64_t>(repeat));
        }
    }
    cb(reinterpret_cast<uv_timer_t*>(this));
}

int AsyncUVTimer::isRestarting() const
{
    return timeout.getCloseCallback() == &AsyncUVLoop::get(loop).timerRestartFn ? 1 : 0;
}
} // namespace SC

UV_API int uv_timer_init(uv_loop_t* loop, uv_timer_t* handle)
{
    using namespace SC;
    AsyncUVTimer::construct(loop, handle);
    return 0;
}

int uv_timer_is_restarting(uv_timer_t* handle)
{
    using namespace SC;
    return AsyncUVTimer::from(handle)->isRestarting();
}

UV_API int uv_timer_start(uv_timer_t* handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat)
{
    using namespace SC;
    if (cb == nullptr)
    {
        return UV_EINVAL;
    }
    if (uv_is_closing(reinterpret_cast<uv_handle_t*>(handle)))
    {
        return UV_EINVAL;
    }
    return AsyncUVTimer::from(handle)->start(cb, timeout, repeat);
}

UV_API int uv_timer_stop(uv_timer_t* handle)
{
    using namespace SC;
    return AsyncUVTimer::from(handle)->stop();
}

UV_API int uv_timer_again(uv_timer_t* handle)
{
    using namespace SC;
    AsyncLoopTimeout& request = AsyncUVTimer::from(handle)->timeout;
    if (not request.callback.isValid())
    {
        return UV_EINVAL;
    }
    AsyncUVTimer::RepeatCallback* tsc = request.callback.dynamicCastTo<AsyncUVTimer::RepeatCallback>();
    SC_ASSERT_RELEASE(tsc);

    uv_timer_cb timer_cb = tsc->cb;
    uint64_t    repeat   = tsc->repeat;

    if (repeat > 0)
    {
        uv_timer_start(handle, timer_cb, repeat, repeat);
    }
    return 0;
}

UV_API void uv_timer_set_repeat(uv_timer_t* handle, uint64_t repeat)
{
    using namespace SC;
    const AsyncLoopTimeout& request = AsyncUVTimer::from(handle)->timeout;
    if (request.callback.isValid())
    {
        AsyncUVTimer::RepeatCallback* tsc = request.callback.dynamicCastTo<AsyncUVTimer::RepeatCallback>();
        SC_ASSERT_RELEASE(tsc);
        tsc->repeat = repeat;
    }
}

UV_API uint64_t uv_timer_get_repeat(const uv_timer_t* handle)
{
    using namespace SC;
    const AsyncLoopTimeout& request = AsyncUVTimer::from(const_cast<uv_timer_t*>(handle))->timeout;
    if (not request.callback.isValid())
    {
        return 0;
    }
    AsyncUVTimer::RepeatCallback* tsc = request.callback.dynamicCastTo<AsyncUVTimer::RepeatCallback>();
    SC_ASSERT_RELEASE(tsc);
    return static_cast<uint64_t>(tsc->repeat);
}

UV_API uint64_t uv_timer_get_due_in(const uv_timer_t* handle)
{
    using namespace SC;
    const AsyncLoopTimeout& request = AsyncUVTimer::from(const_cast<uv_timer_t*>(handle))->timeout;
    return static_cast<uint64_t>(request.relativeTimeout.milliseconds);
}
