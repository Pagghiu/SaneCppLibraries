// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncStreams.h"
#include "../Foundation/Assert.h"
#include "../Foundation/Deferred.h"

//-------------------------------------------------------------------------------------------------------
// AsyncBufferView
//-------------------------------------------------------------------------------------------------------
void SC::AsyncBuffersPool::refBuffer(AsyncBufferView::ID bufferID)
{
    AsyncBufferView* buffer = buffers.get(bufferID.identifier);
    SC_ASSERT_RELEASE(buffer);
    buffer->refs++;
}

void SC::AsyncBuffersPool::unrefBuffer(AsyncBufferView::ID bufferID)
{
    AsyncBufferView* buffer = buffers.get(bufferID.identifier);
    SC_ASSERT_RELEASE(buffer);
    SC_ASSERT_RELEASE(buffer->refs != 0);
    buffer->refs--;
    if (buffer->refs == 0)
    {
        buffer->data = buffer->originalData;
    }
}

SC::Result SC::AsyncBuffersPool::getData(AsyncBufferView::ID bufferID, Span<const char>& data)
{
    Span<char> mutableData;
    SC_TRY(getData(bufferID, mutableData));
    data = mutableData;
    return Result(true);
}

SC::Result SC::AsyncBuffersPool::getData(AsyncBufferView::ID bufferID, Span<char>& data)
{
    AsyncBufferView* buffer = buffers.get(bufferID.identifier);
    if (buffer == nullptr)
    {
        return Result::Error("AsyncBuffersPool::getData - Invalid bufferID");
    }
    data = buffer->data;
    return Result(true);
}

SC::AsyncBufferView* SC::AsyncBuffersPool::getBuffer(AsyncBufferView::ID bufferID)
{
    return buffers.get(bufferID.identifier);
}

SC::Result SC::AsyncBuffersPool::requestNewBuffer(size_t minimumSizeInBytes, AsyncBufferView::ID& bufferID,
                                                  Span<char>& data)
{
    for (AsyncBufferView& buffer : buffers)
    {
        if (buffer.refs == 0 and buffer.data.sizeInBytes() >= minimumSizeInBytes)
        {
            buffer.refs         = 1;
            buffer.originalData = buffer.data;
            bufferID = AsyncBufferView::ID(static_cast<AsyncBufferView::ID::NumericType>(&buffer - buffers.begin()));
            return getData(bufferID, data);
        }
    }
    return Result::Error("AsyncBuffersPool::requestNewBuffer failed");
}

void SC::AsyncBuffersPool::setNewBufferSize(AsyncBufferView::ID bufferID, size_t newSizeInBytes)
{
    AsyncBufferView* buffer = buffers.get(bufferID.identifier);
    if (buffer and (newSizeInBytes < buffer->originalData.sizeInBytes()))
    {
        buffer->data = {buffer->data.data(), newSizeInBytes};
    }
}

//-------------------------------------------------------------------------------------------------------
// AsyncReadableStream
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncReadableStream::init(AsyncBuffersPool& buffersPool, Span<Request> requests)
{
    SC_TRY_MSG(state == State::Stopped, "Can init only in Stopped state")
    buffers   = &buffersPool;
    readQueue = requests;
    state     = State::CanRead;
    return Result(true);
}

SC::Result SC::AsyncReadableStream::start()
{
    SC_TRY_MSG(state == State::CanRead, "Can start only in CanRead state")
    executeRead();
    return Result(true);
}

void SC::AsyncReadableStream::emitOnData()
{
    Request request;
    while (readQueue.popFront(request))
    {
        eventData.emit(request.bufferID);
        buffers->unrefBuffer(request.bufferID); // 1b. refBuffer in push
    }
}

void SC::AsyncReadableStream::push(AsyncBufferView::ID bufferID, size_t newSize)
{
    if (state == State::Destroying)
    {
        eventClose.emit();
        state = State::Destroyed;
        return;
    }
    // Push buffer to the queue
    buffers->setNewBufferSize(bufferID, newSize);
    Request request;
    request.bufferID = bufferID;
    if (not readQueue.pushBack(request))
    {
        state = State::Errored;
        emitError(Result::Error("AsyncReadableStream::push dropping buffer"));
        return;
    }
    buffers->refBuffer(bufferID); // 1a. unrefBuffer in emitOnData()

    switch (state)
    {
    case State::SyncPushing:
    case State::Reading: {
        emitOnData();
        state = State::SyncPushing;
    }
    break;
    case State::AsyncPushing:
    case State::AsyncReading: {
        emitOnData();
        state = State::AsyncPushing;
    }
    break;
    default: {
        emitError(Result::Error("AsyncReadableStream::push - called in wrong state"));
    }
    break;
    }
}

