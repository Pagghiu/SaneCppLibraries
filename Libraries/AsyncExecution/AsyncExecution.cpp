// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncExecution.h"

#if SC_ASYNC_EXECUTION_AVAILABLE
#include "../Strings/Path.h"
#include <stdlib.h>

namespace SC
{
namespace detail
{
static constexpr uint64_t AsyncExecutionMaximumFileOffset = uint64_t(-1) >> 1;
static constexpr size_t   AsyncExecutionFileChunk         = 16384;

static Result AsyncExecutionValidateFileRange(const AsyncExecutionFileOptions& options, size_t bytes)
{
    if (options.useOffset and (options.offset > AsyncExecutionMaximumFileOffset or
                               uint64_t(bytes) > AsyncExecutionMaximumFileOffset - options.offset))
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::InvalidFileOffset);
    if (options.threadPoolMode == AsyncThreadPoolMode::ForceThreadPool and options.threadPool == nullptr)
        return Result::Error(AsyncResultCategory, AsyncError::MissingThreadPool);
    return Result(true);
}

static Result AsyncExecutionValidateSingleTransfer(size_t bytes)
{
    if (uint64_t(bytes) > uint64_t(uint32_t(-1)))
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow);
    return Result(true);
}

template <typename Request>
static Result AsyncExecutionPrepareFile(Request& request, AsyncTaskSequence& task,
                                        const AsyncExecutionFileOptions& options, size_t transferred = 0)
{
    SC_TRY(AsyncExecutionValidateFileRange(options, transferred));
    if (options.useOffset)
        request.setOffset(options.offset + transferred);
    if (options.threadPool)
        SC_TRY(request.executeOn(task, *options.threadPool, options.threadPoolMode));
    return Result(true);
}
} // namespace detail

AsyncExecutionContext::AsyncExecutionContext(AsyncEventLoop& loop) : loop(loop) {}

AsyncExecutionContext::~AsyncExecutionContext() { SC_ASYNC_ASSERT_DEBUG(not accepting.load() and not closing); }

Result AsyncExecutionContext::create()
{
    if (accepting.load() or closing)
        return Result::Error(AsyncResultCategory, AsyncError::AlreadyInitialized);
    ownerThread               = Thread::CurrentThreadID();
    dispatcherWakeUp.callback = [this](AsyncLoopWakeUp::Result& result)
    {
        result.reactivateRequest(true);
        dispatching = true;
        while (auto* command = pop())
            command->execute(*command); // Commands may destroy themselves. Do not access them afterward.
        dispatching = false;
    };
    SC_TRY(dispatcherWakeUp.start(loop));
    accepting.store(true);
    return Result(true);
}

Result AsyncExecutionContext::close()
{
    if (not accepting.load() and not closing)
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    // Read outstanding before notification counts: workers publish their notification before dropping outstanding.
    if (Thread::CurrentThreadID() != ownerThread or running or dispatching or outstanding.load() != 0 or
        poolNotifications.load() != 0 or delivering.load() != 0)
        return Result::Error(AsyncResultCategory, AsyncError::ActiveRequestsRemain);
    // Callers must stop submitting and join producer threads before closing the context.
    accepting.store(false);
    closing = true;
    if (not dispatcherWakeUp.isFree() and not dispatcherWakeUp.isCancelling())
        SC_TRY(dispatcherWakeUp.stop(loop));
    SC_TRY(loop.runNoWait());
    if (not dispatcherWakeUp.isFree())
        return Result::Error(AsyncResultCategory, AsyncError::ActiveRequestsRemain);
    closing = false;
    return Result(true);
}

Result AsyncExecutionContext::post(detail::AsyncExecutionCommand& command) noexcept
{
    mutex.lock();
    if (not accepting.load())
    {
        mutex.unlock();
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    }
    if (command.queued)
    {
        mutex.unlock();
        return Result(true);
    }
    command.next   = nullptr;
    command.queued = true;
    auto* previous = tail;
    if (tail)
        tail->next = &command;
    else
        head = &command;
    tail                = &command;
    const Result status = dispatcherWakeUp.wakeUp(loop);
    if (not status)
    {
        if (previous)
            previous->next = nullptr;
        else
            head = nullptr;
        tail           = previous;
        command.queued = false;
    }
    mutex.unlock();
    return status;
}

void AsyncExecutionContext::remove(detail::AsyncExecutionCommand& command) noexcept
{
    mutex.lock();
    if (command.queued)
    {
        detail::AsyncExecutionCommand* previous = nullptr;
        auto*                          current  = head;
        while (current != &command)
        {
            previous = current;
            current  = current->next;
        }
        if (previous)
            previous->next = command.next;
        else
            head = command.next;
        if (tail == &command)
            tail = previous;
        command.next   = nullptr;
        command.queued = false;
    }
    mutex.unlock();
}

detail::AsyncExecutionCommand* AsyncExecutionContext::pop() noexcept
{
    mutex.lock();
    auto* command = head;
    if (command)
    {
        head = command->next;
        if (not head)
            tail = nullptr;
        command->next   = nullptr;
        command->queued = false;
    }
    mutex.unlock();
    return command;
}

detail::AsyncExecutionDelivery::AsyncExecutionDelivery(AsyncExecutionContext& context) noexcept : context(context)
{
    context.delivering.fetch_add(1);
}

detail::AsyncExecutionDelivery::~AsyncExecutionDelivery() { context.delivering.fetch_sub(1); }

AsyncExecutionNotification::AsyncExecutionNotification(AsyncExecutionContext& context) noexcept : context(context)
{
    command.operation = this;
    command.execute   = [](detail::AsyncExecutionCommand& command) noexcept
    { static_cast<AsyncExecutionNotification*>(command.operation)->dispatch(); };
}

AsyncExecutionNotification::~AsyncExecutionNotification()
{
    SC_ASYNC_ASSERT_DEBUG(waiter == nullptr and not command.queued);
}

Result AsyncExecutionNotification::notify() noexcept
{
    mutex.lock();
    pending             = true;
    const Result status = waiter ? context.post(command) : Result(true);
    mutex.unlock();
    return status;
}

Result AsyncExecutionNotification::attach(detail::AsyncExecutionNotificationWaiter& incoming) noexcept
{
    mutex.lock();
    if (waiter)
    {
        mutex.unlock();
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::NotificationAlreadyWaiting);
    }
    waiter              = &incoming;
    const Result status = pending ? context.post(command) : Result(true);
    if (not status)
        waiter = nullptr;
    mutex.unlock();
    return status;
}

void AsyncExecutionNotification::detach(detail::AsyncExecutionNotificationWaiter& outgoing) noexcept
{
    mutex.lock();
    if (waiter == &outgoing)
    {
        waiter = nullptr;
        context.remove(command);
    }
    mutex.unlock();
}

void AsyncExecutionNotification::dispatch() noexcept
{
    mutex.lock();
    auto* selected = waiter;
    // A producer may have requeued this command after pop() and before this lock.
    context.remove(command);
    waiter  = nullptr;
    pending = false;
    mutex.unlock();
    if (selected)
        selected->deliver(selected->operation); // Delivery may destroy the notification and connected storage.
}

AsyncExecutionNotificationSender AsyncExecutionContext::wakeUp(AsyncExecutionNotification& notification)
{
    return {*this, notification};
}

AsyncExecutionExternalCompletion::AsyncExecutionExternalCompletion(AsyncExecutionContext& context) noexcept
    : context(context)
{
    command.operation = this;
    command.execute   = [](detail::AsyncExecutionCommand& command) noexcept
    { static_cast<AsyncExecutionExternalCompletion*>(command.operation)->dispatch(); };
}

