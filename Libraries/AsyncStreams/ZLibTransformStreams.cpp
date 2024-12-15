// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "ZLibTransformStreams.h"

#include "Internal/ZLibStream.inl"
//-------------------------------------------------------------------------------------------------------
// SyncZLibTransformStream
//-------------------------------------------------------------------------------------------------------
SC::SyncZLibTransformStream::SyncZLibTransformStream()
{
    using Self = SyncZLibTransformStream;
    AsyncWritableStream::asyncWrite.bind<Self, &Self::transform>(*this);
    AsyncWritableStream::canEndWritable.bind<Self, &Self::canEndTransform>(*this);
}

SC::Result SC::SyncZLibTransformStream::transform(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb)
{
    // This function will either process the bufferID fully OR it will unshift the buffer, that means placing it
    // again on top of the AsyncWritableStream write queue
    Span<const char> sourceData;
    SC_TRY(AsyncReadableStream::getBuffersPool().getData(bufferID, sourceData));
    Span<const char> inputData;
    SC_TRY(sourceData.sliceStart(consumedInputBytes, inputData));
    while (not inputData.empty())
    {
        Span<char>          outputData;
        AsyncBufferView::ID outputBufferID;
        if (getBufferOrPause(0, outputBufferID, outputData))
        {
            const size_t outputBefore = outputData.sizeInBytes();
            const size_t inputBefore  = inputData.sizeInBytes();
            SC_TRY(stream.process(inputData, outputData));
            const size_t consumedInput  = inputBefore - inputData.sizeInBytes();
            const size_t consumedOutput = outputBefore - outputData.sizeInBytes();
            consumedInputBytes += consumedInput;
            if (consumedOutput > 0)
            {
                AsyncReadableStream::push(outputBufferID, consumedOutput);
            }
            AsyncReadableStream::getBuffersPool().unrefBuffer(outputBufferID);
        }
        else
        {
            SC_TRY(AsyncWritableStream::unshift(bufferID, move(cb)));
            AsyncWritableStream::stop();
            return Result(true);
        }
    }
    auto callbackCopy = move(cb);
    cb                = {};
    AsyncWritableStream::finishedWriting(bufferID, move(callbackCopy), Result(true));
    consumedInputBytes = 0;
    return Result(true);
}

bool SC::SyncZLibTransformStream::canEndTransform()
{
    // Loop to get buffers in order to finish finalizing the stream
    // If there are no buffers, return true to signal AsyncWritableStream
    // we need to hold the "Ending" state of the state machine, to finish
    // writing this last trail of transformed data.
    AsyncBufferView::ID outputBufferID;
    Span<char>          outputBefore;
    while (getBufferOrPause(0, outputBufferID, outputBefore))
    {
        Span<char> outputData = outputBefore;

        bool streamEnded = false;
        if (not stream.finalize(outputData, streamEnded))
        {
            AsyncWritableStream::emitError(Result::Error("SyncZLibTransformStream::canEndTransform error"));
            return true; // --> Transition to ENDED (unrecoverable error)
        }
        const size_t outputBytes = outputBefore.sizeInBytes() - outputData.sizeInBytes();
        if (outputBytes > 0)
        {
            AsyncReadableStream::push(outputBufferID, outputBytes);
        }
        AsyncReadableStream::getBuffersPool().unrefBuffer(outputBufferID);
        if (streamEnded)
        {
            AsyncReadableStream::pushEnd();
            return true; // --> Transition to ENDED (all data written)
        }
    }
    return false; // == Keep in ENDING state
}