void SC::AsyncReadableStream::reactivate(bool doReactivate)
{
    switch (state)
    {
    case State::SyncPushing: {
        if (doReactivate)
        {
            state = State::SyncReadMore;
        }
        else
        {
            state = State::CanRead;
        }
    }
    break;
    case State::AsyncPushing: {
        if (doReactivate)
        {
            executeRead(); // -> State::Reading
        }
        else
        {
            state = State::CanRead;
        }
    }
    break;
    default: {
        emitError(Result::Error("AsyncReadableStream::reactivate - called in wrong state"));
    }
    }
}

void SC::AsyncReadableStream::pause()
{
    switch (state)
    {
    case State::Reading:
    case State::AsyncReading:
    case State::SyncPushing:
    case State::AsyncPushing: {
        state = State::Pausing;
    }
    break;
    default: {
        emitError(Result::Error("AsyncReadableStream::pause - called in wrong state"));
    }
    }
}

void SC::AsyncReadableStream::resume()
{
    switch (state)
    {
    case State::Pausing:
    case State::Paused: {
        executeRead(); // -> State::Reading
        emitOnData();
    }
    break;
    case State::CanRead: {
        executeRead(); // -> State::Reading
    }
    break;
    case State::Stopped:
    case State::Errored: {
        emitError(Result::Error("AsyncReadableStream::resume - called in wrong state"));
    }
    break;
    case State::Ended: break;
    default: break; // Ignore resume requests while reading
    }
}

void SC::AsyncReadableStream::destroy()
{
    switch (state)
    {
    case State::CanRead:
    case State::SyncPushing:
    case State::SyncReadMore:
    case State::Paused:
    case State::Pausing:
    case State::Reading:
        state = State::Destroyed;
        eventClose.emit();
        break;
    case State::AsyncPushing:
    case State::AsyncReading:
        // Must wait for async read to finish
        state = State::Destroying;
        break;
    case State::Destroying: emitError(Result::Error("AsyncReadableStream::destroy - already destroying")); break;
    case State::Destroyed: emitError(Result::Error("AsyncReadableStream::destroy - already destroyed")); break;
    case State::Ended: emitError(Result::Error("AsyncReadableStream::destroy - already ended")); break;
    case State::Stopped: emitError(Result::Error("AsyncReadableStream::destroy - already stopped")); break;
    case State::Errored: emitError(Result::Error("AsyncReadableStream::destroy - already in error state")); break;
    }
}

void SC::AsyncReadableStream::executeRead()
{
    state = State::Reading;
    while (true)
    {
        const SC::Result res = asyncRead();
        if (res)
        {
            switch (state)
            {
            case State::SyncReadMore:
                // push + reactivate(true) have been called synchronously (inside this method)
                state = State::Reading;
                continue; // loop calling one more asyncRead
            case State::Reading:
                // push + reactivate(...) have not been called so this becomes an async call
                state = State::AsyncReading;
                break;
            case State::SyncPushing:
                state = State::Errored;
                emitError(Result::Error("Forgot to call reactivate({true || false}) from asyncRead"));
                break;
            default: break;
            }
        }
        else
        {
            state = State::Errored;
            emitError(res);
        }
        break;
    }
}

void SC::AsyncReadableStream::pushEnd()
{
    switch (state)
    {
    case State::CanRead:
    case State::Reading:
    case State::SyncPushing:
    case State::SyncReadMore:
    case State::Paused:
    case State::AsyncPushing:
    case State::AsyncReading:
    case State::Pausing:
        // In all these state we can just end directly
        state = State::Ended;
        eventEnd.emit();
        eventClose.emit();
        break;
    case State::Destroying:
        eventClose.emit();
        state = State::Destroyed;
        break;
    case State::Destroyed: emitError(Result::Error("AsyncReadableStream::pushEnd - stream is destroyed")); break;
    case State::Ended: emitError(Result::Error("AsyncReadableStream::pushEnd - stream already ended")); break;
    case State::Stopped: emitError(Result::Error("AsyncReadableStream::pushEnd - stream is not even inited")); break;
    case State::Errored: emitError(Result::Error("AsyncReadableStream::pushEnd - stream is in error state")); break;
    }
}

SC::AsyncBuffersPool& SC::AsyncReadableStream::getBuffersPool() { return *buffers; }

void SC::AsyncReadableStream::emitError(Result error) { eventError.emit(error); }

bool SC::AsyncReadableStream::getBufferOrPause(size_t minumumSizeInBytes, AsyncBufferView::ID& bufferID,
                                               Span<char>& data)
{
    if (getBuffersPool().requestNewBuffer(minumumSizeInBytes, bufferID, data))
    {
        return true;
    }
    else
    {
        pause();
        return false;
    }
}

//-------------------------------------------------------------------------------------------------------
// AsyncWritableStream
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncWritableStream::init(AsyncBuffersPool& buffersPool, Span<Request> requests)
{
    SC_TRY_MSG(state == State::Stopped, "AsyncWritableStream::init - can only be called when stopped");
    buffers    = &buffersPool;
    writeQueue = requests;
    return Result(true);
}