AsyncExecutionExternalCompletion::~AsyncExecutionExternalCompletion()
{
    SC_ASYNC_ASSERT_DEBUG(waiter == nullptr and not command.queued);
}

Result AsyncExecutionExternalCompletion::complete(Result producerStatus, size_t transferred) noexcept
{
    mutex.lock();
    if (completed)
    {
        mutex.unlock();
        return Result::Error(AsyncResultCategory, AsyncError::CompletionAlreadyPosted);
    }
    status                   = producerStatus;
    bytes                    = transferred;
    const Result publication = waiter ? context.post(command) : Result(true);
    if (publication)
        completed = true;
    mutex.unlock();
    return publication;
}

Result AsyncExecutionExternalCompletion::attach(AsyncExecutionContext&                          incomingContext,
                                                detail::AsyncExecutionExternalCompletionWaiter& incoming) noexcept
{
    if (&context != &incomingContext)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::ExternalCompletionContextMismatch);
    mutex.lock();
    if (waiter or consumed)
    {
        const auto error = consumed ? AsyncExecutionError::ExternalCompletionConsumed
                                    : AsyncExecutionError::ExternalCompletionAlreadyWaiting;
        mutex.unlock();
        return Result::Error(AsyncExecutionResultCategory, error);
    }
    waiter                   = &incoming;
    const Result publication = completed ? context.post(command) : Result(true);
    if (not publication)
        waiter = nullptr;
    mutex.unlock();
    return publication;
}

void AsyncExecutionExternalCompletion::detach(detail::AsyncExecutionExternalCompletionWaiter& outgoing) noexcept
{
    mutex.lock();
    if (waiter == &outgoing)
    {
        waiter = nullptr;
        context.remove(command);
    }
    mutex.unlock();
}

void AsyncExecutionExternalCompletion::dispatch() noexcept
{
    mutex.lock();
    auto*        selected       = waiter;
    const Result producerStatus = status;
    const size_t transferred    = bytes;
    SC_ASYNC_ASSERT_DEBUG(completed and selected != nullptr);
    context.remove(command);
    waiter   = nullptr;
    consumed = true;
    mutex.unlock();
    selected->deliver(selected->operation, producerStatus, transferred);
}

AsyncExecutionExternalCompletionSender AsyncExecutionContext::externalCompletion(
    AsyncExecutionExternalCompletion& completion)
{
    return {*this, completion};
}

Result detail::AsyncExecutionPoolTask::submit(AsyncExecutionContext& executionContext, ThreadPool& pool,
                                              void* executionOperation, Notify delivery) noexcept
{
    context       = &executionContext;
    operation     = executionOperation;
    notify        = delivery;
    task.function = []() {};
    // Only the pool's local callback copy remains in use. Receiver delivery may destroy the embedded task.
    // The context/pool remain borrowed through notification return; never access operation after notify.
    task.afterRelease = [this]()
    {
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
        auto scopeAssociation = move(association);
#endif
        auto*      executionOperation = operation;
        const auto delivery           = notify;
        auto*      executionContext   = context;
        executionContext->poolNotifications.fetch_add(1);
        executionContext->outstanding.fetch_sub(1);
        delivery(executionOperation);
        executionContext->poolNotifications.fetch_sub(1);
    };
    executionContext.outstanding.fetch_add(1);
    const Result queued = pool.queueTask(task);
    if (not queued)
    {
        executionContext.outstanding.fetch_sub(1);
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
        association = {};
#endif
    }
    return queued;
}

Result detail::AsyncExecutionTimer::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, duration);
}

Result detail::AsyncExecutionFileSystemBase::prepare(Request& request) const
{
    return request.setThreadPool(pool, poolMode);
}

detail::AsyncExecutionFSOpen::AsyncExecutionFSOpen(ThreadPool& pool, StringSpan inputPath, FileOpen mode,
                                                   AsyncThreadPoolMode poolMode)
    : AsyncExecutionFileSystemBase{pool, poolMode}, mode(mode)
{
    initialization = inputPath.isEmpty() ? Result::Error(AsyncResultCategory, AsyncError::InvalidPath)
                                         : AsyncExecutionResolvePath(inputPath, path);
}

Result detail::AsyncExecutionFSOpen::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    SC_TRY(initialization);
    SC_TRY(prepare(request));
    return request.open(loop, path.view(), mode);
}

void detail::AsyncExecutionFSOpen::observe(Request::Result& result)
{
    if (result.isValid())
        SC_ASYNC_TRUST_RESULT(opened.assign(result.completionData.handle));
}

Result detail::AsyncExecutionFSOpen::discard() { return Result::Explicit(opened.close()); }

AsyncExecutionFile detail::AsyncExecutionFSOpen::value(Request::Result&) { return AsyncExecutionFile{move(opened)}; }

Result detail::AsyncExecutionFSClose::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TRY(file.get(handle, Result::Error(AsyncResultCategory, AsyncError::InvalidFileHandle)));
    SC_TRY(prepare(request));
    return request.close(loop, handle);
}

void detail::AsyncExecutionFSClose::observe(Request::Result& result)
{
    const Result status = result.isValid();
    // These are completion-stage failures: pool FileDescriptor::close or an io_uring close CQE.
    // Validation/submission/queue errors retain the caller's descriptor. Never retry an attempted native close.
    if (status or status.isError(FileResultCategory, FileError::CloseFailed) or
        status.isError(AsyncResultCategory, AsyncError::EventCompletionFailed))
        file.detach();
}

Result detail::AsyncExecutionFSRead::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    const AsyncExecutionFileOptions options{nullptr, offset, true};
    SC_TRY(AsyncExecutionValidateFileRange(options, buffer.sizeInBytes()));
    SC_TRY(AsyncExecutionValidateSingleTransfer(buffer.sizeInBytes()));
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TRY(file.get(handle, Result::Error(AsyncResultCategory, AsyncError::InvalidFileHandle)));
    SC_TRY(prepare(request));
    return request.read(loop, handle, buffer, offset);
}

AsyncExecutionReadResult detail::AsyncExecutionFSRead::value(Request::Result& result)
{
    return {{buffer.data(), result.completionData.numBytes}, result.completionData.numBytes == 0};
}

Result detail::AsyncExecutionFSWrite::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    const AsyncExecutionFileOptions options{nullptr, offset, true};
    SC_TRY(AsyncExecutionValidateFileRange(options, data.sizeInBytes()));
    SC_TRY(AsyncExecutionValidateSingleTransfer(data.sizeInBytes()));
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TRY(file.get(handle, Result::Error(AsyncResultCategory, AsyncError::InvalidFileHandle)));
    SC_TRY(prepare(request));
    return request.write(loop, handle, data, offset);
}

AsyncExecutionFile& AsyncExecutionFile::operator=(AsyncExecutionFile&& other) noexcept
{
    file = move(other.file);
    return *this;
}

AsyncExecutionSender<detail::AsyncExecutionFSOpen> AsyncExecutionContext::fsOpen(ThreadPool& pool, StringSpan path,
                                                                                 FileOpen            mode,
                                                                                 AsyncThreadPoolMode poolMode)
{
    return {*this, {pool, path, mode, poolMode}};
}

AsyncExecutionSender<detail::AsyncExecutionFSClose> AsyncExecutionContext::fsClose(ThreadPool&         pool,
                                                                                   FileDescriptor&     file,
                                                                                   AsyncThreadPoolMode poolMode)
{
    return {*this, {{pool, poolMode}, file}};
}

