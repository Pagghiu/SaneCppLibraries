// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"

#include "AsyncTestFile.inl"
#include "AsyncTestLoop.inl"
#include "AsyncTestLoopTimeout.inl"
#include "AsyncTestLoopWakeUp.inl"
#include "AsyncTestLoopWork.inl"
#include "AsyncTestProcess.inl"
#include "AsyncTestSocket.inl"

SC::AsyncTest::AsyncTest(SC::TestReport& report) : TestCase(report, "AsyncTest")
{
    int numTestsToRun = 1;
    if (AsyncEventLoop::tryLoadingLiburing())
    {
        // Run all tests on epoll backend first, and then re-run them on io_uring
        options.apiType = AsyncEventLoop::Options::ApiType::ForceUseEpoll;
        numTestsToRun   = 2;
    }
    for (int i = 0; i < numTestsToRun; ++i)
    {
        if (test_section("loop free submitting on close"))
        {
            loopFreeSubmittingOnClose();
        }
        if (test_section("loop free active on close"))
        {
            loopFreeActiveOnClose();
        }
        if (test_section("loop work"))
        {
            loopWork();
        }
        if (test_section("loop timeout"))
        {
            loopTimeout();
        }
        if (test_section("loop wakeUpFromExternalThread"))
        {
            loopWakeUpFromExternalThread();
        }
        if (test_section("loop wakeUp"))
        {
            loopWakeUp();
        }
        if (test_section("loop wakeUp eventObject"))
        {
            loopWakeUpEventObject();
        }
        if (test_section("process exit"))
        {
            processExit();
        }
        if (test_section("socket accept"))
        {
            socketAccept();
        }
        if (test_section("socket connect"))
        {
            socketConnect();
        }
        if (test_section("socket send/receive"))
        {
            socketSendReceive();
        }
        if (test_section("error send/receive"))
        {
            socketSendReceiveError();
        }
        if (test_section("socket close"))
        {
            socketClose();
        }
        if (test_section("file read/write"))
        {
            fileReadWrite(false); // do not use thread-pool
            fileReadWrite(true);  // use thread-pool
        }
        if (test_section("file endOfFile"))
        {
            fileEndOfFile(false); // do not use thread-pool
            fileEndOfFile(true);  // use thread-pool
        }
        if (test_section("file close"))
        {
            fileClose();
        }
        if (numTestsToRun == 2)
        {
            // If on Linux next run will test io_uring backend (if it's installed)
            options.apiType = AsyncEventLoop::Options::ApiType::ForceUseIOURing;
        }
    }
}

namespace SC
{
void runAsyncTest(SC::TestReport& report) { AsyncTest test(report); }
} // namespace SC

namespace SC
{
// clang-format off
Result snippetForEventLoop()
{
//! [AsyncEventLoopSnippet]
AsyncEventLoop eventLoop;
SC_TRY(eventLoop.create()); // Create OS specific queue handles
// ...
// Add all needed AsyncRequest
// ...
SC_TRY(eventLoop.run());
// ...
// Here all AsyncRequest have either finished or have been stopped
// ...
SC_TRY(eventLoop.close()); // Free OS specific queue handles
//! [AsyncEventLoopSnippet]
return Result(true);
}

SC::Result snippetForTimeout(AsyncEventLoop& eventLoop, Console& console)
{
    bool someCondition = false;
//! [AsyncLoopTimeoutSnippet]
// Create a timeout that will be called after 200 milliseconds
// AsyncLoopTimeout must be valid until callback is called
AsyncLoopTimeout timeout;
timeout.callback = [&](AsyncLoopTimeout::Result& res)
{
    console.print("My timeout has been called!");
    if (someCondition) // Optionally re-activate the timeout if needed
    {
        // Schedule the timeout callback to fire again 100 ms from now
        res.getAsync().relativeTimeout = Time::Milliseconds(100);
        res.reactivateRequest(true);
    }
};
// Start the timeout, that will be called 200 ms from now
SC_TRY(timeout.start(eventLoop, 200_ms));
//! [AsyncLoopTimeoutSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForWakeUp1(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncLoopWakeUpSnippet1]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
// This code runs on some different thread from the one calling SC::AsyncEventLoop::run.
// The callback is invoked from the thread calling SC::AsyncEventLoop::run
AsyncLoopWakeUp wakeUp; // Memory lifetime must be valid until callback is called
wakeUp.callback = [&](AsyncLoopWakeUp::Result& result)
{
    console.print("My wakeUp has been called!");
    result.reactivateRequest(true); // To allow waking-up again later
};
SC_TRY(wakeUp.start(eventLoop));
//! [AsyncLoopWakeUpSnippet1]
return Result(true);
}

SC::Result snippetForWakeUp2(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncLoopWakeUpSnippet2]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
// This code runs on some different thread from the one calling SC::AsyncEventLoop::run.
// The callback is invoked from the thread calling SC::AsyncEventLoop::run
AsyncLoopWakeUp wakeUpWaiting; // Memory lifetime must be valid until callback is called
wakeUpWaiting.callback = [&](AsyncLoopWakeUp::Result& result)
{
    console.print("My wakeUp has been called!");
    result.reactivateRequest(true); // To allow waking-up it again later
};
EventObject eventObject;
SC_TRY(wakeUpWaiting.start(eventLoop, &eventObject));
eventObject.wait(); // Wait until callback has been fully run inside event loop thread
// From here on we know for sure that callback has been called
//! [AsyncLoopWakeUpSnippet2]
return Result(true);
}