SC::Result SC::AsyncWritableStream::write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb)
{
    if (state == State::Ended or state == State::Ending)
    {
        return Result::Error("AsyncWritableStream::write - failed (ending or ended state)");
    }
    Request request;
    request.bufferID = bufferID;
    request.cb       = move(cb);
    if (not writeQueue.pushBack(request))
    {
        return Result::Error("AsyncWritableStream::write - queue is full");
    }
    buffers->refBuffer(bufferID); // 2a. unrefBuffer below or in finishedWriting
    switch (state)
    {
    case State::Stopped: {
        state = State::Writing;
        SC_ASSERT_RELEASE(writeQueue.popFront(request));
        tryAsync(asyncWrite(request.bufferID, request.cb));
        buffers->unrefBuffer(request.bufferID); // 2b. refBuffer above
    }
    break;
    case State::Writing: {
        // This is fine, it has already been queued
    }
    break;
    case State::Ending:
    case State::Ended: Assert::unreachable(); break;
    }
    return Result(true);
}

SC::Result SC::AsyncWritableStream::write(Span<const char> data, Function<void(AsyncBufferView::ID)> cb)
{
    AsyncBufferView::ID bufferID;
    Span<char>          bufferData;
    SC_TRY(buffers->requestNewBuffer(data.sizeInBytes(), bufferID, bufferData)); // 3a. unrefBuffer below
    memcpy(bufferData.data(), data.data(), data.sizeInBytes());
    buffers->setNewBufferSize(bufferID, data.sizeInBytes());
    auto deferredUnref = MakeDeferred([this, bufferID] { buffers->unrefBuffer(bufferID); }); // 3b. requestNewBuffer
    return write(bufferID, cb);
}

bool SC::AsyncWritableStream::tryAsync(Result potentialError)
{
    if (potentialError)
    {
        eventError.emit(potentialError);
        return false;
    }
    return true;
}

void SC::AsyncWritableStream::finishedWriting(AsyncBufferView::ID                   bufferID,
                                              Function<void(AsyncBufferView::ID)>&& callback, Result res)
{
    SC_ASSERT_RELEASE(state == State::Writing or state == State::Ending);

    if (not res)
    {
        eventError.emit(res);
    }

    bool    emitDrain = false;
    Request request;
    if (writeQueue.popFront(request))
    {
        tryAsync(asyncWrite(request.bufferID, request.cb));
        buffers->unrefBuffer(request.bufferID); // 2c. refbuffer in AsyncWritable::write
    }
    else
    {
        // Queue is empty
        if (state == State::Ending)
        {
            state = State::Ended;
        }
        else
        {
            state     = State::Stopped;
            emitDrain = true;
        }
    }
    if (callback.isValid())
    {
        callback(bufferID);
    }

    if (state == State::Ended)
    {
        eventFinish.emit();
    }
    else if (emitDrain)
    {
        eventDrain.emit();
    }
}

void SC::AsyncWritableStream::end()
{
    switch (state)
    {
    case State::Stopped:
        // Can just jump to ended state
        state = State::Ended;
        eventFinish.emit();
        break;
    case State::Writing:
        // We need to wait for current in-flight write to end
        state = State::Ending;
        break;
    case State::Ending:
    case State::Ended: {
        // Invalid state, already ended or already ending
        eventError.emit(Result::Error("AsyncWritableStream::end - already called"));
    }
    break;
    }
}

SC::AsyncBuffersPool& SC::AsyncWritableStream::getBuffersPool() { return *buffers; }

void SC::AsyncWritableStream::emitError(Result error) { eventError.emit(error); }
//-------------------------------------------------------------------------------------------------------
// AsyncPipeline
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncPipeline::start()
{
    SC_TRY_MSG(source != nullptr, "AsyncPipeline::start - Missing source");

    AsyncBuffersPool& buffers = source->getBuffersPool();
    for (const Pipe& pipe : pipes)
    {
        if (&pipe.sink->getBuffersPool() != &buffers)
        {
            return Result::Error("AsyncPipeline::start - all streams must use the same AsyncBuffersPool");
        }
    }

    // TODO: Register also onErrors
    SC_TRY((source->eventData.addListener<AsyncPipeline, &AsyncPipeline::onBufferRead>(*this)));
    return source->start();
}

void SC::AsyncPipeline::onBufferRead(AsyncBufferView::ID bufferID)
{
    for (const Pipe& pipe : pipes)
    {
        source->getBuffersPool().refBuffer(bufferID); // 4a. AsyncPipeline::onBufferWritten
        Function<void(AsyncBufferView::ID)> cb;
        cb.bind<AsyncPipeline, &AsyncPipeline::onBufferWritten>(*this);
        Result res = pipe.sink->write(bufferID, move(cb));
        if (not res)
        {
            eventError.emit(res);
        }
    }
}

void SC::AsyncPipeline::onBufferWritten(AsyncBufferView::ID bufferID)
{
    source->getBuffersPool().unrefBuffer(bufferID); // 4b. AsyncPipeline::onBufferRead
    source->resume();
}