AsyncExecutionSender<detail::AsyncExecutionFSRead> AsyncExecutionContext::fsRead(ThreadPool&           pool,
                                                                                 const FileDescriptor& file,
                                                                                 Span<char> buffer, uint64_t offset,
                                                                                 AsyncThreadPoolMode poolMode)
{
    return {*this, {{pool, poolMode}, file, buffer, offset}};
}

AsyncExecutionSender<detail::AsyncExecutionFSWrite> AsyncExecutionContext::fsWrite(
    ThreadPool& pool, const FileDescriptor& file, Span<const char> data, uint64_t offset, AsyncThreadPoolMode poolMode)
{
    return {*this, {{pool, poolMode}, file, data, offset}};
}

detail::AsyncExecutionFSPath::AsyncExecutionFSPath(ThreadPool& pool, Operation operation, StringSpan inputPath,
                                                   StringSpan inputDestination, FileSystemCopyFlags flags,
                                                   AsyncThreadPoolMode poolMode)
    : AsyncExecutionFileSystemBase{pool, poolMode}, operation(operation), flags(flags)
{
    const bool copying = operation == Operation::CopyFile or operation == Operation::CopyDirectory;
    if (inputPath.isEmpty())
        initialization =
            Result::Error(AsyncResultCategory, copying ? AsyncError::InvalidSourcePath : AsyncError::InvalidPath);
    else
        initialization = AsyncExecutionResolvePath(inputPath, path);
    if (initialization and (copying or operation == Operation::Rename))
        initialization = inputDestination.isEmpty()
                             ? Result::Error(AsyncResultCategory, AsyncError::InvalidDestinationPath)
                             : AsyncExecutionResolvePath(inputDestination, destination);
}

Result detail::AsyncExecutionFSPath::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    SC_TRY(initialization);
    SC_TRY(prepare(request));
    switch (operation)
    {
    case Operation::CopyFile: return request.copyFile(loop, path.view(), destination.view(), flags);
    case Operation::CopyDirectory: return request.copyDirectory(loop, path.view(), destination.view(), flags);
    case Operation::Rename: return request.rename(loop, path.view(), destination.view());
    case Operation::RemoveFile: return request.removeFile(loop, path.view());
    case Operation::RemoveDirectory: return request.removeEmptyDirectory(loop, path.view());
    default: return Result::Error(AsyncResultCategory, AsyncError::OperationNotSet);
    }
}

AsyncExecutionSender<detail::AsyncExecutionFSPath> AsyncExecutionContext::fsCopyFile(ThreadPool& pool, StringSpan path,
                                                                                     StringSpan          destination,
                                                                                     FileSystemCopyFlags flags,
                                                                                     AsyncThreadPoolMode poolMode)
{
    return {*this, {pool, AsyncFileSystemOperation::Operation::CopyFile, path, destination, flags, poolMode}};
}

AsyncExecutionSender<detail::AsyncExecutionFSPath> AsyncExecutionContext::fsCopyDirectory(
    ThreadPool& pool, StringSpan path, StringSpan destination, FileSystemCopyFlags flags, AsyncThreadPoolMode poolMode)
{
    return {*this, {pool, AsyncFileSystemOperation::Operation::CopyDirectory, path, destination, flags, poolMode}};
}

AsyncExecutionSender<detail::AsyncExecutionFSPath> AsyncExecutionContext::fsRename(ThreadPool& pool, StringSpan path,
                                                                                   StringSpan          destination,
                                                                                   AsyncThreadPoolMode poolMode)
{
    return {*this, {pool, AsyncFileSystemOperation::Operation::Rename, path, destination, {}, poolMode}};
}

AsyncExecutionSender<detail::AsyncExecutionFSPath> AsyncExecutionContext::fsRemoveFile(ThreadPool&         pool,
                                                                                       StringSpan          path,
                                                                                       AsyncThreadPoolMode poolMode)
{
    return {*this, {pool, AsyncFileSystemOperation::Operation::RemoveFile, path, {}, {}, poolMode}};
}

AsyncExecutionSender<detail::AsyncExecutionFSPath> AsyncExecutionContext::fsRemoveEmptyDirectory(
    ThreadPool& pool, StringSpan path, AsyncThreadPoolMode poolMode)
{
    return {*this, {pool, AsyncFileSystemOperation::Operation::RemoveDirectory, path, {}, {}, poolMode}};
}

Result detail::AsyncExecutionFilePoll::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TRY(file.get(handle, Result::Error(AsyncResultCategory, AsyncError::InvalidFileHandle)));
    return request.start(loop, handle);
}

Result detail::AsyncExecutionAccept::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, listener);
}

Result detail::AsyncExecutionAccept::advance(Request::Result& result)
{
    SC_TRY(result.moveTo(client));
    return client.setBlocking(false);
}

detail::AsyncExecutionAccept::Value detail::AsyncExecutionAccept::value(Request::Result&)
{
    return Value(move(client));
}

Result detail::AsyncExecutionConnect::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, socket, address);
}

Result detail::AsyncExecutionSendBuffers::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    size_t first = 0;
    while (first < data.sizeInElements() and data[first].empty())
        ++first;
    if (first == data.sizeInElements())
        return Result::Error(AsyncResultCategory, AsyncError::EmptyBuffer);
    Span<Span<const char>> remaining;
    SC_TRY(data.sliceStart(first, remaining));
    return request.start(loop, socket, remaining);
}

void detail::AsyncExecutionSendAllBuffers::initialize()
{
    index = offset = transferred = 0;
    initialization               = Result(true);
    if (progress)
        *progress = 0;
    size_t total = 0;
    for (const auto& part : data)
    {
        if (part.sizeInBytes() > size_t(-1) - total)
        {
            initialization = Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow);
            return;
        }
        total += part.sizeInBytes();
    }
    findNextBuffer();
}

void detail::AsyncExecutionSendAllBuffers::findNextBuffer()
{
    while (index < data.sizeInElements() and offset == data[index].sizeInBytes())
    {
        ++index;
        offset = 0;
    }
}

Result detail::AsyncExecutionSendAllBuffers::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    SC_TRY(initialization);
    Span<const char> remaining;
    SC_TRY(data[index].sliceStart(offset, remaining));
    Span<const char> chunk;
    SC_TRY(remaining.sliceStartLength(0, remaining.sizeInBytes() > 16384 ? 16384 : remaining.sizeInBytes(), chunk));
    return request.start(loop, socket, chunk);
}

Result detail::AsyncExecutionSendAllBuffers::advance(Request::Result& result)
{
    return advanceBytes(result.completionData.numBytes);
}

Result detail::AsyncExecutionSendAllBuffers::advanceBytes(size_t count)
{
    if (index >= data.sizeInElements())
        return Result::Error(AsyncResultCategory, AsyncError::InvalidState);
    if (count == 0 or count > data[index].sizeInBytes() - offset)
        return Result::Error(AsyncResultCategory, AsyncError::SocketSendIncomplete);
    offset += count;
    transferred += count; // initialize() verified the total size before any I/O.
    if (progress)
        *progress = transferred;
    findNextBuffer();
    return Result(true);
}

void detail::AsyncExecutionReceiveExact::initialize()
{
    received     = 0;
    disconnected = false;
    if (progress)
        *progress = {};
}

Result detail::AsyncExecutionReceiveExact::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    Span<char> remaining;
    SC_TRY(buffer.sliceStart(received, remaining));
    return request.start(loop, socket, remaining);
}

Result detail::AsyncExecutionReceiveExact::advance(Request::Result& result)
{
    Span<char> chunk;
    SC_TRY(result.get(chunk));
    return advanceBytes(chunk.sizeInBytes(), result.isEnded());
}

