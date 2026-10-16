// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Time/Time.h"
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h> // Failure-only independent deadline diagnostic

void SC::AsyncTest::fileReadCancelQuiescence(bool drainBeforeCancel)
{
    Atomic<bool> finished = false;
    Atomic<int>  phase    = 0;
    Thread       deadline;
    SC_TEST_EXPECT(deadline.start(
        [&finished, &phase](Thread&)
        {
            const auto started = Time::Monotonic::now();
            while (not finished.load() and
                   Time::Monotonic::now().getMonotonicMilliseconds() - started.getMonotonicMilliseconds() < 8000)
                Thread::Sleep(10);
            if (not finished.load())
            {
                fprintf(stderr, "Native file cancellation deadline: phase %d\n", phase.load());
                fflush(stderr);
                ::ExitProcess(124);
            }
        }));
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());
    PipeDescriptor pipe;
    PipeOptions    mode;
    mode.blocking = false;
    SC_TEST_EXPECT(pipe.createPipe(mode));
    SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(pipe.readPipe));
    SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(pipe.writePipe));
    char          loan[1] = {};
    AsyncFileRead read;
    int           callbacks = 0;
    read.callback           = [&callbacks](AsyncFileRead::Result&) { ++callbacks; };
    SC_TEST_EXPECT(read.start(loop, pipe.readPipe, loan));
    SC_TEST_EXPECT(loop.runNoWait());
    SC_TEST_EXPECT(loop.runNoWait());
    SC_TEST_EXPECT(not read.isFree() and callbacks == 0);
    int                          stops        = 0;
    Function<void(AsyncResult&)> afterStopped = [&](AsyncResult&)
    {
        ++stops;
        loan[0] = 'z';
    };
    phase.store(1);
    SC_TEST_EXPECT(read.stop(loop, &afterStopped));
    SC_TEST_EXPECT(loop.run());
    // A stop callback must not leave any I/O attached to this handle. On the old backend
    // this call cancels the still-pending read, making the defect directly observable.
    FileDescriptor::Handle handle;
    SC_TEST_EXPECT(pipe.readPipe.get(handle, asyncTestFailure(AsyncTestFailure::ReadPipeUnavailable)));
    const BOOL  pending = ::CancelIoEx(handle, nullptr);
    const DWORD error   = ::GetLastError();
    SC_TEST_EXPECT(pending == FALSE and error == ERROR_NOT_FOUND);
    SC_TEST_EXPECT(stops == 1 and read.isFree() and callbacks == 0 and loan[0] == 'z');
    // Complete a read in the kernel before asking to stop it, then reuse its OVERLAPPED storage.
    phase.store(2);
    AsyncFileWrite writer;
    SC_TEST_EXPECT(writer.start(loop, pipe.writePipe, StringView("x").toCharSpan()));
    SC_TEST_EXPECT(loop.run());
    phase.store(3);
    SC_TEST_EXPECT(read.start(loop, pipe.readPipe, loan));
    alignas(uint64_t) uint8_t storage[8192];
    AsyncKernelEvents         events;
    events.eventsMemory = storage;
    SC_TEST_EXPECT(loop.submitRequests(events));
    SC_TEST_EXPECT(loan[0] == 'x');
    phase.store(4);
    if (drainBeforeCancel)
        SC_TEST_EXPECT(loop.blockingPoll(events));
    SC_TEST_EXPECT(read.stop(loop, &afterStopped));
    // A dispatcher can mark a request cancelling while its completion is already in this batch,
    // before the queued cancellation API call is submitted by the next run step.
    if (drainBeforeCancel)
        SC_TEST_EXPECT(loop.dispatchCompletions(events));
    SC_TEST_EXPECT(loop.run());
    phase.store(5);
    SC_TEST_EXPECT(stops == 2 and callbacks == 0 and read.isFree() and loan[0] == 'z');
    SC_TEST_EXPECT(read.start(loop, pipe.readPipe, loan));
    SC_TEST_EXPECT(loop.runNoWait());
    SC_TEST_EXPECT(loop.runNoWait());
    SC_TEST_EXPECT(not read.isFree() and callbacks == 0 and loan[0] == 'z');
    phase.store(6);
    SC_TEST_EXPECT(read.stop(loop, &afterStopped));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(stops == 3 and callbacks == 0 and read.isFree());
    SC_TEST_EXPECT(pipe.close());
    SC_TEST_EXPECT(loop.close());
    finished.store(true);
    SC_TEST_EXPECT(deadline.join());
}

