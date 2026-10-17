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
static void* AsyncExecutionReallocate(void* memory, size_t bytes) noexcept { return ::realloc(memory, bytes); }
static void  AsyncExecutionRelease(void* memory) noexcept { ::free(memory); }
} // namespace detail

AsyncExecutionContext::AsyncExecutionContext(AsyncEventLoop& loop) : loop(loop) {}

AsyncExecutionContext::~AsyncExecutionContext() { SC_ASYNC_ASSERT_DEBUG(not accepting.load()); }

Result AsyncExecutionContext::create()
{
    if (accepting.load())
        return Result::Error(AsyncResultCategory, AsyncError::AlreadyInitialized);
    ownerThread     = Thread::CurrentThreadID();
    wakeUp.callback = [this](AsyncLoopWakeUp::Result& result)
    {
        result.reactivateRequest(true);
        dispatching = true;
        while (auto* command = pop())
            command->execute(*command); // Commands may destroy themselves. Do not access them afterward.
        dispatching = false;
    };
    SC_TRY(wakeUp.start(loop));
    accepting.store(true);
    return Result(true);
}

Result AsyncExecutionContext::close()
{
    if (not accepting.load())
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    if (Thread::CurrentThreadID() != ownerThread or dispatching or delivering.load() != 0 or outstanding.load() != 0)
        return Result::Error(AsyncResultCategory, AsyncError::ActiveRequestsRemain);
    // Callers must stop submitting and join producer threads before closing the context.
    accepting.store(false);
    SC_TRY(wakeUp.stop(loop));
    SC_TRY(loop.runNoWait());
    SC_ASYNC_ASSERT_DEBUG(wakeUp.isFree());
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
    const Result status = wakeUp.wakeUp(loop);
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

Result detail::AsyncExecutionPoolTicket::submit(AsyncExecutionContext& context, ThreadPool& pool, void* operation,
                                                Notify notify) noexcept
{
    struct Ticket
    {
        ThreadPoolTask         task;
        AsyncExecutionCommand  cleanup;
        AsyncExecutionContext* context;
        ThreadPool*            pool;
        void*                  operation;
        Notify                 notify;
    };

    auto* ticket = static_cast<Ticket*>(AsyncExecutionReallocate(nullptr, sizeof(Ticket)));
    if (not ticket)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory);
    placementNew(*ticket);
    ticket->operation         = operation;
    ticket->notify            = notify;
    ticket->context           = &context;
    ticket->pool              = &pool;
    ticket->cleanup.operation = ticket;
    ticket->cleanup.execute   = [](AsyncExecutionCommand& command) noexcept
    {
        auto* ticket = static_cast<Ticket*>(command.operation);
        // ThreadPool accesses its Task after the callback returns. Reclaim only after that access finishes.
        SC_ASYNC_TRUST_RESULT(ticket->pool->waitForTask(ticket->task));
        ticket->context->outstanding.fetch_sub(1);
        ticket->~Ticket();
        AsyncExecutionRelease(ticket);
    };
    ticket->task.function = [ticket]()
    {
        // Queue reclamation first: run() cannot observe terminal completion before this ticket is joined.
        SC_ASYNC_TRUST_RESULT(ticket->context->post(ticket->cleanup));
        ticket->notify(ticket->operation);
    };
    context.outstanding.fetch_add(1);
    const Result queued = pool.queueTask(ticket->task);
    if (not queued)
    {
        context.outstanding.fetch_sub(1);
        ticket->~Ticket();
        AsyncExecutionRelease(ticket);
    }
    return queued;
}

Result detail::AsyncExecutionTimer::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    return request.start(loop, duration);
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

Result detail::AsyncExecutionFileRead::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
{
    request.setOffset(offset);
    SC_TRY(request.executeOn(task, pool));
    return request.start(loop, file, buffer);
}

Result detail::AsyncExecutionFileWrite::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
{
    request.setOffset(offset);
    SC_TRY(request.executeOn(task, pool));
    return request.start(loop, file, data);
}

Result detail::AsyncExecutionWork::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
{
    request.work = work;
    SC_TRY(request.setThreadPool(pool));
    return request.start(loop);
}