Result detail::AsyncExecutionReceiveExact::advanceBytes(size_t count, bool ended)
{
    if (count > buffer.sizeInBytes() - received)
        return Result::Error(AsyncResultCategory, AsyncError::InvalidState);
    received += count;
    disconnected = ended;
    if (progress)
        *progress = {{buffer.data(), received}, disconnected};
    if (hasRemaining())
    {
        if (ended)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::SocketReceiveIncomplete);
        if (count == 0)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::SocketReceiveNoProgress);
    }
    return Result(true);
}

void detail::AsyncExecutionReceiveLine::initialize()
{
    received     = 0;
    disconnected = lineComplete = false;
    if (progress)
        *progress = {};
}

Result detail::AsyncExecutionReceiveLine::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
{
    if (buffer.empty())
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::EmptyReceiveBuffer);
    return request.start(loop, socket, {&currentByte, 1});
}

Result detail::AsyncExecutionReceiveLine::advance(Request::Result& result)
{
    Span<char> chunk;
    SC_TRY(result.get(chunk));
    return advanceByte(chunk.sizeInBytes(), result.isEnded());
}

Result detail::AsyncExecutionReceiveLine::advanceByte(size_t count, bool ended)
{
    if (count > 1)
        return Result::Error(AsyncResultCategory, AsyncError::InvalidState);
    disconnected = ended;
    if (ended)
    {
        updateProgress();
        return Result(true);
    }
    if (count == 0)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::SocketReceiveNoProgress);
    if (currentByte == '\n')
        lineComplete = true;
    else if (received == buffer.sizeInBytes())
    {
        updateProgress();
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::ReceiveLineBufferExhausted);
    }
    else
        buffer[received++] = currentByte;
    updateProgress();
    return Result(true);
}

detail::AsyncExecutionReceiveLine::Value detail::AsyncExecutionReceiveLine::currentValue()
{
    size_t count = received;
    if (lineComplete and count > 0 and buffer[count - 1] == '\r')
        --count;
    return {{buffer.data(), count}, disconnected, lineComplete};
}

detail::AsyncExecutionReceiveLine::Value detail::AsyncExecutionReceiveLine::value(Request::Result&)
{
    return currentValue();
}

void detail::AsyncExecutionReceiveLine::updateProgress()
{
    if (progress)
        *progress = currentValue();
}

Result detail::AsyncExecutionSend::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, socket, data);
}

Result detail::AsyncExecutionSendAll::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    // Bound each individual send; completion advances over short writes and chunk boundaries alike.
    Span<const char> chunk;
    SC_TRY(remaining.sliceStartLength(0, remaining.sizeInBytes() > 16384 ? 16384 : remaining.sizeInBytes(), chunk));
    return request.start(loop, socket, chunk);
}

Result detail::AsyncExecutionReceive::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, socket, buffer);
}

void detail::AsyncExecutionProcessExit::initialize()
{
    if (progress)
        *progress = {};
}

void detail::AsyncExecutionProcessExit::observe(Request::Result& result)
{
    if (progress and result.isValid())
        *progress = value(result);
}

Result detail::AsyncExecutionProcessExit::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, process);
}

detail::AsyncExecutionProcessExit::Value detail::AsyncExecutionProcessExit::value(Request::Result& result)
{
    return {result.completionData.exitStatus, true};
}

Result detail::AsyncExecutionSignal::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, signalNumber, options);
}

detail::AsyncExecutionSignal::Value detail::AsyncExecutionSignal::value(Request::Result& result)
{
    return {result.completionData.signalNumber, result.completionData.deliveryCount};
}

Result detail::AsyncExecutionSendTo::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    SC_TRY(AsyncExecutionValidateSingleTransfer(data.sizeInBytes()));
    return request.start(loop, socket, address, data);
}

Result detail::AsyncExecutionSendToBuffers::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    size_t total = 0;
    for (const auto part : data)
    {
        SC_TRY(AsyncExecutionValidateSingleTransfer(part.sizeInBytes()));
        if (part.sizeInBytes() > size_t(uint32_t(-1)) - total)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow);
        total += part.sizeInBytes();
    }
    if (data.empty())
        return request.start(loop, socket, address, Span<const char>{});
    return request.start(loop, socket, address, data);
}

Result detail::AsyncExecutionReceiveFrom::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    SC_TRY(AsyncExecutionValidateSingleTransfer(buffer.sizeInBytes()));
    return request.start(loop, socket, buffer);
}

detail::AsyncExecutionReceiveFrom::Value detail::AsyncExecutionReceiveFrom::value(Request::Result& result)
{
    Span<char> data;
    (void)result.get(data);
    return {data, result.getSourceAddress(), result.getSourceSocketAddress(), result.isTruncated()};
}

Result detail::AsyncExecutionFileRead::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
{
    SC_TRY(AsyncExecutionValidateSingleTransfer(buffer.sizeInBytes()));
    SC_TRY(AsyncExecutionValidateFileRange(options, buffer.sizeInBytes()));
    SC_TRY(AsyncExecutionPrepareFile(request, task, options));
    return request.start(loop, file, buffer);
}

Result detail::AsyncExecutionFileWrite::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
{
    SC_TRY(AsyncExecutionValidateSingleTransfer(data.sizeInBytes()));
    SC_TRY(AsyncExecutionValidateFileRange(options, data.sizeInBytes()));
    SC_TRY(AsyncExecutionPrepareFile(request, task, options));
    return request.start(loop, file, data);
}

Result detail::AsyncExecutionFileWriteBuffers::submit(Request& request, AsyncEventLoop& loop,
                                                      AsyncTaskSequence& task) const
{
    size_t first = 0, total = 0;
    for (const auto& part : data)
    {
        if (part.sizeInBytes() > size_t(-1) - total)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow);
        total += part.sizeInBytes();
    }
    while (first < data.sizeInElements() and data[first].empty())
        ++first;
    if (first == data.sizeInElements())
        return Result::Error(AsyncResultCategory, AsyncError::EmptyBuffer);
    SC_TRY(AsyncExecutionValidateSingleTransfer(total));
    SC_TRY(AsyncExecutionValidateFileRange(options, total));
    SC_TRY(AsyncExecutionPrepareFile(request, task, options));
    Span<Span<const char>> remaining;
    SC_TRY(data.sliceStart(first, remaining));
    return request.start(loop, file, remaining);
}

void detail::AsyncExecutionFileReadMany::initialize()
{
    received  = 0;
    endOfFile = false;
    if (progress)
        *progress = {};
    initialization = buffer.empty() ? Result(true) : AsyncExecutionValidateFileRange(options, buffer.sizeInBytes());
}

Result detail::AsyncExecutionFileReadMany::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
{
    SC_TRY(initialization);
    Span<char> remaining;
    SC_TRY(buffer.sliceStart(received, remaining));
    Span<char> chunk;
    SC_TRY(remaining.sliceStartLength(
        0, remaining.sizeInBytes() > AsyncExecutionFileChunk ? AsyncExecutionFileChunk : remaining.sizeInBytes(),
        chunk));
    SC_TRY(AsyncExecutionPrepareFile(request, task, options, received));
    return request.start(loop, file, chunk);
}

Result detail::AsyncExecutionFileReadMany::advance(Request::Result& result)
{
    Span<char> chunk;
    SC_TRY(result.get(chunk));
    return advanceBytes(chunk.sizeInBytes(), result.isEnded());
}

