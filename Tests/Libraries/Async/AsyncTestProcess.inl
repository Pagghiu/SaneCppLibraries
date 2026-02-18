// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Strings/StringBuilder.h"

namespace
{
static bool formatNamedPipePath(SC::TestReport& report, bool useThreadPool, SC::StringPath& pipePath)
{
    SC::SmallString<128> logicalName;
    return SC::StringBuilder::format(logicalName, "sc-async-test-{}-{}", report.mapPort(5400),
                                     useThreadPool ? 1 : 0) and
           SC::NamedPipeName::build(logicalName.view(), pipePath);
}
} // namespace

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
    SC_TEST_EXPECT(asyncSuccess.start(eventLoop, processSuccess.handle));
    SC_TEST_EXPECT(asyncFailure.start(eventLoop, processFailure.handle));
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
        SC_TEST_EXPECT(threadPool.create(2));
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

void SC::AsyncTest::namedPipeInputOutput(bool useThreadPool)
{
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(2));
    }

    StringPath pipePath;
    SC_TEST_EXPECT(formatNamedPipePath(report, useThreadPool, pipePath));

    NamedPipeServer        server;
    NamedPipeServerOptions serverOptions;
    serverOptions.connectionOptions.blocking       = false;
    serverOptions.posix.removeEndpointBeforeCreate = true;
    SC_TEST_EXPECT(server.create(pipePath.view(), serverOptions));

    PipeDescriptor serverConnection;
    EventObject    acceptedEvent;
    Result         acceptResult = Result::Error("NamedPipe accept not completed");

    struct AcceptContext
    {
        NamedPipeServer* server;
        PipeDescriptor*  connection;
        Result*          result;
        EventObject*     event;
    } acceptContext = {&server, &serverConnection, &acceptResult, &acceptedEvent};

    Thread acceptThread;
    SC_TEST_EXPECT(acceptThread.start(
        [&acceptContext](Thread&)
        {
            *acceptContext.result = acceptContext.server->accept(*acceptContext.connection);
            acceptContext.event->signal();
        }));

    NamedPipeClientOptions clientOptions;
    clientOptions.connectionOptions.blocking = false;

    PipeDescriptor clientConnection;
    SC_TEST_EXPECT(NamedPipeClient::connect(pipePath.view(), clientConnection, clientOptions));

    acceptedEvent.wait();
    SC_TEST_EXPECT(acceptThread.join());
    SC_TEST_EXPECT(acceptResult);

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(serverConnection.readPipe));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(clientConnection.writePipe));
    }

    SC_TEST_EXPECT(serverConnection.writePipe.close());
    SC_TEST_EXPECT(clientConnection.readPipe.close());

    AsyncFileRead  asyncRead;
    AsyncFileWrite asyncWrite;

    AsyncTaskSequence writeTask;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncWrite.executeOn(writeTask, threadPool));
    }

    SC_TEST_EXPECT(serverConnection.readPipe.get(asyncRead.handle, Result::Error("server read handle")));
    SC_TEST_EXPECT(clientConnection.writePipe.get(asyncWrite.handle, Result::Error("client write handle")));

    char readBuffer[4];
    asyncRead.buffer = {readBuffer, sizeof(readBuffer)};

    struct IOContext
    {
        int  numChunks     = 0;
        bool writeFinished = false;
    } ioContext = {0, false};

    asyncRead.callback = [this, &ioContext](AsyncFileRead::Result& res)
    {
        Span<char> data;
        SC_TEST_EXPECT(res.get(data));
        if (res.completionData.endOfFile)
        {
            return;
        }
        StringSpan span(data, false, StringEncoding::Ascii);
        SC_TEST_EXPECT(span == "asdf");
        ioContext.numChunks++;
        if (ioContext.numChunks < 4)
        {
            res.reactivateRequest(true);
        }
    };

    constexpr char payload[] = "asdfasdfasdfasdf";
    asyncWrite.buffer        = {payload, sizeof(payload) - 1};
    asyncWrite.callback      = [this, &ioContext](AsyncFileWrite::Result& res)
    {
        size_t writtenSize = 0;
        SC_TEST_EXPECT(res.get(writtenSize));
        SC_TEST_EXPECT(writtenSize == (sizeof(payload) - 1));
        ioContext.writeFinished = true;
    };

    SC_TEST_EXPECT(asyncRead.start(eventLoop));
    SC_TEST_EXPECT(asyncWrite.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(clientConnection.writePipe.close());
    SC_TEST_EXPECT(ioContext.writeFinished);
    SC_TEST_EXPECT(ioContext.numChunks == 4);

    SC_TEST_EXPECT(serverConnection.readPipe.close());
    SC_TEST_EXPECT(serverConnection.writePipe.close());
    SC_TEST_EXPECT(clientConnection.readPipe.close());
    SC_TEST_EXPECT(server.close());
}
