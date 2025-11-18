// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncStreams.h"
#include "../Foundation/Assert.h"
#include "../Foundation/Deferred.h"

namespace SC
{
//-------------------------------------------------------------------------------------------------------
// AsyncBufferView
//-------------------------------------------------------------------------------------------------------
void AsyncBuffersPool::refBuffer(AsyncBufferView::ID bufferID)
{
    AsyncBufferView* buffer = getBuffer(bufferID);
    SC_ASSERT_RELEASE(buffer);
    buffer->refs++;
}

void AsyncBuffersPool::unrefBuffer(AsyncBufferView::ID bufferID)
{
    AsyncBufferView* buffer = getBuffer(bufferID);
    SC_ASSERT_RELEASE(buffer);
    SC_ASSERT_RELEASE(buffer->refs != 0);
    buffer->refs--;
    if (buffer->refs == 0)
    {
        switch (buffer->type)
        {
        case AsyncBufferView::Type::Writable: buffer->writableData = buffer->originalWritableData; break;
        case AsyncBufferView::Type::ReadOnly: *buffer = {}; break;
        case AsyncBufferView::Type::Growable: *buffer = {}; break;
        case AsyncBufferView::Type::Empty: Assert::unreachable(); break;
        }
    }
}

Result AsyncBuffersPool::getReadableData(AsyncBufferView::ID bufferID, Span<const char>& data)
{
    AsyncBufferView* buffer = getBuffer(bufferID);
    SC_TRY_MSG(buffer != nullptr, "AsyncBuffersPool::getData - Invalid bufferID");
    switch (buffer->type)
    {
    case AsyncBufferView::Type::Writable: data = buffer->writableData; break;
    case AsyncBufferView::Type::ReadOnly: data = buffer->readonlyData; break;
    case AsyncBufferView::Type::Growable: {
        AsyncBufferView::GrowableStorage storage;

        auto da = buffer->getGrowableBuffer(storage, true)->getDirectAccess();
        data    = {static_cast<char*>(da.data), da.sizeInBytes};
        (void)buffer->getGrowableBuffer(storage, false); // destruct
        break;
    }
    case AsyncBufferView::Type::Empty: Assert::unreachable(); break;
    }
    return Result(true);
}

Result AsyncBuffersPool::getWritableData(AsyncBufferView::ID bufferID, Span<char>& data)
{
    AsyncBufferView* buffer = getBuffer(bufferID);
    SC_TRY_MSG(buffer != nullptr, "AsyncBuffersPool::getWritableData - Invalid bufferID");
    SC_TRY_MSG(buffer->type == AsyncBufferView::Type::Writable, "AsyncBuffersPool::getWritableData - Readonly buffer");
    data = buffer->writableData;
    return Result(true);
}

AsyncBufferView* AsyncBuffersPool::getBuffer(AsyncBufferView::ID bufferID)
{
    if (bufferID.identifier >= 0 and buffers.sizeInElements() > unsigned(bufferID.identifier))
    {
        AsyncBufferView* buffer = &buffers[unsigned(bufferID.identifier)];
        return buffer->type != AsyncBufferView::Type::Empty ? buffer : nullptr;
    }
    return nullptr;
}

Result AsyncBuffersPool::requestNewBuffer(size_t minimumSizeInBytes, AsyncBufferView::ID& bufferID, Span<char>& data)
{
    for (AsyncBufferView& buffer : buffers)
    {
        if (buffer.refs == 0 and buffer.writableData.sizeInBytes() >= minimumSizeInBytes)
        {
            buffer.refs = 1;

            switch (buffer.type)
            {
            case AsyncBufferView::Type::Writable: buffer.originalWritableData = buffer.writableData; break;
            case AsyncBufferView::Type::ReadOnly: buffer.originalReadonlyData = buffer.readonlyData; break;
            case AsyncBufferView::Type::Growable: {
                AsyncBufferView::GrowableStorage storage;

                auto da = buffer.getGrowableBuffer(storage, true)->getDirectAccess();

                buffer.writableData         = {static_cast<char*>(da.data), da.sizeInBytes};
                buffer.originalWritableData = {static_cast<char*>(da.data), da.sizeInBytes};
                (void)buffer.getGrowableBuffer(storage, false); // destruct
                break;
            }
            case AsyncBufferView::Type::Empty: SC_ASSERT_RELEASE(false); break;
            }
            bufferID = AsyncBufferView::ID(static_cast<AsyncBufferView::ID::NumericType>(&buffer - buffers.begin()));
            return getWritableData(bufferID, data);
        }
    }
    return Result::Error("AsyncBuffersPool::requestNewBuffer failed");
}

void AsyncBuffersPool::setNewBufferSize(AsyncBufferView::ID bufferID, size_t newSizeInBytes)
{
    AsyncBufferView* buffer = getBuffer(bufferID);
    if (buffer != nullptr)
    {
        switch (buffer->type)
        {
        case AsyncBufferView::Type::Writable:
            if ((newSizeInBytes < buffer->originalWritableData.sizeInBytes()))
            {
                buffer->writableData = {buffer->writableData.data(), newSizeInBytes};
            }
            break;
        case AsyncBufferView::Type::ReadOnly:
            if ((newSizeInBytes < buffer->originalReadonlyData.sizeInBytes()))
            {
                buffer->readonlyData = {buffer->readonlyData.data(), newSizeInBytes};
            }
            break;
        case AsyncBufferView::Type::Growable: {
            AsyncBufferView::GrowableStorage storage;

            IGrowableBuffer* growable = buffer->getGrowableBuffer(storage, true);
            if (growable->resizeWithoutInitializing(newSizeInBytes))
            {
                auto da = growable->getDirectAccess();

                buffer->writableData         = {static_cast<char*>(da.data), da.sizeInBytes};
                buffer->originalWritableData = {static_cast<char*>(da.data), da.sizeInBytes};
            }
            (void)buffer->getGrowableBuffer(storage, false); // destruct
            break;
        }

        break;
        case AsyncBufferView::Type::Empty: SC_ASSERT_RELEASE(false); break;
        }
    }
}

Result AsyncBuffersPool::pushBuffer(AsyncBufferView&& buffer, AsyncBufferView::ID& bufferID)
{
    for (size_t idx = 0; idx < buffers.sizeInElements(); ++idx)
    {
        if (buffers[idx].getType() == AsyncBufferView::Type::Empty)
        {
            buffers[idx] = buffer;
            bufferID     = AsyncBufferView::ID(static_cast<AsyncBufferView::ID::NumericType>(idx));
            return Result(true);
        }
    }
    return Result::Error("pushBuffer failed");
}

//-------------------------------------------------------------------------------------------------------
// AsyncReadableStream
//-------------------------------------------------------------------------------------------------------

Result AsyncReadableStream::init(AsyncBuffersPool& buffersPool, Span<Request> requests)
{
    SC_TRY_MSG(state == State::Stopped, "Can init only in Stopped state")
    buffers   = &buffersPool;
    readQueue = requests;
    state     = State::CanRead;
    return Result(true);
}

Result AsyncReadableStream::start()
{
    SC_TRY_MSG(state == State::CanRead, "Can start only in CanRead state")
    executeRead();
    return Result(true);
}

void AsyncReadableStream::emitOnData()
{
    Request request;
    while (readQueue.popFront(request))
    {
        eventData.emit(request.bufferID);
        buffers->unrefBuffer(request.bufferID); // 1b. refBuffer in push
    }
}

void AsyncReadableStream::push(AsyncBufferView::ID bufferID, size_t newSize)
{
    if (newSize == 0)
    {
        emitError(Result::Error("AsyncReadableStream::push zero sized buffer is not allowed"));
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
    case State::Pausing: {
        // Process buffers received while Pausing is being propagated upstream
        emitOnData();
    }
    break;
    default: {
        emitError(Result::Error("AsyncReadableStream::push - called in wrong state"));
    }
    break;
    }
}

void AsyncReadableStream::reactivate(bool doReactivate)
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

void AsyncReadableStream::pause()
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

void AsyncReadableStream::resumeReading()
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

void AsyncReadableStream::destroy()
{
    switch (state)
    {
    case State::CanRead:
    case State::SyncPushing:
    case State::SyncReadMore:
    case State::Paused:
    case State::Pausing:
    case State::Reading:
    case State::AsyncPushing:
    case State::AsyncReading:
        state = State::Destroyed;
        eventClose.emit();
        break;
    case State::Destroyed: emitError(Result::Error("AsyncReadableStream::destroy - already destroyed")); break;
    case State::Ended: emitError(Result::Error("AsyncReadableStream::destroy - already ended")); break;
    case State::Stopped: emitError(Result::Error("AsyncReadableStream::destroy - already stopped")); break;
    case State::Errored: emitError(Result::Error("AsyncReadableStream::destroy - already in error state")); break;
    }
}

void AsyncReadableStream::executeRead()
{
    state = State::Reading;
    while (true)
    {
        const Result res = asyncRead();
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

void AsyncReadableStream::pushEnd()
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
    case State::Destroyed: emitError(Result::Error("AsyncReadableStream::pushEnd - stream is destroyed")); break;
    case State::Ended: emitError(Result::Error("AsyncReadableStream::pushEnd - stream already ended")); break;
    case State::Stopped: emitError(Result::Error("AsyncReadableStream::pushEnd - stream is not even inited")); break;
    case State::Errored: emitError(Result::Error("AsyncReadableStream::pushEnd - stream is in error state")); break;
    }
}

AsyncBuffersPool& AsyncReadableStream::getBuffersPool() { return *buffers; }

void AsyncReadableStream::emitError(Result error) { eventError.emit(error); }

bool AsyncReadableStream::getBufferOrPause(size_t minumumSizeInBytes, AsyncBufferView::ID& bufferID, Span<char>& data)
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

Result AsyncWritableStream::init(AsyncBuffersPool& buffersPool, Span<Request> requests)
{
    SC_TRY_MSG(state == State::Stopped, "AsyncWritableStream::init - can only be called when stopped");
    buffers    = &buffersPool;
    writeQueue = requests;
    return Result(true);
}

Result AsyncWritableStream::write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb)
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
    resumeWriting();
    return Result(true);
}