Result detail::AsyncExecutionFileReadMany::advanceBytes(size_t count, bool ended)
{
    if (count > buffer.sizeInBytes() - received)
        return Result::Error(AsyncResultCategory, AsyncError::InvalidState);
    received += count;
    endOfFile = ended;
    if (progress)
        *progress = {{buffer.data(), received}, endOfFile};
    if (received < buffer.sizeInBytes())
    {
        if (ended and requireFull)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::UnexpectedEndOfFile);
        if (not ended and count == 0)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileReadNoProgress);
    }
    return Result(true);
}

void detail::AsyncExecutionFileWriteAll::initialize()
{
    index = offset = transferred = 0;
    if (progress)
        *progress = 0;
    size_t total   = singleBuffer ? data.sizeInBytes() : 0;
    initialization = Result(true);
    if (not singleBuffer)
    {
        for (const auto& part : buffers)
        {
            if (part.sizeInBytes() > size_t(-1) - total)
            {
                initialization = Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow);
                return;
            }
            total += part.sizeInBytes();
        }
        findNextBuffer();
    }
    if (total != 0)
        initialization = AsyncExecutionValidateFileRange(options, total);
}

void detail::AsyncExecutionFileWriteAll::findNextBuffer()
{
    while (index < buffers.sizeInElements() and offset == buffers[index].sizeInBytes())
    {
        ++index;
        offset = 0;
    }
}

Result detail::AsyncExecutionFileWriteAll::remaining(Span<const char>& output) const
{
    if (not hasRemaining())
        return Result::Error(AsyncResultCategory, AsyncError::InvalidState);
    return Result((singleBuffer ? data : buffers[index]).sliceStart(offset, output));
}

Result detail::AsyncExecutionFileWriteAll::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
{
    SC_TRY(initialization);
    Span<const char> rest, chunk;
    SC_TRY(remaining(rest));
    SC_TRY(rest.sliceStartLength(
        0, rest.sizeInBytes() > AsyncExecutionFileChunk ? AsyncExecutionFileChunk : rest.sizeInBytes(), chunk));
    SC_TRY(AsyncExecutionPrepareFile(request, task, options, transferred));
    return request.start(loop, file, chunk);
}

Result detail::AsyncExecutionFileWriteAll::advance(Request::Result& result)
{
    return advanceBytes(result.completionData.numBytes);
}

Result detail::AsyncExecutionFileWriteAll::advanceBytes(size_t count)
{
    Span<const char> rest;
    SC_TRY(remaining(rest));
    if (count == 0 or count > rest.sizeInBytes())
        return Result::Error(AsyncResultCategory, AsyncError::FileWriteIncomplete);
    offset += count;
    transferred += count;
    if (progress)
        *progress = transferred;
    if (not singleBuffer)
        findNextBuffer();
    return Result(true);
}

void detail::AsyncExecutionFileSend::initialize()
{
    if (progress)
        *progress = {};
}

Result detail::AsyncExecutionFileSend::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
{
    if (options.offset < 0 or uint64_t(options.length) > AsyncExecutionMaximumFileOffset - uint64_t(options.offset))
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::InvalidFileOffset);
    // TransmitFile's maximum includes any framing; this adapter sends only the selected file range.
    if (uint64_t(options.length) > 2147483646ULL)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow);
    if (options.threadPoolMode == AsyncThreadPoolMode::ForceThreadPool)
    {
        if (options.threadPool == nullptr)
            return Result::Error(AsyncResultCategory, AsyncError::MissingThreadPool);
        SC_TRY(request.executeOn(task, *options.threadPool, options.threadPoolMode));
    }
    // Every supported backend has native file-send support, including Windows TransmitFile.
    return request.start(loop, file, socket, options.offset, options.length, options.pipeSize);
}

void detail::AsyncExecutionFileSend::observe(Request::Result& result)
{
    if (progress)
        *progress = value(result);
}

detail::AsyncExecutionFileSend::Value detail::AsyncExecutionFileSend::value(Request::Result& result)
{
    return {result.getBytesTransferred(), result.usedZeroCopy(), result.isComplete()};
}

Result detail::AsyncExecutionWork::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    request.work = work;
    SC_TRY(request.setThreadPool(pool));
    return request.start(loop);
}

Result detail::AsyncExecutionReadFile::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
{
    SC_TRY(initialization);
    SC_TRY(request.setThreadPool(pool));
    request.work = [this]() -> Result
    {
        FileDescriptor file;
        FileOpen       options;
        options.mode = FileOpen::Read;
        SC_TRY(file.open(path.view(), options));
        used                = 0;
        const Result status = [&]() -> Result
        {
            while (true)
            {
                char         probe;
                Span<char>   read;
                const bool   full      = used == destination.sizeInBytes();
                const size_t available = destination.sizeInBytes() - used;
                const size_t count     = available > 32768 ? 32768 : available;
                SC_TRY(file.read(full ? Span<char>{&probe, 1} : Span<char>{destination.data() + used, count}, read));
                if (read.empty())
                    return Result(true);
                if (full)
                    return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
                used += read.sizeInBytes();
            }
        }();
        const Result closed = file.close();
        return status ? closed : status;
    };
    return request.start(loop);
}

Result detail::AsyncExecutionWriteFile::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
{
    SC_TRY(initialization);
    SC_TRY(request.setThreadPool(pool));
    request.work = [this]() -> Result
    {
        FileDescriptor file;
        FileOpen       options;
        options.mode = FileOpen::Write;
        SC_TRY(file.open(path.view(), options));
        const Result status = [&]() -> Result
        {
            size_t used = 0;
            while (used < data.sizeInBytes())
            {
                const size_t     available = data.sizeInBytes() - used;
                const size_t     count     = available > 32768 ? 32768 : available;
                const ResultFile written   = file.write({data.data() + used, count});
                if (written)
                    used += count;
                else if (written.isError(FileError::IncompleteWrite) and
                         written.contextKind == FileErrorContextKind::ActualBytes and
                         written.context.actualBytes > 0 and written.context.actualBytes <= count)
                    used += written.context.actualBytes;
                else
                    return written;
            }
            return Result(true);
        }();
        const Result closed = file.close();
        return status ? closed : status;
    };
    return request.start(loop);
}

Result detail::AsyncExecutionOwnedConnect::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
{
    SC_TRY(initialization);
    SC_TRY(socket.create(address.getAddressFamily()));
    SC_TRY(socket.setBlocking(false));
    SC_TRY(loop.associateExternallyCreatedSocket(socket));
    return request.start(loop, socket, address);
}

Result detail::AsyncExecutionSendAll::advance(Request::Result& result)
{
    return advanceBytes(result.completionData.numBytes);
}

Result detail::AsyncExecutionSendAll::advanceBytes(size_t count)
{
    if (count == 0 or count > remaining.sizeInBytes())
        return Result::Error(AsyncResultCategory, AsyncError::SocketSendIncomplete);
    if (count > size_t(-1) - transferred)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow);
    transferred += count;
    if (progress)
        *progress = transferred;
    return Result(remaining.sliceStartLength(count, remaining.sizeInBytes() - count, remaining));
}

detail::AsyncExecutionReceive::Value detail::AsyncExecutionReceive::value(Request::Result& result)
{
    Span<char> data;
    (void)result.get(data);
    return {data, result.isEnded()};
}

detail::AsyncExecutionFileRead::Value detail::AsyncExecutionFileRead::value(Request::Result& result)
{
    Span<char> data;
    (void)result.get(data);
    return {data, result.isEnded()};
}

AsyncExecutionConnection& AsyncExecutionConnection::operator=(AsyncExecutionConnection&& other) noexcept
{
    if (this != &other)
    {
        // Retire the previous descriptor with normal RAII close semantics; transfer into an empty destination.
        SocketDescriptor previous(move(socket));
        socket = move(other.socket);
    }
    return *this;
}