void SC::AsyncTest::fileWorkerAssociatedCompletion()
{
    Atomic<bool> finished = false;
    Atomic<int>  phase    = 0;
    Thread       deadline;
    SC_TEST_EXPECT(deadline.start(
        [&finished, &phase](Thread&)
        {
            const auto started = Time::Monotonic::now();
            while (not finished.load() and
                   Time::Monotonic::now().getMonotonicMilliseconds() - started.getMonotonicMilliseconds() < 8000)
                Thread::Sleep(10);
            if (not finished.load())
            {
                fprintf(stderr, "Associated worker deadline: phase %d\n", phase.load());
                fflush(stderr);
                ::ExitProcess(124);
            }
        }));
    AsyncEventLoop loop;
    ThreadPool     pool;
    SC_TEST_EXPECT(loop.create());
    SC_TEST_EXPECT(pool.create(1));
    StringPath path;
    SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncAssociatedWorker.txt"}));
    FileDescriptor file;
    FileOpen       mode;
    mode.mode     = FileOpen::WriteRead;
    mode.blocking = false;
    SC_TEST_EXPECT(file.open(path.view(), mode));
    SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(file));
    AsyncFileWrite write;
    int            writes = 0;
    write.callback        = [this, &writes](AsyncFileWrite::Result& result)
    {
        size_t bytes = 0;
        SC_TEST_EXPECT(result.get(bytes));
        SC_TEST_EXPECT(bytes == 6);
        ++writes;
    };
    write.setOffset(0);
    SC_TEST_EXPECT(write.start(loop, file, StringView("native").toCharSpan()));
    phase.store(1);
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(writes == 1 and write.isFree());
    AsyncTaskSequence task;
    SC_TEST_EXPECT(write.executeOn(task, pool, AsyncThreadPoolMode::ForceThreadPool));
    SC_TEST_EXPECT(write.start(loop, file, StringView("worker").toCharSpan()));
    alignas(uint64_t) uint8_t storage[8192];
    AsyncKernelEvents         events;
    events.eventsMemory = storage;
    SC_TEST_EXPECT(loop.submitRequests(events));
    phase.store(2);
    SC_TEST_EXPECT(pool.waitForAllTasks());
    phase.store(3);
    SC_TEST_EXPECT(loop.dispatchCompletions(events));
    phase.store(4);
    SC_TEST_EXPECT(writes == 2 and write.isFree());
    // Force a kernel poll after manual worker completion. It must not redispatch the same request.
    AsyncLoopTimeout poll;
    SC_TEST_EXPECT(poll.start(loop, {1}));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(writes == 2 and write.isFree());
    // The shared worker path must also isolate each vector element and read completion.
    Span<const char> vectors[] = {StringView("wo").toCharSpan(), StringView("rk").toCharSpan(),
                                  StringView("er").toCharSpan()};
    phase.store(5);
    SC_TEST_EXPECT(write.start(loop, file, vectors));
    SC_TEST_EXPECT(loop.submitRequests(events));
    SC_TEST_EXPECT(pool.waitForAllTasks());
    SC_TEST_EXPECT(loop.dispatchCompletions(events));
    SC_TEST_EXPECT(writes == 3 and write.isFree());
    SC_TEST_EXPECT(poll.start(loop, {1}));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(writes == 3 and write.isFree());

    char          loan[6] = {};
    AsyncFileRead read;
    int           reads = 0;
    read.callback       = [this, &reads](AsyncFileRead::Result& result)
    {
        Span<char> received;
        SC_TEST_EXPECT(result.get(received));
        SC_TEST_EXPECT(received.sizeInBytes() == 6);
        SC_TEST_EXPECT(StringView(received, false, StringEncoding::Ascii) == "worker");
        ++reads;
    };
    read.setOffset(0);
    phase.store(6);
    SC_TEST_EXPECT(read.start(loop, file, loan));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(reads == 1 and read.isFree());
    AsyncTaskSequence readTask;
    SC_TEST_EXPECT(read.executeOn(readTask, pool, AsyncThreadPoolMode::ForceThreadPool));
    SC_TEST_EXPECT(read.start(loop, file, loan));
    SC_TEST_EXPECT(loop.submitRequests(events));
    phase.store(7);
    SC_TEST_EXPECT(pool.waitForAllTasks());
    SC_TEST_EXPECT(loop.dispatchCompletions(events));
    SC_TEST_EXPECT(reads == 2 and read.isFree());
    SC_TEST_EXPECT(poll.start(loop, {1}));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(reads == 2 and read.isFree());
    SC_TEST_EXPECT(file.close());
    SC_TEST_EXPECT(FileSystem::Operations::removeFile(path.view()));
    SC_TEST_EXPECT(loop.close());
    SC_TEST_EXPECT(pool.destroy());
    finished.store(true);
    SC_TEST_EXPECT(deadline.join());
}
#endif
