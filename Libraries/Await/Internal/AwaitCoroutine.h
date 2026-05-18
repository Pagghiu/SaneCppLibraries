// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include <coroutine>

namespace SC
{
using AwaitCoroutineHandle = std::coroutine_handle<>;

template <typename Promise>
using AwaitCoroutineTypedHandle = std::coroutine_handle<Promise>;

using AwaitSuspendAlways = std::suspend_always;
} // namespace SC
