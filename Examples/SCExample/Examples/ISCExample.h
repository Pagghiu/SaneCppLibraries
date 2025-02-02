// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Foundation/Function.h"
#include "Libraries/Foundation/Result.h"
#include "Libraries/Plugin/PluginHash.h"

namespace SC
{
struct AsyncEventLoop;
struct ISCExample
{
    static constexpr auto InterfaceHash = SC::PluginHash("ISCExample");

    Function<void(void)> onDraw;

    Function<Result(AsyncEventLoop&)> initAsync;
    Function<Result(AsyncEventLoop&)> closeAsync;

    Function<Result(Vector<uint8_t>&, Vector<uint8_t>&)>       serialize;
    Function<Result(Span<const uint8_t>, Span<const uint8_t>)> deserialize;
};

} // namespace SC
