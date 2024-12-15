// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "AsyncStreams.h"
#include "Internal/ZLibStream.h"
namespace SC
{
struct SyncZLibTransformStream : public AsyncTransformStream
{
    SyncZLibTransformStream();
    ZLibStream stream;

  private:
    size_t consumedInputBytes = 0;

    Result transform(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);
    bool   canEndTransform();
};

} // namespace SC