Result detail::AsyncExecutionResolvePath(StringSpan path, StringPath& output)
{
    StringPath directory;
    const bool copied = Path::isAbsolute(path, Path::AsNative)
                            ? output.assign(path)
                            : Path::join(output, {FileSystem::Operations::getCurrentWorkingDirectory(directory), path});
    return copied ? Result(true) : Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::InvalidPath);
}

AsyncExecution::AsyncExecution() : context(loop) {}

AsyncExecution::~AsyncExecution()
{
    // All graphs/producers must already be quiescent. A completed graph can still have a copied pool notification
    // returning to ThreadPool; join that tail before destroying the borrowed context and pool.
    if (poolCreated)
        SC_ASYNC_TRUST_RESULT(pool.waitForAllTasks());
    SC_ASYNC_TRUST_RESULT(close());
}

Result AsyncExecution::create(size_t workers)
{
    if (loopCreated or poolCreated or contextCreated)
        return Result::Error(AsyncResultCategory, AsyncError::AlreadyInitialized);
    ownerThread = Thread::CurrentThreadID();
    SocketNetworking::initNetworking();
    SC_TRY(loop.create());
    loopCreated   = true;
    Result status = pool.create(workers);
    if (status)
    {
        poolCreated = true;
        status      = context.create();
        if (status)
        {
            contextCreated = true;
            return Result(true);
        }
    }
    const Result cleanup = close();
    return cleanup ? status : cleanup;
}

Result AsyncExecution::close()
{
    if (not loopCreated and not poolCreated and not contextCreated)
        return Result(true);
    if (Thread::CurrentThreadID() != ownerThread)
        return Result::Error(AsyncResultCategory, AsyncError::InvalidState);
    if (contextCreated)
    {
        SC_TRY(context.close());
        contextCreated = false;
    }
    if (loopCreated)
    {
        SC_TRY(loop.close());
        loopCreated = false;
    }
    if (poolCreated)
    {
        SC_TRY(pool.destroy());
        poolCreated = false;
    }
    return Result(true);
}

void AsyncExecutionContext::RunState::finishOnLoop(Result result) noexcept
{
    status    = result;
    completed = true;
    if (driving)
        context.loop.interrupt();
}

void AsyncExecutionContext::RunState::finish(Result result) noexcept
{
    if (Thread::CurrentThreadID() == context.ownerThread)
        finishOnLoop(result);
    else
    {
        pending              = result;
        completion.operation = this;
        completion.execute   = [](detail::AsyncExecutionCommand& command) noexcept
        {
            auto& state = *static_cast<RunState*>(command.operation);
            state.finishOnLoop(state.pending);
        };
        SC_ASYNC_TRUST_RESULT(context.post(completion));
    }
}

AsyncExecutionSender<detail::AsyncExecutionAccept> AsyncExecutionContext::accept(const SocketDescriptor& listener)
{
    return {*this, {listener, {}}};
}

AsyncExecutionSender<detail::AsyncExecutionConnect> AsyncExecutionContext::connect(const SocketDescriptor& socket,
                                                                                   SocketIPAddress         address)
{
    return connect(socket, SocketAddress(address));
}

AsyncExecutionSender<detail::AsyncExecutionConnect> AsyncExecutionContext::connect(const SocketDescriptor& socket,
                                                                                   SocketAddress           address)
{
    return {*this, {socket, address}};
}

AsyncExecutionSender<detail::AsyncExecutionSendBuffers> AsyncExecutionContext::send(const SocketDescriptor& socket,
                                                                                    Span<Span<const char>>  data)
{
    return {*this, {socket, data}};
}

AsyncExecutionSender<detail::AsyncExecutionSendAllBuffers> AsyncExecutionContext::sendAll(
    const SocketDescriptor& socket, Span<Span<const char>> data, size_t* progress)
{
    return {*this, {socket, data, progress}};
}

AsyncExecutionSender<detail::AsyncExecutionReceiveExact> AsyncExecutionContext::receiveExact(
    const SocketDescriptor& socket, Span<char> buffer, AsyncExecutionReadResult* progress)
{
    return {*this, {socket, buffer, progress}};
}

AsyncExecutionSender<detail::AsyncExecutionReceiveLine> AsyncExecutionContext::receiveLine(
    const SocketDescriptor& socket, Span<char> buffer, AsyncExecutionLineResult* progress)
{
    return {*this, {socket, buffer, progress}};
}

AsyncExecutionSender<detail::AsyncExecutionTimer> AsyncExecutionContext::sleep(TimeMs duration)
{
    return AsyncExecutionSender<detail::AsyncExecutionTimer>{*this, {duration}};
}

AsyncExecutionSender<detail::AsyncExecutionFilePoll> AsyncExecutionContext::filePoll(const FileDescriptor& file)
{
    return {*this, {file}};
}

AsyncExecutionSender<detail::AsyncExecutionSend> AsyncExecutionContext::send(const SocketDescriptor& socket,
                                                                             Span<const char>        data)
{
    return AsyncExecutionSender<detail::AsyncExecutionSend>{*this, {socket, data}};
}

AsyncExecutionSender<detail::AsyncExecutionSendAll> AsyncExecutionContext::sendAll(const SocketDescriptor& socket,
                                                                                   Span<const char>        data,
                                                                                   size_t*                 progress)
{
    return AsyncExecutionSender<detail::AsyncExecutionSendAll>{*this, {socket, data, 0, progress}};
}

AsyncExecutionSender<detail::AsyncExecutionReceive> AsyncExecutionContext::receive(const SocketDescriptor& socket,
                                                                                   Span<char>              buffer)
{
    return AsyncExecutionSender<detail::AsyncExecutionReceive>{*this, {socket, buffer}};
}

AsyncExecutionSender<detail::AsyncExecutionProcessExit> AsyncExecutionContext::processExit(
    FileDescriptor::Handle process, AsyncExecutionProcessExitResult* progress)
{
    return {*this, {process, progress}};
}

AsyncExecutionSender<detail::AsyncExecutionSignal> AsyncExecutionContext::signal(int                signalNumber,
                                                                                 AsyncSignalOptions options)
{
    return {*this, {signalNumber, options}};
}

AsyncExecutionSender<detail::AsyncExecutionSendTo> AsyncExecutionContext::sendTo(const SocketDescriptor& socket,
                                                                                 SocketIPAddress         address,
                                                                                 Span<const char>        data)
{
    return sendTo(socket, SocketAddress(address), data);
}

AsyncExecutionSender<detail::AsyncExecutionSendTo> AsyncExecutionContext::sendTo(const SocketDescriptor& socket,
                                                                                 SocketAddress           address,
                                                                                 Span<const char>        data)
{
    return {*this, {socket, address, data}};
}

AsyncExecutionSender<detail::AsyncExecutionSendToBuffers> AsyncExecutionContext::sendTo(const SocketDescriptor& socket,
                                                                                        SocketIPAddress         address,
                                                                                        Span<Span<const char>>  data)
{
    return sendTo(socket, SocketAddress(address), data);
}

AsyncExecutionSender<detail::AsyncExecutionSendToBuffers> AsyncExecutionContext::sendTo(const SocketDescriptor& socket,
                                                                                        SocketAddress           address,
                                                                                        Span<Span<const char>>  data)
{
    return {*this, {socket, address, data}};
}

AsyncExecutionSender<detail::AsyncExecutionReceiveFrom> AsyncExecutionContext::receiveFrom(
    const SocketDescriptor& socket, Span<char> buffer)
{
    return {*this, {socket, buffer}};
}