SC::Result snippetForProcess(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncProcessSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
Process process;
SC_TRY(process.launch({"executable", "--parameter"}));
ProcessDescriptor::Handle processHandle;
SC_TRY(process.handle.get(processHandle, Result::Error("Invalid Handle")));
AsyncProcessExit processExit; //  Memory lifetime must be valid until callback is called
processExit.callback = [&](AsyncProcessExit::Result& res)
{
    ProcessDescriptor::ExitStatus exitStatus;
    if(res.get(exitStatus))
    {
        console.print("Process Exit status = {}", exitStatus.status);
    }
};
SC_TRY(processExit.start(eventLoop, processHandle));
//! [AsyncProcessSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketAccept(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncSocketAcceptSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
// Create a listening socket
constexpr uint32_t numWaitingConnections = 2;
SocketDescriptor   serverSocket;
uint16_t           tcpPort = 5050;
SocketIPAddress    nativeAddress;
SC_TRY(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
SC_TRY(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
SocketServer server(serverSocket);
SC_TRY(server.bind(nativeAddress));
SC_TRY(server.listen(numWaitingConnections));
// Accept connect for new clients
AsyncSocketAccept accept;
accept.callback = [&](AsyncSocketAccept::Result& res)
{
    SocketDescriptor client;
    if(res.moveTo(client))
    {
        // ...do something with new client
        console.printLine("New client connected!");
        res.reactivateRequest(true); // We want to receive more clients
    }
};
SC_TRY(accept.start(eventLoop, serverSocket));
// ... at some later point
// Stop accepting new clients
SC_TRY(accept.stop());
//! [AsyncSocketAcceptSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketConnect(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncSocketConnectSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
SocketIPAddress localHost;
SC_TRY(localHost.fromAddressPort("127.0.0.1", 5050)); // Connect to some host and port
AsyncSocketConnect connect;
SocketDescriptor   client;
SC_TRY(eventLoop.createAsyncTCPSocket(localHost.getAddressFamily(), client));
connect.callback = [&](AsyncSocketConnect::Result& res)
{
    if (res.isValid())
    {
        // Do something with client that is now connected
        console.printLine("Client connected");
    }
};
SC_TRY(connect.start(eventLoop, client, localHost));
//! [AsyncSocketConnectSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketSend(AsyncEventLoop& eventLoop, Console& console)
{
SocketDescriptor client;
//! [AsyncSocketSendSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// and a connected or accepted socket named `client`
// ...
const char sendBuffer[] = {123, 111};

// The memory pointed by the span must be valid until callback is called
Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

AsyncSocketSend sendAsync;
sendAsync.callback = [&](AsyncSocketSend::Result& res)
{
    if(res.isValid())
    {
        // Now we could free the data pointed by span and queue new data
        console.printLine("Ready to send more data");
    }
};

SC_TRY(sendAsync.start(eventLoop, client, sendData));
//! [AsyncSocketSendSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketReceive(AsyncEventLoop& eventLoop, Console& console)
{
SocketDescriptor client;
//! [AsyncSocketReceiveSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// and a connected or accepted socket named `client`
// ...
char receivedData[100] = {0}; // A buffer to hold data read from the socket
AsyncSocketReceive receiveAsync;
receiveAsync.callback = [&](AsyncSocketReceive::Result& res)
{
    Span<char> readData;
    if(res.get(readData))
    {
        if(res.completionData.disconnected)
        {
            // Last callback invocation done when other side of the socket has disconnected.
            // - completionData.disconnected is == true
            // - readData.sizeInBytes() is == 0
            console.print("Client disconnected");
        }
        else
        {
            // readData is a slice of receivedData with the received bytes
            console.print("{} bytes have been read", readData.sizeInBytes());
            
            // IMPORTANT: Reactivate the request to receive more data
            res.reactivateRequest(true);
        }
    }
    else
    {
        // Some error occurred, check res.returnCode
    }
};
SC_TRY(receiveAsync.start(eventLoop, client, {receivedData, sizeof(receivedData)}));
//! [AsyncSocketReceiveSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketClose(AsyncEventLoop& eventLoop, Console& console)
{
SocketDescriptor client;
//! [AsyncSocketCloseSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// and a connected or accepted socket named `client`
// ...
AsyncSocketClose asyncClose;

asyncClose.callback = [&](AsyncSocketClose::Result& result)
{
    if(result.isValid())
    {
        console.printLine("Socket was closed successfully");
    }
};
SC_TRY(asyncClose.start(eventLoop, client));

//! [AsyncSocketCloseSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForFileRead(AsyncEventLoop& eventLoop, Console& console)
{
ThreadPool threadPool;
SC_TRY(threadPool.create(4));
//! [AsyncFileReadSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// ...

// Assuming an already created threadPool named `eventLoop
// ...

// Open the file
FileDescriptor fd;
File::OpenOptions options;
options.blocking = true; // AsyncFileRead::Task enables using regular blocking file descriptors
SC_TRY(File(fd).open("MyFile.txt", File::ReadOnly, options));

// Create the async file read request and async task
AsyncFileRead asyncReadFile;
asyncReadFile.callback = [&](AsyncFileRead::Result& res)
{
    Span<char> readData;
    if(res.get(readData))
    {
        if(res.completionData.endOfFile)
        {
            // Last callback invocation done when end of file has been reached
            // - completionData.endOfFile is == true
            // - readData.sizeInBytes() is == 0
            console.print("End of file reached");
        }
        else
        {
            // readData is a slice of receivedData with the received bytes
            console.print("Read {} bytes from file", readData.sizeInBytes());
            
            // OPTIONAL: Update file offset to receive a different range of bytes
            res.getAsync().setOffset(res.getAsync().getOffset()  + readData.sizeInBytes());
            
            // IMPORTANT: Reactivate the request to receive more data
            res.reactivateRequest(true);
        }
    }
    else
    {
        // Some error occurred, check res.returnCode
    }
};
char buffer[100] = {0};
asyncReadFile.buffer = {buffer, sizeof(buffer)};
// Obtain file descriptor handle and associate it with event loop
SC_TRY(fd.get(asyncReadFile.fileDescriptor, Result::Error("Invalid handle")));

// Start the operation on a thread pool
AsyncFileRead::Task asyncFileTask;
SC_TRY(asyncReadFile.setThreadPoolAndTask(threadPool, asyncFileTask));
SC_TRY(asyncReadFile.start(eventLoop));

// Alternatively if the file is opened with blocking == false, AsyncFileRead can be omitted
// but the operation will not be fully async on regular (buffered) files, except on io_uring.
//
// SC_TRY(asyncReadFile.start(eventLoop));
//! [AsyncFileReadSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}


SC::Result snippetForFileWrite(AsyncEventLoop& eventLoop, Console& console)
{
ThreadPool threadPool;
SC_TRY(threadPool.create(4));
//! [AsyncFileWriteSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// ...

// Assuming an already created threadPool named `threadPool`
// ...

// Open the file (for write)
File::OpenOptions options;
options.blocking = true; // AsyncFileWrite::Task enables using regular blocking file descriptors
FileDescriptor fd;
SC_TRY(File(fd).open("MyFile.txt", File::WriteCreateTruncate, options));

// Create the async file write request
AsyncFileWrite asyncWriteFile;
asyncWriteFile.callback = [&](AsyncFileWrite::Result& res)
{
    size_t writtenBytes = 0;
    if(res.get(writtenBytes))
    {
        console.print("{} bytes have been written", writtenBytes);
    }
};
// Obtain file descriptor handle
SC_TRY(fd.get(asyncWriteFile.fileDescriptor, Result::Error("Invalid Handle")));
asyncWriteFile.buffer = StringView("test").toCharSpan();;

// Start the operation in a thread pool
AsyncFileWrite::Task asyncFileTask;
SC_TRY(asyncWriteFile.setThreadPoolAndTask(threadPool, asyncFileTask));
SC_TRY(asyncWriteFile.start(eventLoop));

// Alternatively if the file is opened with blocking == false, AsyncFileRead can be omitted
// but the operation will not be fully async on regular (buffered) files, except on io_uring.
//
// SC_TRY(asyncWriteFile.start(eventLoop));
//! [AsyncFileWriteSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForFileClose(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncFileCloseSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...

// Open a file and associated it with event loop
FileDescriptor fd;
File::OpenOptions options;
options.blocking = false;
SC_TRY(File(fd).open("MyFile.txt", File::WriteCreateTruncate, options));
SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(fd));

// Create the file close request
FileDescriptor::Handle handle;
SC_TRY(fd.get(handle, Result::Error("handle")));
AsyncFileClose asyncFileClose;
asyncFileClose.callback = [&](AsyncFileClose::Result& result)
{
    if(result.isValid())
    {
        console.printLine("File was closed successfully");
    }
};
SC_TRY(asyncFileClose.start(eventLoop, handle));
//! [AsyncFileCloseSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}
// clang-format on
} // namespace SC
