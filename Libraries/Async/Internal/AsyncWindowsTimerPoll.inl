// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

namespace SC
{
namespace detail
{
// Poll returns true only for a timeout. Other results are handled by the caller.
template <typename Poll, typename RefreshTime>
void pollWindowsAsyncTimer(const TimeMs* deadline, bool noWait, TimeMs now, Poll&& poll, RefreshTime&& refreshTime)
{
    if (deadline == nullptr)
    {
        poll(noWait ? 0 : INFINITE);
        return;
    }
    for (;;)
    {
        const int64_t remaining    = deadline->milliseconds - now.milliseconds;
        const DWORD   milliseconds = remaining <= 0                                ? 0
                                     : remaining >= static_cast<int64_t>(INFINITE) ? INFINITE - 1
                                                                                   : static_cast<DWORD>(remaining);
        if (not poll(milliseconds))
            return;
        now = refreshTime();
        if (now.milliseconds >= deadline->milliseconds)
            return;
    }
}
} // namespace detail
} // namespace SC
