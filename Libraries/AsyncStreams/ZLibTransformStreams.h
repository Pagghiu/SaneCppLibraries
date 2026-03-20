// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
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

template <typename T_AsyncEventLoop>
struct AsyncZLibTransformStreamT : public AsyncTransformStream
{
    AsyncZLibTransformStreamT()
    {
        asyncWork.work.template bind<AsyncZLibTransformStreamT, &AsyncZLibTransformStreamT::work>(*this);
        asyncWork.callback.template bind<AsyncZLibTransformStreamT, &AsyncZLibTransformStreamT::afterWork>(*this);
    }

    ZLibStream stream;

    typename T_AsyncEventLoop::LoopWork asyncWork;

    void setEventLoop(T_AsyncEventLoop& loop) { eventLoop = &loop; }

  private:
    T_AsyncEventLoop* eventLoop = nullptr;

    virtual Result onProcess(Span<const char> input, Span<char> output) override
    {
        SC_ASSERT_RELEASE(not finalizing);
        savedInput  = input;
        savedOutput = output;
        finalizing  = false;
        SC_TRY_MSG(eventLoop != nullptr, "AsyncZLibTransformStreamT::setEventLoop not called");
        return asyncWork.start(*eventLoop);
    }

    virtual Result onFinalize(Span<char> output) override
    {
        // Intentionally not resetting savedInput, that can contain leftover data to process
        savedOutput = output;
        finalizing  = true;
        SC_TRY_MSG(eventLoop != nullptr, "AsyncZLibTransformStreamT::setEventLoop not called");
        return asyncWork.start(*eventLoop);
    }

    void afterWork(typename T_AsyncEventLoop::LoopWork::Result&)
    {
        if (finalizing)
        {
            AsyncTransformStream::afterFinalize(savedOutput, streamEnded);
        }
        else
        {
            AsyncTransformStream::afterProcess(savedInput, savedOutput);
        }
    }

    Result work()
    {
        return finalizing ? stream.finalize(savedOutput, streamEnded) : stream.process(savedInput, savedOutput);
    }

    bool finalizing  = false;
    bool streamEnded = false;

    Span<const char> savedInput;
    Span<char>       savedOutput;
};

} // namespace SC
