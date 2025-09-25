// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Process/Process.h"

void SC::AsyncTest::processExit()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    Process processSuccess;
    Process processFailure;
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(processSuccess.launch({"where", "where.exe"}));        // Returns 0 error code
    SC_TEST_EXPECT(processFailure.launch({"cmd", "/C", "dir /DOCTORS"})); // Returns 1 error code
#else
    // Must wait for the process to be still active when adding it to kqueue
    SC_TEST_EXPECT(processSuccess.launch({"sleep", "0.2"})); // Returns 0 error code
    SC_TEST_EXPECT(processFailure.launch({"ls", "/~"}));     // Returns 1 error code
#endif
    ProcessDescriptor::Handle processHandleSuccess = 0;
    SC_TEST_EXPECT(processSuccess.handle.get(processHandleSuccess, Result::Error("Invalid Handle 1")));
    ProcessDescriptor::Handle processHandleFailure = 0;
    SC_TEST_EXPECT(processFailure.handle.get(processHandleFailure, Result::Error("Invalid Handle 2")));
    AsyncProcessExit asyncSuccess;
    AsyncProcessExit asyncFailure;

    struct OutParams
    {
        int numCallbackCalled = 0;
        int exitStatus        = -1;
    };
    OutParams outParams1;
    OutParams outParams2;
    asyncSuccess.setDebugName("asyncSuccess");
    asyncSuccess.callback = [&](AsyncProcessExit::Result& res)
    {
        SC_TEST_EXPECT(res.get(outParams1.exitStatus));
        outParams1.numCallbackCalled++;
    };
    asyncFailure.setDebugName("asyncFailure");
    asyncFailure.callback = [&](AsyncProcessExit::Result& res)
    {
        SC_TEST_EXPECT(res.get(outParams2.exitStatus));
        outParams2.numCallbackCalled++;
    };
    SC_TEST_EXPECT(asyncSuccess.start(eventLoop, processHandleSuccess));
    SC_TEST_EXPECT(asyncFailure.start(eventLoop, processHandleFailure));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(outParams1.numCallbackCalled == 1);
    SC_TEST_EXPECT(outParams1.exitStatus == 0); // Status == Ok
    SC_TEST_EXPECT(outParams2.numCallbackCalled == 1);
    SC_TEST_EXPECT(outParams2.exitStatus != 0); // Status == Not OK
}

void SC::AsyncTest::processInputOutput(bool useThreadPool)
{
    StringSpan params[] = {report.executableFile.view(), "--quiet", "--test", "AsyncTest", "--test-section",
                           "process input output child"};
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(1));
    }
    PipeDescriptor processStdOut;
    PipeOptions    pipeOptions;

    pipeOptions.blocking         = useThreadPool;
    pipeOptions.writeInheritable = true;
    SC_TEST_EXPECT(processStdOut.createPipe(pipeOptions));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncFileRead asyncRead;

    int numCallbackCalled = 0;

    asyncRead.callback = [&](AsyncFileRead::Result& res)
    {
        if (not res.completionData.endOfFile)
        {
            Span<char> data;
            SC_TEST_EXPECT(res.get(data));
            StringSpan span(data, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(span == "asdf");
            numCallbackCalled++;
            res.reactivateRequest(true);
        }
    };
    AsyncTaskSequence asyncReadTask;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncRead.executeOn(asyncReadTask, threadPool));
    }
    else
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(processStdOut.readPipe));
    }
    SC_TEST_EXPECT(processStdOut.readPipe.get(asyncRead.handle, Result::Error("handle")));
    char myBuffer[4]; // just enough to hold "asdf";
    asyncRead.buffer = myBuffer;
    SC_TEST_EXPECT(asyncRead.start(eventLoop));
    Process process;
    SC_TEST_EXPECT(process.launch(params, processStdOut));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(numCallbackCalled == 4);
}

void SC::AsyncTest::processInputOutputChild()
{
    FileDescriptor stdOut;
    SC_TEST_EXPECT(stdOut.openStdOutDuplicate());
    for (int i = 0; i < 4; ++i)
    {
        SC_TEST_EXPECT(stdOut.writeString("asdf"));
        Thread::Sleep(1); // just to simulate some delay
    }
    Thread::Sleep(10); // wait before closing
}
