// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Function.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/StrongID.h"
#include "Internal/CircularQueue.h"
#include "Internal/Event.h"

//! @defgroup group_async_streams Async Streams
//! Read, transform and write data concurrently from async sources to destinations.
//!
/// Read, writes and transforms happen in parallel if sources and destinations are asynchronous.
/// This library does not allocate any memory, all buffers are supplied by the caller.
///
/// Async Streams are largely inspired by [node.js Streams](https://nodejs.org/api/stream.html), a very powerful tool to
/// process large amounts of data concurrently.
///
/// The basic idea about an async stream is to create a Source / Sink abstraction (also called Readable and Writable)
/// and process small buffers of data at time.
///
/// The state machine that coordinates this interaction handles data buffering and more importantly handles also
/// back-pressure, that means:
///
/// - **Pausing** the readable stream when a connected writable stream cannot process data fast enough
/// - **Resuming** the readable stream when a connected writable stream is finally able to receive more data
///
/// By implementing streams on top of async operations it's possible to run many of them concurrently very efficiently.
/// When properly implemented for example an async pipeline can concurrently read from disk, write to a socket while
/// compressing data.
///
///
/// Most notable differences with node.js streams are for now:
///
/// - No allocation (designed to work inside user-provided list of buffers)
/// - No object mode
/// - Fixed Layout to create data pipelines (AsyncPipeline)
/// - onData support only (no readable event)

//! @addtogroup group_async_streams
//! @{
namespace SC
{

/// @brief A Span of bytes memory to be read or written by async streams
struct AsyncBufferView
{
    struct Tag
    {
    };
    using ID = StrongID<Tag>;
    Span<char> data;

  private:
    Span<char> originalData;
    friend struct AsyncBuffersPool;

    int32_t refs = 0; // Counts AsyncReadable (single) or AsyncWritable (multiple) using it
};

/// @brief Holds a Span of AsyncBufferView (allocated by user) holding available memory for the streams
/// @note User must fill the AsyncBuffersPool::buffers with a `Span` of AsyncBufferView
struct AsyncBuffersPool
{
    /// @brief Span of buffers to be filled in by the user
    Span<AsyncBufferView> buffers;

    /// @brief Increments a buffer reference count
    void refBuffer(AsyncBufferView::ID bufferID);

    /// @brief Decrements a buffer reference count.
    /// When reference count becomes zero the buffer will be re-used
    void unrefBuffer(AsyncBufferView::ID bufferID);

    /// @brief Access data span owned by the buffer
    Result getData(AsyncBufferView::ID bufferID, Span<const char>& data);

    /// @brief Access data span owned by the buffer
    Result getData(AsyncBufferView::ID bufferID, Span<char>& data);

    /// @brief Access the raw AsyncBufferView (if any) at a given bufferID (or nullptr if invalid)
    AsyncBufferView* getBuffer(AsyncBufferView::ID bufferID);

    /// @brief Requests a new available buffer that is at least minimumSizeInBytes, incrementing its refcount
    Result requestNewBuffer(size_t minimumSizeInBytes, AsyncBufferView::ID& bufferID, Span<char>& data);

    /// @brief Sets the new size in bytes for the buffer
    void setNewBufferSize(AsyncBufferView::ID bufferID, size_t newSizeInBytes);
};

/// @brief Async source abstraction emitting data events in caller provided byte buffers.
/// After AsyncReadableStream::start it will start emitting AsyncReadableStream::eventData with buffers.
/// User must provide a custom async red implementation in AsyncReadableStream::asyncRead.
/// The stream must be paused when the AsyncBuffersPool is full (use AsyncReadableStream::getBufferOrPause).
/// Once the stream is ended, it will emit AsyncReadableStream::eventEnd and it cannot be used further.
/// AsyncReadableStream::eventError will be emitted when an error occurs in any phase.
struct AsyncReadableStream
{
    struct Request
    {
        AsyncBufferView::ID bufferID;
    };
    /// @brief Function that every stream must define to implement its custom read operation
    Function<Result()> asyncRead;

    static constexpr int MaxListeners = 8;

    Event<MaxListeners, Result>              eventError; /// Emitted when an error occurs
    Event<MaxListeners, AsyncBufferView::ID> eventData;  /// Emitted when a new buffer has been read
    Event<MaxListeners>                      eventEnd;   /// Emitted when there is no more data
    Event<MaxListeners>                      eventClose; /// Emitted when the underlying resource has been closed

    /// @brief Inits the readable stream with an AsyncBuffersPool instance that will provide memory for it
    /// @param buffersPool An instance of AsyncBuffersPool providing read buffers
    /// @param requests User owned memory to hold a circular buffer for read requests
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests);

    /// @brief Starts the readable stream, that will emit eventData
    Result start();

    /// @brief Pauses the readable stream (that can be later resumed)
    void pause();

    /// @brief Resumes the readable stream paused by AsyncReadableStream::pause
    void resume();

    /// @brief Forcefully destroys the readable stream before it's end event releasing all resources
    void destroy();

    /// @brief Returns true if the stream is ended (AsyncReadableStream::end has been called)
    [[nodiscard]] bool isEnded() const { return state == State::Ended; }

    /// @brief Obtains the AsyncBuffersPool to request more buffers
    AsyncBuffersPool& getBuffersPool();

    /// @brief Use push from inside AsyncReadableStream::asyncRead function to queue received data
    void push(AsyncBufferView::ID bufferID, size_t newSize);

    /// @brief Use pushEnd from inside AsyncReadableStream::asyncRead to signal production end
    void pushEnd();

    /// @brief Use reactivate(true) from inside AsyncReadableStream::asyncRead function to ask the
    /// state machine to invoke asyncRead again.
    void reactivate(bool doReactivate);

