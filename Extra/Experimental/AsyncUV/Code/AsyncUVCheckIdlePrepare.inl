// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncUV.h"

//-----------------------------------------------------------------------------------------------------------
// uv_check
//-----------------------------------------------------------------------------------------------------------

UV_API int uv_check_init(uv_loop_t* uvLoop, uv_check_t* handle)
{
    using namespace SC;
    handle->type     = UV_CHECK;
    handle->loop     = uvLoop;
    handle->close_cb = nullptr;

    handle->handle_queue.next = &handle->handle_queue;
    handle->handle_queue.prev = &handle->handle_queue;
    return 0;
}

UV_API int uv_check_start(uv_check_t* handle, uv_check_cb cb)
{
    using namespace SC;
    AsyncUVLoop& asyncUVLoop = AsyncUVLoop::get(handle->loop);
    if (cb == nullptr)
        return UV_EINVAL;
    if (handle->check_cb == nullptr)
    {
        asyncUVLoop.checkHandles.queueFront(*handle);
    }
    handle->check_cb = cb;
    return 0;
}

UV_API int uv_check_stop(uv_check_t* handle)
{
    using namespace SC;
    AsyncUVLoop& asyncUVLoop = AsyncUVLoop::get(handle->loop);
    if (handle->check_cb != nullptr)
    {
        asyncUVLoop.checkHandles.remove(*handle);
    }
    handle->check_cb = nullptr;
    return 0;
}

//-----------------------------------------------------------------------------------------------------------
// uv_prepare
//-----------------------------------------------------------------------------------------------------------

UV_API int uv_prepare_init(uv_loop_t* uvLoop, uv_prepare_t* handle)
{
    using namespace SC;
    handle->loop     = uvLoop;
    handle->type     = UV_PREPARE;
    handle->close_cb = nullptr;

    handle->handle_queue.next = &handle->handle_queue;
    handle->handle_queue.prev = &handle->handle_queue;
    return 0;
}

UV_API int uv_prepare_start(uv_prepare_t* handle, uv_prepare_cb cb)
{
    using namespace SC;
    AsyncUVLoop& asyncUVLoop = AsyncUVLoop::get(handle->loop);
    if (cb == nullptr)
        return UV_EINVAL;
    if (handle->prepare_cb == nullptr)
    {
        asyncUVLoop.prepareHandles.queueFront(*handle);
    }
    handle->prepare_cb = cb;
    return 0;
}

UV_API int uv_prepare_stop(uv_prepare_t* handle)
{
    using namespace SC;
    AsyncUVLoop& asyncUVLoop = AsyncUVLoop::get(handle->loop);

    if (handle->prepare_cb != nullptr)
    {
        asyncUVLoop.prepareHandles.remove(*handle);
    }
    handle->prepare_cb = nullptr;
    return 0;
}

//-----------------------------------------------------------------------------------------------------------
// uv_idle
//-----------------------------------------------------------------------------------------------------------

UV_API int uv_idle_init(uv_loop_t* uvLoop, uv_idle_t* handle)
{
    using namespace SC;
    handle->loop     = uvLoop;
    handle->type     = UV_IDLE;
    handle->close_cb = nullptr;

    handle->handle_queue.next = &handle->handle_queue;
    handle->handle_queue.prev = &handle->handle_queue;
    return 0;
}

UV_API int uv_idle_start(uv_idle_t* handle, uv_idle_cb cb)
{
    using namespace SC;
    AsyncUVLoop& asyncUVLoop = AsyncUVLoop::get(handle->loop);
    if (cb == nullptr)
        return UV_EINVAL;
    if (handle->idle_cb == nullptr)
    {
        asyncUVLoop.idleHandles.queueFront(*handle);
    }
    handle->idle_cb = cb;
    if (asyncUVLoop.idleTimeout.isFree())
    {
        asyncUVLoop.idleTimeout.callback = [&asyncUVLoop](AsyncLoopTimeout::Result& result)
        {
            result.reactivateRequest(true);
            asyncUVLoop.idleHandles.forEach(
                [](uv_idle_t& idle)
                {
                    idle.idle_cb(&idle);
                    return true;
                });
        };
        Result res = asyncUVLoop.idleTimeout.start(asyncUVLoop.eventLoop, 0_ms);
        if (not res)
        {
            return UV_EINVAL;
        }
    }
    return 0;
}

UV_API int uv_idle_stop(uv_idle_t* handle)
{
    using namespace SC;
    AsyncUVLoop&    asyncUVLoop = AsyncUVLoop::get(handle->loop);
    AsyncEventLoop& eventLoop   = AsyncUVLoop::getLoop(handle->loop);

    if (handle->idle_cb != nullptr)
    {
        asyncUVLoop.idleHandles.remove(*handle);
    }
    handle->idle_cb = nullptr;
    if (asyncUVLoop.idleHandles.isEmpty())
    {
        if (not asyncUVLoop.idleTimeout.isFree())
        {
            SC_ASSERT_RELEASE(asyncUVLoop.idleTimeout.stop(eventLoop));
        }
    }
    return 0;
}
