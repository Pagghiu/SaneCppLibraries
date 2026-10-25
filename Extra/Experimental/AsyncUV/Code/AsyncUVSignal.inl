// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncUV.h"

namespace SC
{

AsyncUVSignal* AsyncUVSignal::from(uv_signal_t* handle)
{
    SC_ASSERT_RELEASE(handle->type == UV_SIGNAL);
    return reinterpret_cast<AsyncUVSignal*>(handle);
}

void AsyncUVSignal::construct(uv_loop_t* loop, uv_signal_t* handle)
{
    handle->type      = UV_SIGNAL;
    handle->signal_cb = nullptr;
    handle->signum    = 0;

    AsyncUVSignal* request = AsyncUVSignal::from(handle);
    placementNew(*request, loop, handle->type);
    request->signal.setUserFlags(AsyncUVFlags(handle->type).getUserFlags());
    AsyncUVLoop::get(loop).registerActiveHandle(request);
}

int AsyncUVSignal::start(uv_signal_cb cb, int signalNumber, bool oneShot)
{
    if (cb == nullptr or signalNumber <= 0)
    {
        return UV_EINVAL;
    }
    if (uv_is_closing(reinterpret_cast<uv_handle_t*>(this)))
    {
        return UV_EINVAL;
    }

    signal_cb    = cb;
    this->signum = signalNumber;

    AsyncSignalOptions options;
    options.mode    = oneShot ? AsyncSignalOptions::Mode::OneShot : AsyncSignalOptions::Mode::Persistent;
    signal.callback = [this](AsyncSignal::Result& result)
    {
        if (signal_cb != nullptr)
        {
            signal_cb(reinterpret_cast<uv_signal_t*>(this), result.completionData.signalNumber);
        }
    };

    return signal.start(AsyncUVLoop::getLoop(loop), signalNumber, options) ? 0 : UV_EINVAL;
}

int AsyncUVSignal::stop()
{
    signal_cb = nullptr;
    if (signal.isFree())
    {
        return 0;
    }
    return signal.stop(AsyncUVLoop::getLoop(loop)) ? 0 : UV_EINVAL;
}

} // namespace SC

UV_API int uv_signal_init(uv_loop_t* loop, uv_signal_t* signal)
{
    if (loop == nullptr or signal == nullptr)
    {
        return UV_EINVAL;
    }
    SC::AsyncUVSignal::construct(loop, signal);
    return 0;
}

UV_API int uv_signal_start(uv_signal_t* signal, uv_signal_cb signal_cb, int signum)
{
    if (signal == nullptr)
    {
        return UV_EINVAL;
    }
    return SC::AsyncUVSignal::from(signal)->start(signal_cb, signum, false);
}

UV_API int uv_signal_start_oneshot(uv_signal_t* signal, uv_signal_cb signal_cb, int signum)
{
    if (signal == nullptr)
    {
        return UV_EINVAL;
    }
    return SC::AsyncUVSignal::from(signal)->start(signal_cb, signum, true);
}

UV_API int uv_signal_stop(uv_signal_t* signal)
{
    if (signal == nullptr)
    {
        return UV_EINVAL;
    }
    return SC::AsyncUVSignal::from(signal)->stop();
}

UV_API int uv_kill(uv_pid_t pid, int signum)
{
    if (signum <= 0)
    {
        return UV_EINVAL;
    }
#if defined(NSIG)
    if (signum >= NSIG)
    {
        return UV_EINVAL;
    }
#endif
#if SC_PLATFORM_WINDOWS
    (void)pid;
    return UV_ENOSYS;
#else
    return ::kill(pid, signum) == 0 ? 0 : SC::AsyncUVMapSystemErrorToUv(errno);
#endif
}
