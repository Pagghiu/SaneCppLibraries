// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Async/Async.h"
#include "AsyncStreams.h"
#include "Internal/ZLibStream.h"
namespace SC
{
struct SyncZLibTransformStream : public AsyncDuplexStream
{
    SyncZLibTransformStream();
    ZLibStream stream;

  private:
    size_t consumedInputBytes = 0;

    Result transform(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);
    bool   canEndTransform();
};

struct AsyncZLibTransformStream : public AsyncTransformStream
{
    AsyncZLibTransformStream();

    ZLibStream    stream;
    AsyncLoopWork asyncWork;

    void setEventLoop(AsyncEventLoop& loop) { eventLoop = &loop; }

  private:
    AsyncEventLoop* eventLoop = nullptr;

    Result compressExecute(Span<const char> input, Span<char> output);
    Result compressFinalize(Span<char> output);
    void   afterWork(AsyncLoopWork::Result& result);
    Result work();

    bool finalizing  = false;
    bool streamEnded = false;

    Span<const char> savedInput;
    Span<char>       savedOutput;
};

} // namespace SC