Result AsyncWritableStream::write(AsyncBufferView&& bufferView, Function<void(AsyncBufferView::ID)> cb)
{
    AsyncBufferView::ID bufferID;
    SC_TRY(buffers->pushBuffer(move(bufferView), bufferID));
    return write(bufferID, cb);
}

void AsyncWritableStream::resumeWriting()
{
    switch (state)
    {
    case State::Stopped: {
        Request request;
        if (writeQueue.popFront(request))
        {
            state = State::Writing;
            tryAsync(asyncWrite(request.bufferID, request.cb));
            buffers->unrefBuffer(request.bufferID); // 2b. refBuffer above
        }
    }
    break;
    case State::Writing: {
        // This is fine, it has already been queued
    }
    break;
    case State::Ending:
        if (not canEndWritable.isValid() or canEndWritable())
        {
            eventFinish.emit();
            state = State::Ended;
        }
        break;
    case State::Ended: break;
    }
}

Result AsyncWritableStream::unshift(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)>&& cb)
{
    Request request;
    request.cb       = move(cb);
    request.bufferID = bufferID;
    buffers->refBuffer(bufferID);
    // Let's push this request in front instead of to the back
    SC_TRY_MSG(writeQueue.pushFront(request), "unshift failed");
    return Result(true);
}

