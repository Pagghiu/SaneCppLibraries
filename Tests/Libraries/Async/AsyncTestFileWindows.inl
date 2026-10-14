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
#endif