Result detail::AsyncExecutionOwnedRead::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
{
    SC_TRY(initialization);
    SC_TRY(request.setThreadPool(pool));
    request.work = [this]() -> Result
    {
        FileDescriptor file;
        FileOpen       options;
        options.mode = FileOpen::Read;
        SC_TRY(file.open(path.view(), options));
        size_t used = 0;
        while (true)
        {
            if (used == maximumBytes)
            {
                char       probe;
                Span<char> read;
                SC_TRY(file.read({&probe, 1}, read));
                if (not read.empty())
                    return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
                break;
            }
            const size_t available = maximumBytes - used;
            const size_t count     = available > 32768 ? 32768 : available;
            SC_TRY(data.resize(used + count));
            Span<char> read;
            SC_TRY(file.read({data.bytes().data() + used, count}, read));
            used += read.sizeInBytes();
            if (read.empty())
                break;
        }
        return data.resize(used);
    };
    return request.start(loop);
}

Result detail::AsyncExecutionOwnedWrite::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
{
    SC_TRY(initialization);
    SC_TRY(request.setThreadPool(pool));
    request.work = [this]() -> Result
    {
        FileDescriptor file;
        FileOpen       options;
        options.mode = FileOpen::Write;
        SC_TRY(file.open(path.view(), options));
        return file.write(data.bytes());
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

Result detail::AsyncExecutionOwnedSend::submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task)
{
    return send.submit(request, loop, task);
}

Result detail::AsyncExecutionSendAll::advance(Request::Result& result)
{
    const size_t count = result.completionData.numBytes;
    if (count == 0 or count > remaining.sizeInBytes())
        return Result::Error(AsyncResultCategory, AsyncError::SocketSendIncomplete);
    transferred += count;
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

AsyncExecutionBuffer::AsyncExecutionBuffer(AsyncExecutionBuffer&& other) noexcept { *this = move(other); }

AsyncExecutionBuffer& AsyncExecutionBuffer::operator=(AsyncExecutionBuffer&& other) noexcept
{
    if (this != &other)
    {
        detail::AsyncExecutionRelease(memory);
        memory       = other.memory;
        length       = other.length;
        capacity     = other.capacity;
        other.memory = nullptr;
        other.length = other.capacity = 0;
    }
    return *this;
}

AsyncExecutionBuffer::~AsyncExecutionBuffer() { detail::AsyncExecutionRelease(memory); }

Result AsyncExecutionBuffer::resize(size_t bytes)
{
    if (bytes > capacity)
    {
        auto* replacement = static_cast<char*>(detail::AsyncExecutionReallocate(memory, bytes));
        if (not replacement)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory);
        memory   = replacement;
        capacity = bytes;
    }
    length = bytes;
    return Result(true);
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
    if (initialized)
    {
        SC_ASYNC_ASSERT_RELEASE(not running);
        SC_ASYNC_TRUST_RESULT(context.close());
        SC_ASYNC_TRUST_RESULT(loop.close());
        SC_ASYNC_TRUST_RESULT(pool.destroy());
    }
}

Result AsyncExecution::create(size_t workers)
{
    if (initialized)
        return Result::Error(AsyncResultCategory, AsyncError::AlreadyInitialized);
    SocketNetworking::initNetworking();
    SC_TRY(loop.create());
    const Result createdPool = pool.create(workers);
    if (not createdPool)
    {
        (void)loop.close();
        return createdPool;
    }
    const Result createdContext = context.create();
    if (not createdContext)
    {
        (void)pool.destroy();
        (void)loop.close();
        return createdContext;
    }
    initialized = true;
    return Result(true);
}

void AsyncExecution::RunState::finishOnLoop(Result result) noexcept
{
    status    = result;
    completed = true;
    if (driving)
        context.loop.interrupt();
}

void AsyncExecution::RunState::finish(Result result) noexcept
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

AsyncExecutionSender<detail::AsyncExecutionTimer> AsyncExecutionContext::sleep(TimeMs duration)
{
    return AsyncExecutionSender<detail::AsyncExecutionTimer>{*this, {duration}};
}

AsyncExecutionSender<detail::AsyncExecutionSend> AsyncExecutionContext::send(const SocketDescriptor& socket,
                                                                             Span<const char>        data)
{
    return AsyncExecutionSender<detail::AsyncExecutionSend>{*this, {socket, data}};
}

AsyncExecutionSender<detail::AsyncExecutionSendAll> AsyncExecutionContext::sendAll(const SocketDescriptor& socket,
                                                                                   Span<const char>        data)
{
    return AsyncExecutionSender<detail::AsyncExecutionSendAll>{*this, {socket, data}};
}

AsyncExecutionSender<detail::AsyncExecutionReceive> AsyncExecutionContext::receive(const SocketDescriptor& socket,
                                                                                   Span<char>              buffer)
{
    return AsyncExecutionSender<detail::AsyncExecutionReceive>{*this, {socket, buffer}};
}

AsyncExecutionSender<detail::AsyncExecutionFileRead> AsyncExecutionContext::fileRead(const FileDescriptor& file,
                                                                                     Span<char>            buffer,
                                                                                     ThreadPool& pool, uint64_t offset)
{
    return AsyncExecutionSender<detail::AsyncExecutionFileRead>{*this, {file, buffer, pool, offset}};
}

AsyncExecutionSender<detail::AsyncExecutionFileWrite> AsyncExecutionContext::fileWrite(const FileDescriptor& file,
                                                                                       Span<const char>      data,
                                                                                       ThreadPool&           pool,
                                                                                       uint64_t              offset)
{
    return AsyncExecutionSender<detail::AsyncExecutionFileWrite>{*this, {file, data, pool, offset}};
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

AsyncExecutionSender<detail::AsyncExecutionOwnedRead> AsyncExecution::readFile(StringSpan path, size_t maximumBytes)
{
    detail::AsyncExecutionOwnedRead setup{pool, {}, Result(true), {}, maximumBytes};
    setup.initialization = detail::AsyncExecutionResolvePath(path, setup.path);
    return AsyncExecutionSender<detail::AsyncExecutionOwnedRead>{context, move(setup)};
}

AsyncExecutionSender<detail::AsyncExecutionOwnedWrite> AsyncExecution::writeFile(StringSpan           path,
                                                                                 AsyncExecutionBuffer data)
{
    detail::AsyncExecutionOwnedWrite setup{pool, {}, Result(true), move(data)};
    setup.initialization = detail::AsyncExecutionResolvePath(path, setup.path);
    return AsyncExecutionSender<detail::AsyncExecutionOwnedWrite>{context, move(setup)};
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

AsyncExecutionSender<detail::AsyncExecutionOwnedSend> detail::AsyncExecutionSendConnected::operator()(
    AsyncExecutionConnection& connection) noexcept
{
    return {*context, {connection.socket, move(data)}};
}

detail::AsyncExecutionTCPSender AsyncExecution::sendTCP(StringSpan host, uint16_t port, AsyncExecutionBuffer data,
                                                        Result status)
{
    detail::AsyncExecutionOwnedConnect setup;
    setup.initialization = status;
    if (status)
        setup.initialization = setup.address.fromAddressPort(host, port);
    return AsyncExecutionProtocol::let_value(
        AsyncExecutionSender<detail::AsyncExecutionOwnedConnect>{context, move(setup)},
        detail::AsyncExecutionSendConnected{&context, move(data)});
}

detail::AsyncExecutionTCPSender AsyncExecution::sendTCP(StringSpan host, uint16_t port,
                                                        AsyncExecutionBufferResult encoded)
{
    return sendTCP(host, port, move(encoded.data), encoded.status);
}

Result AsyncExecution::beginRun()
{
    if (not initialized)
        return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
    if (running)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::AlreadyRunning);
    running = true;

    return Result(true);
}

Result AsyncExecution::finishRun(RunState& state)
{
    // SC sender completions occur on the owner thread. Synchronous external senders can finish in start().
    if (not state.completed)
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

} // namespace SC
#endif