void AsyncWritableStream::tryAsync(Result potentialError)
{
    if (not potentialError)
    {
        eventError.emit(potentialError);
    }
}

void AsyncWritableStream::finishedWriting(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)>&& callback,
                                          Result res)
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
            if (not canEndWritable.isValid() or canEndWritable())
            {
                state = State::Ended;
            }
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

void AsyncWritableStream::end()
{
    switch (state)
    {
    case State::Stopped:
        if (canEndWritable.isValid())
        {
            if (canEndWritable())
            {
                state = State::Ended;
                eventFinish.emit();
            }
            else
            {
                state = State::Ending;
            }
        }
        else
        {
            // Can just jump to ended state
            state = State::Ended;
            eventFinish.emit();
        }
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

AsyncBuffersPool& AsyncWritableStream::getBuffersPool() { return *buffers; }

void AsyncWritableStream::emitError(Result error) { eventError.emit(error); }
//-------------------------------------------------------------------------------------------------------
// AsyncDuplexStream
//-------------------------------------------------------------------------------------------------------

AsyncDuplexStream::AsyncDuplexStream()
{
    asyncRead.bind([] { return Result(true); });
}

Result AsyncDuplexStream::init(AsyncBuffersPool& buffersPool, Span<AsyncReadableStream::Request> readableRequests,
                               Span<AsyncWritableStream::Request> writableRequests)
{
    SC_TRY(AsyncReadableStream::init(buffersPool, readableRequests));
    SC_TRY(AsyncWritableStream::init(buffersPool, writableRequests));
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// AsyncTransformStream
//-------------------------------------------------------------------------------------------------------
AsyncTransformStream::AsyncTransformStream()
{
    using Self = AsyncTransformStream;
    AsyncWritableStream::asyncWrite.bind<Self, &Self::transform>(*this);
    AsyncWritableStream::canEndWritable.bind<Self, &Self::canEndTransform>(*this);
}

Result AsyncTransformStream::transform(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb)
{
    switch (state)
    {
    case State::None: {
        SC_TRY(AsyncReadableStream::getBuffersPool().getReadableData(bufferID, inputData));
        return prepare(bufferID, cb);
    }
    case State::Paused: {
        SC_TRY_MSG(bufferID == inputBufferID, "Logical Error")
        return prepare(bufferID, cb);
    }
    case State::Finalized: {
        return Result::Error("Transform cannot be called during Finalized State");
    }
    case State::Processing: {
        return Result::Error("Transform cannot be called during Processing State");
    }
    case State::Finalizing: {
        return Result::Error("Transform cannot be called during Finalizing State");
    }
    }
    return Result(true);
}

Result AsyncTransformStream::prepare(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb)
{
    inputCallback = move(cb);
    inputBufferID = bufferID;
    if (getBufferOrPause(0, outputBufferID, outputData))
    {
        state = State::Processing;
        return onProcess(inputData, outputData);
    }
    else
    {
        state = State::Paused;
        SC_TRY(AsyncWritableStream::unshift(inputBufferID, move(inputCallback)));
        AsyncWritableStream::stop();
        return Result(true);
    }
}

void AsyncTransformStream::afterProcess(Span<const char> inputAfter, Span<char> outputAfter)
{
    const size_t consumedOutput = outputData.sizeInBytes() - outputAfter.sizeInBytes();
    if (consumedOutput > 0)
    {
        AsyncReadableStream::push(outputBufferID, consumedOutput);
    }
    AsyncReadableStream::getBuffersPool().unrefBuffer(outputBufferID);
    if (inputAfter.empty())
    {
        auto cb        = move(inputCallback);
        auto bufferID  = inputBufferID;
        inputCallback  = {};
        inputBufferID  = {};
        inputData      = {};
        outputBufferID = {};
        outputData     = {};
        state          = State::None;
        AsyncWritableStream::finishedWriting(bufferID, move(cb), Result(true));
    }
    else
    {
        inputData = inputAfter;

        state = State::Paused;
        tryAsync(transform(inputBufferID, inputCallback));
    }
}

void AsyncTransformStream::afterFinalize(Span<char> outputAfter, bool streamEnded)
{
    const size_t consumedOutput = outputData.sizeInBytes() - outputAfter.sizeInBytes();
    if (consumedOutput > 0)
    {
        AsyncReadableStream::push(outputBufferID, consumedOutput);
    }
    AsyncReadableStream::getBuffersPool().unrefBuffer(outputBufferID);
    if (streamEnded)
    {
        AsyncReadableStream::pushEnd();
        state = State::Finalized; // --> Transition to ENDED (all data written)
    }
    else
    {
        state = State::Paused;
        tryFinalize();
    }
}

void AsyncTransformStream::tryFinalize()
{
    if (getBufferOrPause(0, outputBufferID, outputData))
    {
        state      = State::Finalizing; // Retry later when we can get some memory
        Result res = onFinalize(outputData);
        if (not res)
        {
            AsyncWritableStream::emitError(Result::Error("AsyncTransformStream::onFinalize error"));
            state = State::None; // --> Transition to ENDED (unrecoverable error)
        }
    }
    else
    {
        state = State::Paused; // Retry later when we can get some memory
    }
}

bool AsyncTransformStream::canEndTransform()
{
    switch (state)
    {
    case State::None:
    case State::Paused: {
        tryFinalize();
        return state == State::Finalized;
    }
    case State::Finalized: {
        return true;
    }
    case State::Finalizing:
    case State::Processing: {
        return false; // Still processing stuff
    }
    }
    Assert::unreachable();
}

//-------------------------------------------------------------------------------------------------------
// AsyncPipeline
//-------------------------------------------------------------------------------------------------------

AsyncPipeline::~AsyncPipeline() { SC_ASSERT_DEBUG(unpipe()); }

Result AsyncPipeline::validate()
{
    int validSinks = 0;
    for (size_t idx = 0; idx < MaxSinks; ++idx)
    {
        if (sinks[idx] != nullptr)
            validSinks++;
    }
    SC_TRY_MSG(source != nullptr, "AsyncPipeline::validate() invalid source");
    SC_TRY_MSG(validSinks > 0, "AsyncPipeline::validate() invalid 0 sized list of sinks");
    return Result(true);
}

Result AsyncPipeline::pipe()
{
    SC_TRY(validate());
    SC_TRY(checkBuffersPool());

    AsyncReadableStream* readable = source;
    SC_TRY(chainTransforms(readable));

    bool res;
    res = readable->eventData.addListener<AsyncPipeline, &AsyncPipeline::dispatchToPipes>(*this);
    SC_TRY_MSG(res, "AsyncPipeline::pipe() run out of eventData");
    res = readable->eventEnd.addListener<AsyncPipeline, &AsyncPipeline::endPipes>(*this);
    SC_TRY_MSG(res, "AsyncPipeline::pipe() run out of eventEnd");
    res = readable->eventError.addListener<AsyncPipeline, &AsyncPipeline::emitError>(*this);
    SC_TRY_MSG(res, "AsyncPipeline::pipe() run out of eventError");
    for (AsyncWritableStream* sink : sinks)
    {
        if (sink == nullptr)
            break;
        res = sink->eventError.addListener<AsyncPipeline, &AsyncPipeline::emitError>(*this);
        SC_TRY_MSG(res, "AsyncPipeline::pipe() pipe run out of eventError");
    }
    return Result(true);
}

bool AsyncPipeline::unpipe()
{
    bool res = true;
    // Deregister all source events
    if (source)
    {
        int validTransforms = 0;
        for (size_t idx = 0; idx < MaxTransforms; ++idx)
        {
            if (transforms[idx] != nullptr)
                validTransforms++;
        }
        if (validTransforms == 0)
        {
            res = source->eventData.removeAllListenersBoundTo(*this);
            SC_TRY(res);
            res = source->eventEnd.removeAllListenersBoundTo(*this);
            SC_TRY(res);
        }
        else
        {
            AsyncWritableStream& writable = *transforms[0];

            res = listenToEventData(*source, *transforms[0], false);
            SC_TRY(res);
            res = source->eventEnd.removeAllListenersBoundTo(writable);
            SC_TRY(res);
        }
        res = source->eventError.removeAllListenersBoundTo(*this);
        SC_TRY(res);
        source = nullptr;
    }

    // Deregister all transforms events
    for (size_t idx = 0; idx < MaxTransforms; ++idx)
    {
        AsyncDuplexStream* transform = transforms[idx];
        if (transform == nullptr)
            break;
        if ((idx + 1 == MaxTransforms) or transforms[idx + 1] == nullptr)
        {
            res = transform->eventData.removeAllListenersBoundTo(*this);
            SC_TRY(res);
            res = transform->eventEnd.removeAllListenersBoundTo(*this);
            SC_TRY(res);
            break;
        }
        else
        {
            AsyncDuplexStream&   nextTransform = *transforms[idx + 1];
            AsyncWritableStream& nextWritable  = nextTransform;

            res = listenToEventData(*transform, nextTransform, false);
            SC_TRY(res);
            res = source->eventEnd.removeAllListenersBoundTo(nextWritable);
            SC_TRY(res);
        }
        res = transform->AsyncReadableStream::eventError.removeAllListenersBoundTo(*this);
        SC_TRY(res);
        res = transform->AsyncWritableStream::eventError.removeAllListenersBoundTo(*this);
        SC_TRY(res);
    }
    for (size_t idx = 0; idx < MaxTransforms; ++idx)
        transforms[idx] = nullptr;

    // Deregister all sinks events
    for (AsyncWritableStream* sink : sinks)
    {
        if (sink == nullptr)
            break;
        res = sink->eventError.removeListener<AsyncPipeline, &AsyncPipeline::emitError>(*this);
        SC_TRY(res);
    }
    for (size_t idx = 0; idx < MaxSinks; ++idx)
        sinks[idx] = nullptr;
    return true;
}

Result AsyncPipeline::start()
{
    SC_TRY(validate());
    for (auto transform : transforms)
    {
        if (transform == nullptr)
            break;
        SC_TRY(transform->start());
    }
    SC_TRY(source->start());
    return Result(true);
}

void AsyncPipeline::emitError(Result res) { eventError.emit(res); }

Result AsyncPipeline::checkBuffersPool()
{
    AsyncBuffersPool& buffers = source->getBuffersPool();

    for (AsyncWritableStream* sink : sinks)
    {
        if (sink == nullptr)
            break;
        if (&sink->getBuffersPool() != &buffers)
        {
            return Result::Error("AsyncPipeline::start - all streams must use the same AsyncBuffersPool");
        }
    }
    for (AsyncDuplexStream* transform : transforms)
    {
        if (transform == nullptr)
            break;
        if ((&transform->AsyncReadableStream::getBuffersPool() != &buffers) and
            (&transform->AsyncWritableStream::getBuffersPool() != &buffers))
        {
            return Result::Error("AsyncPipeline::start - all streams must use the same AsyncBuffersPool");
        }
    }
    return Result(true);
}

bool AsyncPipeline::listenToEventData(AsyncReadableStream& readable, AsyncDuplexStream& transform, bool listen)
{
    AsyncDuplexStream* pTransform = &transform;

    auto lambda = [this, pTransform](AsyncBufferView::ID bufferID)
    {
        // Write readable to transform
        asyncWriteWritable(bufferID, *pTransform);
    };
    if (listen)
    {
        return readable.eventData.addListener(lambda);
    }
    else
    {
        return readable.eventData.removeListener(lambda);
    }
}

Result AsyncPipeline::chainTransforms(AsyncReadableStream*& readable)
{
    for (AsyncDuplexStream* transform : transforms)
    {
        if (transform == nullptr)
        {
            break;
        }
        bool res;
        res = listenToEventData(*readable, *transform, true);
        SC_TRY_MSG(res, "AsyncPipeline::chainTransforms run out of eventData");
        AsyncWritableStream& writable = *transform;
        res = readable->eventEnd.addListener<AsyncWritableStream, &AsyncWritableStream::end>(writable);
        SC_TRY_MSG(res, "AsyncPipeline::chainTransforms run out of eventEnd");
        res = readable->eventError.addListener<AsyncPipeline, &AsyncPipeline::emitError>(*this);
        SC_TRY_MSG(res, "AsyncPipeline::chainTransforms run out of eventError");

        readable = transform;

        res = transform->AsyncReadableStream::eventError.addListener<AsyncPipeline, &AsyncPipeline::emitError>(*this);
        SC_TRY_MSG(res, "AsyncPipeline::chainTransforms run out of eventError");

        res = transform->AsyncWritableStream::eventError.addListener<AsyncPipeline, &AsyncPipeline::emitError>(*this);
        SC_TRY_MSG(res, "AsyncPipeline::chainTransforms run out of eventError");
    }
    return Result(true);
}

void AsyncPipeline::asyncWriteWritable(AsyncBufferView::ID bufferID, AsyncWritableStream& writable)
{
    source->getBuffersPool().refBuffer(bufferID);
    Function<void(AsyncBufferView::ID)> func;
    func.template bind<AsyncPipeline, &AsyncPipeline::afterWrite>(*this);
    // TODO: We should probably block when closing for in-flight writes
    Result res = writable.write(bufferID, func);
    if (not res)
    {
        eventError.emit(res);
    }
}

void AsyncPipeline::afterWrite(AsyncBufferView::ID bufferID)
{
    source->getBuffersPool().unrefBuffer(bufferID);
    // Try resume in reverse
    for (size_t idx = 0; idx < MaxTransforms; ++idx)
    {
        AsyncDuplexStream* transform = transforms[MaxTransforms - 1 - idx];
        if (transform)
        {
            transform->resumeWriting();
            transform->resumeReading();
        }
    }
    source->resumeReading();
}

void AsyncPipeline::dispatchToPipes(AsyncBufferView::ID bufferID)
{
    for (AsyncWritableStream* sink : sinks)
    {
        if (sink == nullptr)
            break;
        asyncWriteWritable(bufferID, *sink);
    }
}

void AsyncPipeline::endPipes()
{
    for (AsyncWritableStream* sink : sinks)
    {
        if (sink == nullptr)
            break;
        sink->end();
    }
}

} // namespace SC