AsyncExecutionSender<detail::AsyncExecutionFileSend> AsyncExecutionContext::fileSend(
    const FileDescriptor& file, const SocketDescriptor& socket, AsyncExecutionFileSendOptions options,
    AsyncExecutionFileSendResult* progress)
{
    return {*this, {file, socket, options, progress}};
}

AsyncExecutionSender<detail::AsyncExecutionFileRead> AsyncExecutionContext::fileRead(
    const FileDescriptor& file, Span<char> buffer, AsyncExecutionFileReadOptions options)
{
    return {*this, {file, buffer, options}};
}

AsyncExecutionSender<detail::AsyncExecutionFileRead> AsyncExecutionContext::fileReadAt(
    const FileDescriptor& file, uint64_t offset, Span<char> buffer, AsyncExecutionFileReadOptions options)
{
    options.useOffset = true;
    options.offset    = offset;
    return fileRead(file, buffer, options);
}

AsyncExecutionSender<detail::AsyncExecutionFileReadMany> AsyncExecutionContext::fileReadExact(
    const FileDescriptor& file, Span<char> buffer, AsyncExecutionFileReadOptions options,
    AsyncExecutionReadResult* progress)
{
    return {*this, {file, buffer, options, progress, true}};
}

AsyncExecutionSender<detail::AsyncExecutionFileReadMany> AsyncExecutionContext::fileReadExactAt(
    const FileDescriptor& file, uint64_t offset, Span<char> buffer, AsyncExecutionFileReadOptions options,
    AsyncExecutionReadResult* progress)
{
    options.useOffset = true;
    options.offset    = offset;
    return fileReadExact(file, buffer, options, progress);
}

AsyncExecutionSender<detail::AsyncExecutionFileReadMany> AsyncExecutionContext::fileReadUntilFullOrEOF(
    const FileDescriptor& file, Span<char> buffer, AsyncExecutionFileReadOptions options,
    AsyncExecutionReadResult* progress)
{
    return {*this, {file, buffer, options, progress, false}};
}

AsyncExecutionSender<detail::AsyncExecutionFileWrite> AsyncExecutionContext::fileWrite(
    const FileDescriptor& file, Span<const char> data, AsyncExecutionFileWriteOptions options)
{
    return {*this, {file, data, options}};
}

AsyncExecutionSender<detail::AsyncExecutionFileWriteBuffers> AsyncExecutionContext::fileWrite(
    const FileDescriptor& file, Span<Span<const char>> data, AsyncExecutionFileWriteOptions options)
{
    return {*this, {file, data, options}};
}

AsyncExecutionSender<detail::AsyncExecutionFileWrite> AsyncExecutionContext::fileWriteAt(
    const FileDescriptor& file, uint64_t offset, Span<const char> data, AsyncExecutionFileWriteOptions options)
{
    options.useOffset = true;
    options.offset    = offset;
    return fileWrite(file, data, options);
}

AsyncExecutionSender<detail::AsyncExecutionFileWriteBuffers> AsyncExecutionContext::fileWriteAt(
    const FileDescriptor& file, uint64_t offset, Span<Span<const char>> data, AsyncExecutionFileWriteOptions options)
{
    options.useOffset = true;
    options.offset    = offset;
    return fileWrite(file, data, options);
}

AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> AsyncExecutionContext::fileWriteAll(
    const FileDescriptor& file, Span<const char> data, AsyncExecutionFileWriteOptions options, size_t* progress)
{
    return {*this, {file, data, {}, options, progress, true}};
}

AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> AsyncExecutionContext::fileWriteAll(
    const FileDescriptor& file, Span<Span<const char>> data, AsyncExecutionFileWriteOptions options, size_t* progress)
{
    return {*this, {file, {}, data, options, progress, false}};
}

AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> AsyncExecutionContext::fileWriteAllAt(
    const FileDescriptor& file, uint64_t offset, Span<const char> data, AsyncExecutionFileWriteOptions options,
    size_t* progress)
{
    options.useOffset = true;
    options.offset    = offset;
    return fileWriteAll(file, data, options, progress);
}

AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> AsyncExecutionContext::fileWriteAllAt(
    const FileDescriptor& file, uint64_t offset, Span<Span<const char>> data, AsyncExecutionFileWriteOptions options,
    size_t* progress)
{
    options.useOffset = true;
    options.offset    = offset;
    return fileWriteAll(file, data, options, progress);
}

AsyncExecutionSender<detail::AsyncExecutionFileRead> AsyncExecutionContext::fileRead(const FileDescriptor& file,
                                                                                     Span<char>            buffer,
                                                                                     ThreadPool& pool, uint64_t offset)
{
    return fileRead(file, buffer, AsyncExecutionFileReadOptions{&pool, offset, true});
}

AsyncExecutionSender<detail::AsyncExecutionFileWrite> AsyncExecutionContext::fileWrite(const FileDescriptor& file,
                                                                                       Span<const char>      data,
                                                                                       ThreadPool&           pool,
                                                                                       uint64_t              offset)
{
    return fileWrite(file, data, AsyncExecutionFileWriteOptions{&pool, offset, true});
}

AsyncExecutionSender<detail::AsyncExecutionWork> AsyncExecutionContext::work(ThreadPool&        pool,
                                                                             Function<Result()> procedure)
{
    return AsyncExecutionSender<detail::AsyncExecutionWork>{*this, {pool, move(procedure)}};
}

AsyncExecutionScheduler                           AsyncExecutionContext::getScheduler() noexcept { return {this}; }
AsyncExecutionSender<detail::AsyncExecutionTimer> AsyncExecutionScheduler::schedule() const noexcept
{
    return AsyncExecutionSender<detail::AsyncExecutionTimer>{*context, {{0}}};
}

AsyncExecutionSender<detail::AsyncExecutionReadFile> AsyncExecution::readFile(StringSpan path, Span<char> destination)
{
    detail::AsyncExecutionReadFile setup{pool, {}, Result(true), destination};
    setup.initialization = detail::AsyncExecutionResolvePath(path, setup.path);
    return AsyncExecutionSender<detail::AsyncExecutionReadFile>{context, move(setup)};
}

AsyncExecutionSender<detail::AsyncExecutionWriteFile> AsyncExecution::writeFile(StringSpan path, Span<const char> data)
{
    detail::AsyncExecutionWriteFile setup{pool, {}, Result(true), move(data)};
    setup.initialization = detail::AsyncExecutionResolvePath(path, setup.path);
    return AsyncExecutionSender<detail::AsyncExecutionWriteFile>{context, move(setup)};
}

AsyncExecutionSender<detail::AsyncExecutionOwnedConnect> AsyncExecution::connectTCP(StringSpan host, uint16_t port)
{
    detail::AsyncExecutionOwnedConnect setup;
    setup.initialization = setup.address.fromAddressPort(host, port);
    return AsyncExecutionSender<detail::AsyncExecutionOwnedConnect>{context, move(setup)};
}

AsyncExecutionSender<detail::AsyncExecutionTimer> AsyncExecution::sleep(TimeMs duration)
{
    return context.sleep(duration);
}

AsyncExecutionSender<detail::AsyncExecutionSendAll> detail::AsyncExecutionSendConnected::operator()(
    AsyncExecutionConnection& connection) noexcept
{
    return {*context, {connection.socket, move(data)}};
}

detail::AsyncExecutionTCPSender AsyncExecution::sendTCP(StringSpan host, uint16_t port, Span<const char> data)
{
    detail::AsyncExecutionOwnedConnect setup;
    setup.initialization = setup.address.fromAddressPort(host, port);
    return AsyncExecutionProtocol::let_value(
        AsyncExecutionSender<detail::AsyncExecutionOwnedConnect>{context, move(setup)},
        detail::AsyncExecutionSendConnected{&context, move(data)});
}