    /// @brief Signals an async error received
    void emitError(Result error);

    /// @brief Returns an unused buffer from pool or pauses the stream if none is available
    [[nodiscard]] bool getBufferOrPause(size_t minumumSizeInBytes, AsyncBufferView::ID& bufferID, Span<char>& data);

  private:
    void emitOnData();
    void executeRead();

    enum class State
    {
        Stopped,      // Stream must be inited
        CanRead,      // Stream is ready to issue a read ( AsyncReadableStream::start / AsyncReadableStream::resume)
        Reading,      // A read is being issued (may be sync or async)
        SyncPushing,  // One or multiple AsyncReadableStream::push have been received (sync)
        SyncReadMore, // SyncPushing + AsyncReadableStream::reactivate(true)
        AsyncReading, // An async read is in flight
        AsyncPushing, // AsyncReading + AsyncReadableStream::push
        Pausing,      // Pause requested while read in flight
        Paused,       // Actually paused with no read in flight
        Ended,        // Emitted all data, no more data will be emitted
        Destroying,   // Readable is waiting for async call before
        Destroyed,    // Readable has been destroyed before emitting all data
        Errored,      // Error occurred
    };
    State state = State::Stopped;

    AsyncBuffersPool* buffers = nullptr;

    CircularQueue<Request> readQueue;
};

/// @brief Async destination abstraction where bytes can be written to.
/// When buffers are pushed faster than the stream can handle, they will get queued.
/// Queuing process happens with a linked list stored in the AsyncBufferView itself.
/// As AsyncBufferView contains a fixed (at init) number of buffers, the queue is bounded
/// by the fact that user will be unable to allocate buffers to write until at least one
/// will be made available again (i.e. a write finishes).
/// User can listen to AsyncWritableStream::eventWritten to know when a buffer is written
/// (and its refcount decreased) or AsyncWritableStream::eventDrain when the queue is empty.
struct AsyncWritableStream
{
    /// @brief Function that every stream must define to implement its custom write operation
    Function<Result(AsyncBufferView::ID, Function<void(AsyncBufferView::ID)>)> asyncWrite;

    struct Request
    {
        AsyncBufferView::ID bufferID;

        Function<void(AsyncBufferView::ID)> cb;
    };
    static constexpr int MaxListeners = 8;

    Event<MaxListeners, Result> eventError;  /// Emitted when an error occurs
    Event<MaxListeners>         eventDrain;  /// Emitted when write queue is empty
    Event<MaxListeners>         eventFinish; /// Emitted when no more data can be written

    /// @brief Inits the writable stream
    /// @param buffersPool An instance of AsyncBuffersPool providing write buffers
    /// @param requests User owned memory to hold a circular buffer for write requests
    Result init(AsyncBuffersPool& buffersPool, Span<Request> requests);

    /// @brief Writes a buffer (that must be allocated by the AsyncBuffersPool passed in AsyncWritableStream)
    /// When the buffer it will be actually written, AsyncWritableStream::eventWritten will be raised and
    /// its reference count will be decreased.
    /// @param bufferID Buffer allocated from the associated AsyncBuffersPool (AsyncWritableStream::getBuffersPool)
    /// @param cb Callback that will be invoked when the write is finished
    /// @return Invalid Result if write queue is full
    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb = {});

    /// @brief Try requesting a buffer big enough and copy data into it
    /// @return Invalid Result if write queue is full or if there are no available buffers in the pool
    Result write(Span<const char> data, Function<void(AsyncBufferView::ID)> cb = {});

    /// @brief Write a C-string literal in the stream
    /// @return Invalid Result if write queue is full or if there are no available buffers in the pool
    template <size_t N>
    [[nodiscard]] Result write(const char (&str)[N])
    {
        return write(Span<const char>(str, N - 1));
    }

    /// @brief Ends the writable stream, waiting for all in-flight and queued writes to finish.
    /// After this happens, AsyncWritableStream::eventFinished will be raised
    void end();

    /// @brief Obtains the buffers pool to access its data
    AsyncBuffersPool& getBuffersPool();

    /// @brief Signals that the given buffer (previously queued by write) has been fully written
    void finishedWriting(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)>&& cb, Result res);

    /// @brief Signals an async error received
    void emitError(Result error);

  private:
    bool tryAsync(Result potentialError);

    enum class State
    {
        Stopped,
        Writing,
        Ending,
        Ended
    };
    State state = State::Stopped;

    AsyncBuffersPool* buffers = nullptr;

    CircularQueue<Request> writeQueue;
};

/// @brief Pipes reads on SC::AsyncReadableStream to SC::AsyncWritableStream.
/// Back-pressure happens when the source provides data at a faster rate than what the sink (writable)
/// is able to process.
/// When this happens, AsyncPipeline will AsyncReadableStream::pause the (source).
/// It will also AsyncReadableStream::resume it when some writable has finished writing, freeing one buffer.
/// Caller needs to set AsyncPipeline::source field and AsyncPipeline::sinks with valid streams.
/// @note It's crucial to use the same AsyncBuffersPool for the AsyncReadableStream and all AsyncWritableStream
struct AsyncPipeline
{
    static constexpr int MaxListeners = 8;

    Event<MaxListeners, Result> eventError; /// Emitted when an error occurs

    // TODO: Make all these private
    AsyncReadableStream* source = nullptr; /// User specified source

    Span<AsyncWritableStream*> sinks; /// User specified sinks

    /// @brief Starts the pipeline
    /// @note Both source and sinks must have been already setup by the caller
    Result start();

    // TODO: Add a pause and cancel/step
  private:
    void onBufferRead(AsyncBufferView::ID bufferID);
    void onBufferWritten(AsyncBufferView::ID bufferID);

    void endPipes();
};
} // namespace SC
//! @}
