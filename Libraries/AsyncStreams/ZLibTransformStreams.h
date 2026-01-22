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

    virtual Result asyncWrite(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb) override;
    virtual bool   canEndWritable() override;
};

struct AsyncZLibTransformStream : public AsyncTransformStream
{
    AsyncZLibTransformStream();

    ZLibStream    stream;
    AsyncLoopWork asyncWork;

    void setEventLoop(AsyncEventLoop& loop) { eventLoop = &loop; }

  private:
    AsyncEventLoop* eventLoop = nullptr;

    virtual Result onProcess(Span<const char> input, Span<char> output) override;
    virtual Result onFinalize(Span<char> output) override;

    void   afterWork(AsyncLoopWork::Result& result);
    Result work();

    bool finalizing  = false;
    bool streamEnded = false;

    Span<const char> savedInput;
    Span<char>       savedOutput;
};

} // namespace SC