Result AsyncExecutionContext::beginRun()
{
    if (not accepting.load())
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    if (Thread::CurrentThreadID() != ownerThread)
        return Result::Error(AsyncResultCategory, AsyncError::InvalidState);
    if (running or dispatching or delivering.load() != 0)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::AlreadyRunning);
    running = true;

    return Result(true);
}

Result AsyncExecutionContext::finishRun(RunState& state)
{
    // SC sender completions occur on the owner thread. Synchronous external senders can finish in start().
    while (not state.completed)
    {
        state.driving = true;
        // A failed dispatcher cannot safely release arbitrary connected operation storage.
        SC_ASYNC_TRUST_RESULT(loop.run());
        state.driving = false;
    }
    running = false;
    SC_ASYNC_ASSERT_RELEASE(state.completed);
    return state.status;
}

#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
namespace detail
{
static void* AsyncExecutionScopeAllocate(void*, size_t bytes, size_t alignment) noexcept
{
    if (alignment < alignof(void*))
        alignment = alignof(void*);
    const size_t overhead = sizeof(void*) + alignment - 1;
    if (bytes > size_t(-1) - overhead)
        return nullptr;
    void* raw = ::malloc(bytes + overhead);
    if (raw == nullptr)
        return nullptr;
    const size_t address                  = (reinterpret_cast<size_t>(raw) + overhead) & ~(alignment - 1);
    auto*        aligned                  = reinterpret_cast<void*>(address);
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return aligned;
}
static void AsyncExecutionScopeDeallocate(void*, void* memory, size_t, size_t) noexcept
{
    ::free(reinterpret_cast<void**>(memory)[-1]);
}
} // namespace detail

AsyncExecutionScope::AsyncExecutionScope(AsyncExecutionContext& context, AsyncExecutionScopeAllocator allocator)
    : context(context), allocator(allocator)
{
    if (not this->allocator.allocate and not this->allocator.deallocate)
    {
        this->allocator.allocate   = detail::AsyncExecutionScopeAllocate;
        this->allocator.deallocate = detail::AsyncExecutionScopeDeallocate;
    }
}

AsyncExecutionScope::~AsyncExecutionScope()
{
    SC_ASYNC_ASSERT_RELEASE(not joining and not contextBound and (not used or joined));
}

Result AsyncExecutionScope::checkSpawn()
{
    mutex.lock();
    const bool closed = sealed;
    mutex.unlock();
    if (closed)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::ScopeClosed);
    if (not allocator.allocate or not allocator.deallocate)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::InvalidAllocator);
    if (not context.accepting.load())
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    return Result(true);
}

Result AsyncExecutionScope::beginChild(detail::AsyncExecutionScopeAssociation& association)
{
    mutex.lock();
    if (sealed)
    {
        mutex.unlock();
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::ScopeClosed);
    }
    if (not context.accepting.load())
    {
        mutex.unlock();
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    }
    association = provider.get_token().try_associate();
    if (not association)
    {
        mutex.unlock();
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::ScopeClosed);
    }
    if (not used)
    {
        contextBound = true;
        context.outstanding.fetch_add(1); // Keep the used scope's context open until an explicit join.
    }
    used   = true;
    joined = false;
    context.outstanding.fetch_add(1);
    mutex.unlock();
    return Result(true);
}

void AsyncExecutionScope::acceptedChild()
{
    mutex.lock();
    ++summary.accepted;
    mutex.unlock();
}

void AsyncExecutionScope::abandonChild() { context.outstanding.fetch_sub(1); }

void AsyncExecutionScope::completedChild(Result status, bool stopped)
{
    mutex.lock();
    if (not status)
    {
        ++summary.failed;
        if (summary.firstError)
            summary.firstError = status;
    }
    else if (stopped)
        ++summary.stopped;
    else
        ++summary.succeeded;
    mutex.unlock();
}

void AsyncExecutionScope::requestStop() noexcept
{
    mutex.lock();
    sealed = true;
    mutex.unlock();
    // Seal only the SC root interface. Provider associations remain open for already-accepted child graphs' tails.
    provider.request_stop();
}

AsyncExecutionScopeJoinSender AsyncExecutionScope::join() noexcept { return {this}; }

AsyncExecutionScopeSummary AsyncExecutionScope::getSummary()
{
    mutex.lock();
    const auto result = summary;
    mutex.unlock();
    return result;
}

Result AsyncExecutionScope::beginJoin()
{
    mutex.lock();
    if (joining)
    {
        mutex.unlock();
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::AlreadyRunning);
    }
    if (not context.accepting.load())
    {
        mutex.unlock();
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    }
    sealed  = true;
    joining = true;
    context.outstanding.fetch_add(1);
    mutex.unlock();
    return Result(true);
}

AsyncExecutionScopeSummary AsyncExecutionScope::finishJoin()
{
    mutex.lock();
    joining                  = false;
    joined                   = true;
    const auto result        = summary;
    const int  registrations = contextBound ? 2 : 1;
    contextBound             = false;
    mutex.unlock();
    context.outstanding.fetch_sub(registrations);
    return result;
}

detail::AsyncExecutionScopeEnvironment detail::AsyncExecutionScopeNode::getEnvironment() const noexcept
{
    return {scope.context.getScheduler(), scope.provider.get_token()};
}

void detail::AsyncExecutionScopeNode::complete(Result status, bool stopped) noexcept
{
    auto       lifetime = move(this->association);
    auto*      owner    = &scope;
    const auto release  = destroy;
    owner->completedChild(status, stopped);
    release(this);
    owner->abandonChild();
    // Releasing the association can synchronously finish join and destroy the scope. No owner/node access follows.
}

#endif // SC_ASYNC_EXECUTION_SCOPES_AVAILABLE

void detail::AsyncExecutionTimeoutState::workCompleted(Result status, bool stopped) noexcept
{
    mutex.lock();
    workError   = status;
    workStopped = stopped;
    if (winner == Winner::Pending)
        winner = Winner::Work;
    mutex.unlock();
    timerStop.request_stop();
}

void detail::AsyncExecutionTimeoutState::timerCompleted(Result status, bool stopped) noexcept
{
    mutex.lock();
    timerError = status;
    if (not stopped and winner == Winner::Pending)
        winner = Winner::Timer;
    mutex.unlock();
    workStop.request_stop();
}

bool detail::AsyncExecutionTimeoutState::beginCallerStop() noexcept
{
    mutex.lock();
    const bool accepted = not delivering;
    if (accepted)
    {
        callerStopped  = true;
        stoppingCaller = true;
    }
    mutex.unlock();
    return accepted;
}

bool detail::AsyncExecutionTimeoutState::endCallerStop() noexcept
{
    mutex.lock();
    stoppingCaller   = false;
    const bool ready = bothJoined;
    delivering       = ready;
    mutex.unlock();
    return ready;
}

bool detail::AsyncExecutionTimeoutState::joined() noexcept
{
    mutex.lock();
    bothJoined       = true;
    const bool ready = not stoppingCaller;
    delivering       = ready;
    mutex.unlock();
    return ready;
}

Result detail::AsyncExecutionTimeoutState::outcome(bool& stopped) noexcept
{
    mutex.lock();
    Result status = workError;
    if (status)
        status = timerError;
    if (status)
    {
        if (callerStopped)
            stopped = true;
        else if (winner == Winner::Timer)
            status = Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut);
        else
            stopped = workStopped;
    }
    mutex.unlock();
    return status;
}

} // namespace SC
#endif
