// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"
#include "../../File/File.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Process/Process.h"
#include "../../Socket/Socket.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"
#include "../../Threading/Threading.h" // EventObject

namespace SC
{
struct AsyncTest;
}

struct SC::AsyncTest : public SC::TestCase
{
    AsyncEventLoop::Options options;
    AsyncTest(SC::TestReport& report) : TestCase(report, "AsyncTest")
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
    void createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client, SocketDescriptor& serverSideClient);

    void loopFreeSubmittingOnClose();
    void loopFreeActiveOnClose();
    void loopWork();
    void loopTimeout();
    void loopWakeUpFromExternalThread();
    void loopWakeUp();
    void loopWakeUpEventObject();
    void processExit();
    void socketAccept();
    void socketConnect();
    void socketSendReceive();
    void socketClose();
    void socketSendReceiveError();
    void fileReadWrite(bool useThreadPool);
    void fileEndOfFile(bool useThreadPool);
    void fileClose();
};

void SC::AsyncTest::createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client,
                                        SocketDescriptor& serverSideClient)
{
    SocketDescriptor serverSocket;
    uint16_t         tcpPort        = 5050;
    StringView       connectAddress = "::1";
    SocketIPAddress  nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
    SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));

    {
        SocketServer server(serverSocket);
        SC_TEST_EXPECT(server.bind(nativeAddress));
        SC_TEST_EXPECT(server.listen(0));
    }

    SC_TEST_EXPECT(client.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(client).connect(connectAddress, tcpPort));
    SC_TEST_EXPECT(SocketServer(serverSocket).accept(nativeAddress.getAddressFamily(), serverSideClient));
    SC_TEST_EXPECT(client.setBlocking(false));
    SC_TEST_EXPECT(serverSideClient.setBlocking(false));

    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(client));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(serverSideClient));
}

void SC::AsyncTest::loopFreeSubmittingOnClose()
{
    // This test checks that on close asyncs being submitted are being removed for submission queue and set as Free.
    AsyncLoopTimeout  loopTimeout[2];
    AsyncLoopWakeUp   loopWakeUp[2];
    AsyncSocketAccept socketAccept[2];

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    SC_TEST_EXPECT(loopTimeout[0].start(eventLoop, Time::Milliseconds(12)));
    SC_TEST_EXPECT(loopTimeout[1].start(eventLoop, Time::Milliseconds(122)));
    SC_TEST_EXPECT(loopWakeUp[0].start(eventLoop));
    SC_TEST_EXPECT(loopWakeUp[1].start(eventLoop));
    constexpr uint32_t numWaitingConnections = 2;
    SocketDescriptor   serverSocket[2];
    SocketIPAddress    serverAddress[2];
    SC_TEST_EXPECT(serverAddress[0].fromAddressPort("127.0.0.1", 5052));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[0].getAddressFamily(), serverSocket[0]));
    {
        SocketServer server(serverSocket[0]);
        SC_TEST_EXPECT(server.bind(serverAddress[0]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(serverAddress[1].fromAddressPort("127.0.0.1", 5053));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[1].getAddressFamily(), serverSocket[1]));
    {
        SocketServer server(serverSocket[1]);
        SC_TEST_EXPECT(server.bind(serverAddress[1]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(socketAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(socketAccept[1].start(eventLoop, serverSocket[1]));

    // All the above requests are in submitting state, but we just abruptly close the loop
    SC_TEST_EXPECT(eventLoop.close());

    // So let's try using them again, and we should get no errors of anything "in use"
    SC_TEST_EXPECT(eventLoop.create());
    SC_TEST_EXPECT(loopTimeout[0].start(eventLoop, Time::Milliseconds(12)));
    SC_TEST_EXPECT(loopTimeout[1].start(eventLoop, Time::Milliseconds(123)));
    SC_TEST_EXPECT(loopWakeUp[0].start(eventLoop));
    SC_TEST_EXPECT(loopWakeUp[1].start(eventLoop));
    SC_TEST_EXPECT(socketAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(socketAccept[1].start(eventLoop, serverSocket[1]));
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::loopFreeActiveOnClose()
{
    // This test checks that on close active asyncs are being removed for submission queue and set as Free.
    AsyncEventLoop   eventLoop;
    SocketDescriptor acceptedClient[3];

    SC_TEST_EXPECT(eventLoop.create(options));

    constexpr uint32_t numWaitingConnections = 2;
    SocketDescriptor   serverSocket[2];
    SocketIPAddress    serverAddress[2];
    SC_TEST_EXPECT(serverAddress[0].fromAddressPort("127.0.0.1", 5052));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[0].getAddressFamily(), serverSocket[0]));
    {
        SocketServer server(serverSocket[0]);
        SC_TEST_EXPECT(server.bind(serverAddress[0]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(serverAddress[1].fromAddressPort("127.0.0.1", 5053));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[1].getAddressFamily(), serverSocket[1]));

    {
        SocketServer server(serverSocket[1]);
        SC_TEST_EXPECT(server.bind(serverAddress[1]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    AsyncSocketAccept asyncAccept[2];
    SC_TEST_EXPECT(asyncAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(asyncAccept[1].start(eventLoop, serverSocket[1]));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    // After runNoWait now the two AsyncSocketAccept are active
    SC_TEST_EXPECT(eventLoop.close()); // but closing should make them available again

    // So let's try using them again, and we should get no errors
    SC_TEST_EXPECT(eventLoop.create(options));
    SC_TEST_EXPECT(asyncAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(asyncAccept[1].start(eventLoop, serverSocket[1]));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::loopWork()
{
    //! [AsyncLoopWorkSnippet1]
    // This test creates a thread pool with 4 thread and 16 AsyncLoopWork.
    // All the 16 AsyncLoopWork are scheduled to do some work on a background thread.
    // After work is done, their respective after-work callback is invoked on the event loop thread.

    static constexpr int NUM_THREADS = 4;
    static constexpr int NUM_WORKS   = NUM_THREADS * NUM_THREADS;

    ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(NUM_THREADS));

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    AsyncLoopWork works[NUM_WORKS];

    int         numAfterWorkCallbackCalls = 0;
    Atomic<int> numWorkCallbackCalls      = 0;

    for (int idx = 0; idx < NUM_WORKS; ++idx)
    {
        works[idx].work = [&]
        {
            // This work callback is called on some random threadPool thread
            Thread::Sleep(50);                 // Execute some work on the thread
            numWorkCallbackCalls.fetch_add(1); // Atomically increment this counter
            return Result(true);
        };
        works[idx].callback = [&](AsyncLoopWork::Result&)
        {
            // This after-work callback is invoked on the event loop thread.
            // More precisely this runs on the thread calling eventLoop.run().
            numAfterWorkCallbackCalls++; // No need for atomics here, callback is run inside loop thread
        };
        // Must always call setThreadPool at least once before start
        SC_TEST_EXPECT(works[idx].setThreadPool(threadPool));
        SC_TEST_EXPECT(works[idx].start(eventLoop));
    }
    SC_TEST_EXPECT(eventLoop.run());

    // Check that callbacks have been actually called
    SC_TEST_EXPECT(numWorkCallbackCalls.load() == NUM_WORKS);
    SC_TEST_EXPECT(numAfterWorkCallbackCalls == NUM_WORKS);
    //! [AsyncLoopWorkSnippet1]
}

void SC::AsyncTest::loopTimeout()
{
    AsyncLoopTimeout timeout1, timeout2;
    AsyncEventLoop   eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    int timeout1Called = 0;
    int timeout2Called = 0;
    timeout1.callback  = [&](AsyncLoopTimeout::Result& res)
    {
        SC_TEST_EXPECT(res.getAsync().relativeTimeout.ms == 1);
        SC_TEST_EXPECT(res.getAsync().isFree());
        SC_TEST_EXPECT(not res.getAsync().isActive());
        SC_TEST_EXPECT(not res.getAsync().isCancelling());
        timeout1Called++;
    };
    SC_TEST_EXPECT(timeout1.start(eventLoop, Time::Milliseconds(1)));
    timeout2.callback = [&](AsyncLoopTimeout::Result& res)
    {
        if (timeout2Called == 0)
        {
            // Re-activate timeout2, modifying also its relative timeout to 1 ms (see SC_TEST_EXPECT below)
            SC_TEST_EXPECT(res.getAsync().isFree());
            SC_TEST_EXPECT(not res.getAsync().isActive());
            res.reactivateRequest(true);
            SC_TEST_EXPECT(res.getAsync().isActive());
            res.getAsync().relativeTimeout = Time::Milliseconds(1);
        }
        timeout2Called++;
    };
    SC_TEST_EXPECT(timeout2.start(eventLoop, Time::Milliseconds(100)));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 0); // timeout1 fires after 1 ms
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 1); // timeout2 fires after 100 ms
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 2); // Re-activated timeout2 fires again after 1 ms
}

void SC::AsyncTest::loopWakeUpFromExternalThread()
{
    // TODO: This test is not actually testing anything (on Linux)
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    Thread newThread;

    struct Context
    {
        AsyncEventLoop& eventLoop;
        int             threadWasCalled;
        int             wakeUpSucceeded;
    } context                 = {eventLoop, 0, 0};
    auto externalThreadLambda = [this, &context](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test"));
        context.threadWasCalled++;
        if (context.eventLoop.wakeUpFromExternalThread())
        {
            context.wakeUpSucceeded++;
        }
    };
    SC_TEST_EXPECT(newThread.start(externalThreadLambda));
    Time::HighResolutionCounter start, end;
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(newThread.join());
    SC_TEST_EXPECT(newThread.start(externalThreadLambda));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(newThread.join());
    SC_TEST_EXPECT(context.threadWasCalled == 2);
    SC_TEST_EXPECT(context.wakeUpSucceeded == 2);
}

void SC::AsyncTest::loopWakeUp()
{
    struct Context
    {
        int      wakeUp1Called   = 0;
        int      wakeUp2Called   = 0;
        uint64_t wakeUp1ThreadID = 0;
    } context;
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    AsyncLoopWakeUp wakeUp1;
    AsyncLoopWakeUp wakeUp2;
    wakeUp1.setDebugName("wakeUp1");
    wakeUp1.callback = [this, &context](AsyncLoopWakeUp::Result& res)
    {
        context.wakeUp1ThreadID = Thread::CurrentThreadID();
        context.wakeUp1Called++;
        SC_TEST_EXPECT(not res.getAsync().isActive());
    };
    SC_TEST_EXPECT(wakeUp1.start(eventLoop));
    wakeUp2.setDebugName("wakeUp2");
    wakeUp2.callback = [this, &context](AsyncLoopWakeUp::Result& res)
    {
        context.wakeUp2Called++;
        SC_TEST_EXPECT(res.getAsync().stop());
    };
    SC_TEST_EXPECT(wakeUp2.start(eventLoop));
    Thread newThread1;
    Thread newThread2;
    Result loopRes1 = Result(false);
    Result loopRes2 = Result(false);
    auto   action1  = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test1"));
        loopRes1 = wakeUp1.wakeUp();
    };
    auto action2 = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test2"));
        loopRes2 = wakeUp1.wakeUp();
    };
    SC_TEST_EXPECT(newThread1.start(action1));
    SC_TEST_EXPECT(newThread2.start(action2));
    SC_TEST_EXPECT(newThread1.join());
    SC_TEST_EXPECT(newThread2.join());
    SC_TEST_EXPECT(loopRes1);
    SC_TEST_EXPECT(loopRes2);
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(context.wakeUp1Called == 1);
    SC_TEST_EXPECT(context.wakeUp2Called == 0);
    SC_TEST_EXPECT(context.wakeUp1ThreadID == Thread::CurrentThreadID());
}

void SC::AsyncTest::loopWakeUpEventObject()
{
    struct TestParams
    {
        int         notifier1Called         = 0;
        int         observedNotifier1Called = -1;
        EventObject eventObject;
        Result      loopRes1 = Result(false);
    } params;

    uint64_t callbackThreadID = 0;

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    AsyncLoopWakeUp wakeUp;

    wakeUp.callback = [&](AsyncLoopWakeUp::Result&)
    {
        callbackThreadID = Thread::CurrentThreadID();
        params.notifier1Called++;
    };
    SC_TEST_EXPECT(wakeUp.start(eventLoop, &params.eventObject));
    Thread newThread1;
    auto   threadLambda = [&params, &wakeUp](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test1"));
        params.loopRes1 = wakeUp.wakeUp();
        params.eventObject.wait();
        params.observedNotifier1Called = params.notifier1Called;
    };
    SC_TEST_EXPECT(newThread1.start(threadLambda));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(params.notifier1Called == 1);
    SC_TEST_EXPECT(newThread1.join());
    SC_TEST_EXPECT(params.loopRes1);
    SC_TEST_EXPECT(params.observedNotifier1Called == 1);
    SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
}

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

        ProcessDescriptor::ExitStatus exitStatus = {-1};
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
    SC_TEST_EXPECT(outParams1.exitStatus.status == 0); // Status == Ok
    SC_TEST_EXPECT(outParams2.numCallbackCalled == 1);
    SC_TEST_EXPECT(outParams2.exitStatus.status != 0); // Status == Not OK
}

void SC::AsyncTest::socketAccept()
{
    struct Context
    {
        int              acceptedCount = 0;
        SocketDescriptor acceptedClient[3];
    } context;
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    constexpr uint32_t numWaitingConnections = 2;
    SocketDescriptor   serverSocket;
    uint16_t           tcpPort = 5050;
    SocketIPAddress    nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    {
        SocketServer server(serverSocket);
        SC_TEST_EXPECT(server.bind(nativeAddress));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    AsyncSocketAccept accept;
    accept.setDebugName("Accept");
    accept.callback = [this, &context](AsyncSocketAccept::Result& res)
    {
        SC_TEST_EXPECT(res.moveTo(context.acceptedClient[context.acceptedCount]));
        context.acceptedCount++;
        SC_TEST_EXPECT(context.acceptedCount < 3);
        res.reactivateRequest(true);
    };
    SC_TEST_EXPECT(accept.start(eventLoop, serverSocket));

    SocketDescriptor client1, client2;
    SC_TEST_EXPECT(client1.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(client2.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(client1).connect("127.0.0.1", tcpPort));
    SC_TEST_EXPECT(SocketClient(client2).connect("127.0.0.1", tcpPort));
    SC_TEST_EXPECT(not context.acceptedClient[0].isValid());
    SC_TEST_EXPECT(not context.acceptedClient[1].isValid());
    SC_TEST_EXPECT(eventLoop.runOnce()); // first connect
    SC_TEST_EXPECT(eventLoop.runOnce()); // second connect
    SC_TEST_EXPECT(context.acceptedClient[0].isValid());
    SC_TEST_EXPECT(context.acceptedClient[1].isValid());
    SC_TEST_EXPECT(client1.close());
    SC_TEST_EXPECT(client2.close());
    SC_TEST_EXPECT(context.acceptedClient[0].close());
    SC_TEST_EXPECT(context.acceptedClient[1].close());

    SC_TEST_EXPECT(accept.stop());

    // on Windows stopAsync generates one more event loop run because
    // of the closing of the client socket used for acceptex, so to unify
    // the behaviors in the test we do a runNoWait
    SC_TEST_EXPECT(eventLoop.runNoWait());

    SocketDescriptor client3;
    SC_TEST_EXPECT(client3.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(client3).connect("127.0.0.1", tcpPort));

    // Now we need a runNoWait for both because there are for sure no other events to be dequeued
    SC_TEST_EXPECT(eventLoop.runNoWait());

    SC_TEST_EXPECT(not context.acceptedClient[2].isValid());
    SC_TEST_EXPECT(serverSocket.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::socketConnect()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    SocketDescriptor serverSocket;
    uint16_t         tcpPort        = 5050;
    StringView       connectAddress = "::1";
    SocketIPAddress  nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));

    {
        SocketServer server(serverSocket);
        SC_TEST_EXPECT(server.bind(nativeAddress));
        SC_TEST_EXPECT(server.listen(2)); // 2 waiting connections
    }

    struct Context
    {
        int              acceptedCount = 0;
        SocketDescriptor acceptedClient[3];
    } context;

    AsyncSocketAccept accept;
    accept.callback = [this, &context](AsyncSocketAccept::Result& res)
    {
        SC_TEST_EXPECT(res.moveTo(context.acceptedClient[context.acceptedCount]));
        context.acceptedCount++;
        res.reactivateRequest(context.acceptedCount < 2);
    };
    SC_TEST_EXPECT(accept.start(eventLoop, serverSocket));

    SocketIPAddress localHost;

    SC_TEST_EXPECT(localHost.fromAddressPort(connectAddress, tcpPort));

    AsyncSocketConnect connect[2];
    SocketDescriptor   clients[2];

    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[0]));
    int connectedCount  = 0;
    connect[0].callback = [&](AsyncSocketConnect::Result& res)
    {
        connectedCount++;
        SC_TEST_EXPECT(res.isValid());
    };
    SC_TEST_EXPECT(connect[0].start(eventLoop, clients[0], localHost));

    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[1]));
    connect[1].callback = connect[0].callback;
    SC_TEST_EXPECT(connect[1].start(eventLoop, clients[1], localHost));

    SC_TEST_EXPECT(connectedCount == 0);
    SC_TEST_EXPECT(context.acceptedCount == 0);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.acceptedCount == 2);
    SC_TEST_EXPECT(connectedCount == 2);

    char       receiveBuffer[1] = {0};
    Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

    AsyncSocketReceive receiveAsync;
    int                receiveCalls = 0;
    receiveAsync.callback           = [&](AsyncSocketReceive::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        SC_TEST_EXPECT(readData.data()[0] == 1);
        receiveCalls++;
    };
    SC_TEST_EXPECT(receiveAsync.start(eventLoop, context.acceptedClient[0], receiveData));
    char v = 1;
    SC_TEST_EXPECT(SocketClient(clients[0]).write({&v, 1}));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(receiveCalls == 1);
    SC_TEST_EXPECT(context.acceptedClient[0].close());
    SC_TEST_EXPECT(context.acceptedClient[1].close());
}

void SC::AsyncTest::socketSendReceive()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor client, serverSideClient;
    createTCPSocketPair(eventLoop, client, serverSideClient);

    const char sendBuffer[] = {123, 111};

    Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

    int             sendCount = 0;
    AsyncSocketSend sendAsync;
    sendAsync.callback = [&](AsyncSocketSend::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        sendCount++;
    };

    SC_TEST_EXPECT(sendAsync.start(eventLoop, client, sendData));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(sendCount == 1);
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(sendCount == 1);

    char       receiveBuffer[1] = {0};
    Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

    AsyncSocketReceive receiveAsync;

    struct Params
    {
        int  receiveCount                     = 0;
        char receivedData[sizeof(sendBuffer)] = {0};
        int  sizeOfSendBuffer                 = sizeof(sendBuffer); // Need this for vs2019
    };
    Params params;
    receiveAsync.callback = [this, &params](AsyncSocketReceive::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        SC_TEST_EXPECT(readData.sizeInBytes() == 1);
        params.receivedData[params.receiveCount] = readData.data()[0];
        params.receiveCount++;
        res.reactivateRequest(params.receiveCount < params.sizeOfSendBuffer);
    };
    SC_TEST_EXPECT(receiveAsync.start(eventLoop, serverSideClient, receiveData));
    SC_TEST_EXPECT(params.receiveCount == 0); // make sure we receive after run, in case of sync results
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(params.receiveCount == 2);
    SC_TEST_EXPECT(memcmp(params.receivedData, sendBuffer, sizeof(sendBuffer)) == 0);

    // Test sending large data
    constexpr size_t largeBufferSize = 1024 * 1024; // 1Mb
    Vector<char>     sendBufferLarge, receiveBufferLarge;
    SC_TEST_EXPECT(sendBufferLarge.resize(largeBufferSize));
    SC_TEST_EXPECT(receiveBufferLarge.resizeWithoutInitializing(sendBufferLarge.size()));
    sendAsync.callback = {};
    SC_TEST_EXPECT(sendAsync.start(eventLoop, client, sendBufferLarge.toSpanConst()));

    struct Context
    {
        SocketDescriptor& client;

        size_t bufferSize          = largeBufferSize; // Need this for vs2019
        int    largeCallbackCalled = 0;
        size_t totalNumBytesRead   = 0;
    } context = {client};

    receiveAsync.callback = [this, &context](AsyncSocketReceive::Result& res)
    {
        context.largeCallbackCalled++;
        if (context.totalNumBytesRead < context.bufferSize)
        {
            context.totalNumBytesRead += res.completionData.numBytes;
            if (context.totalNumBytesRead == context.bufferSize)
            {
                SC_TEST_EXPECT(context.client.close()); // Causes EOF
            }
            res.reactivateRequest(true);
        }
        else
        {
            SC_TEST_EXPECT(res.completionData.disconnected);
            SC_TEST_EXPECT(res.completionData.numBytes == 0); // EOF
        }
    };
    SC_TEST_EXPECT(receiveAsync.start(eventLoop, serverSideClient, receiveBufferLarge.toSpan()));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.largeCallbackCalled >= 1);
    SC_TEST_EXPECT(context.totalNumBytesRead == largeBufferSize);
}

void SC::AsyncTest::socketClose()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor client, serverSideClient;
    createTCPSocketPair(eventLoop, client, serverSideClient);

    AsyncSocketClose asyncClose1;

    int numCalledClose1  = 0;
    asyncClose1.callback = [&](AsyncSocketClose::Result& result)
    {
        numCalledClose1++;
        SC_TEST_EXPECT(result.isValid());
    };

    SC_TEST_EXPECT(asyncClose1.start(eventLoop, client));

    AsyncSocketClose asyncClose2;

    int numCalledClose2  = 0;
    asyncClose2.callback = [&](AsyncSocketClose::Result& result)
    {
        numCalledClose2++;
        SC_TEST_EXPECT(result.isValid());
    };
    SC_TEST_EXPECT(asyncClose2.start(eventLoop, serverSideClient));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(numCalledClose1 == 1);
    SC_TEST_EXPECT(numCalledClose2 == 1);
}

void SC::AsyncTest::socketSendReceiveError()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor client, serverSideClient;
    createTCPSocketPair(eventLoop, client, serverSideClient);

    // Setup send side on serverSideClient
    AsyncSocketSend asyncSend;
    asyncSend.setDebugName("server");
    char sendBuffer[1] = {1};

    {
        // Extract the raw handle from socket and close it
        // This will provoke the following failures:
        // - Apple: after poll on macOS (where we're pushing the async handles to OS)
        // - Windows: during Staging (precisely in Activate)
        SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
        SC_TEST_EXPECT(serverSideClient.get(handle, Result::Error("ASD")));
        SocketDescriptor socketToClose;
        SC_TEST_EXPECT(socketToClose.assign(handle));
        SC_TEST_EXPECT(socketToClose.close());
    }
    int numOnSend      = 0;
    asyncSend.callback = [&](AsyncSocketSend::Result& result)
    {
        numOnSend++;
        SC_TEST_EXPECT(not result.isValid());
    };
    SC_TEST_EXPECT(asyncSend.start(eventLoop, serverSideClient, {sendBuffer, sizeof(sendBuffer)}));

    // Setup receive side on client
    char recvBuffer[1] = {1};

    int                numOnReceive = 0;
    AsyncSocketReceive asyncRecv;
    asyncRecv.setDebugName("client");
    asyncRecv.callback = [&](AsyncSocketReceive::Result& result)
    {
        numOnReceive++;
        SC_TEST_EXPECT(not result.isValid());
    };
    SC_TEST_EXPECT(asyncRecv.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

    // This will fail because the receive async is not in Free state
    SC_TEST_EXPECT(not asyncRecv.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

    // Just close the client to cause an error in the callback
    SC_TEST_EXPECT(client.close());

    AsyncSocketReceive asyncErr;
    asyncErr.setDebugName("asyncErr");
    // This will fail immediately as the socket is already closed before this call
    SC_TEST_EXPECT(not asyncErr.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(not asyncSend.stop());
    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(numOnSend == 1);
    SC_TEST_EXPECT(numOnReceive == 1);
}

void SC::AsyncTest::fileReadWrite(bool useThreadPool)
{
    // 1. Create ThreadPool and tasks
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(4));
    }

    // 2. Create EventLoop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // 3. Create some files on disk
    StringNative<255> filePath = StringEncoding::Native;
    StringNative<255> dirPath  = StringEncoding::Native;
    const StringView  name     = "AsyncTest";
    const StringView  fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));

    // 4. Open the destination file and associate it with the event loop
    File::OpenOptions openOptions;
    openOptions.blocking = useThreadPool;

    FileDescriptor fd;
    File           file(fd);
    SC_TEST_EXPECT(file.open(filePath.view(), File::WriteCreateTruncate, openOptions));
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

    // 5. Create and start the write operation
    AsyncFileWrite       asyncWriteFile;
    AsyncFileWrite::Task asyncWriteTask;

    asyncWriteFile.setDebugName("FileWrite");
    asyncWriteFile.callback = [&](AsyncFileWrite::Result& res)
    {
        size_t writtenBytes = 0;
        SC_TEST_EXPECT(res.get(writtenBytes));
        SC_TEST_EXPECT(writtenBytes == 4);
    };
    asyncWriteFile.fileDescriptor = handle;
    asyncWriteFile.buffer         = StringView("test").toCharSpan();
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncWriteFile.setThreadPoolAndTask(threadPool, asyncWriteTask));
    }
    SC_TEST_EXPECT(asyncWriteFile.start(eventLoop));

    // 6. Run the write operation and close the file
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(fd.close());

    // 7. Open the file for read now
    SC_TEST_EXPECT(file.open(filePath.view(), File::ReadOnly, openOptions));
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }
    SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

    // 8. Create and run the read task, reading a single byte at every reactivation
    struct Params
    {
        int  readCount     = 0;
        char readBuffer[4] = {0};
    };
    Params              params;
    AsyncFileRead       asyncReadFile;
    AsyncFileRead::Task asyncReadTask;
    asyncReadFile.setDebugName("FileRead");
    asyncReadFile.callback = [this, &params](AsyncFileRead::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        if (params.readCount < 4)
        {
            SC_TEST_EXPECT(readData.sizeInBytes() == 1);
            params.readBuffer[params.readCount++] = readData.data()[0];
            res.getAsync().setOffset(res.getAsync().getOffset() + readData.sizeInBytes());
            res.reactivateRequest(true);
        }
        else
        {
            SC_TEST_EXPECT(res.completionData.endOfFile);
            SC_TEST_EXPECT(readData.empty()); // EOF
        }
    };
    char buffer[1]               = {0};
    asyncReadFile.fileDescriptor = handle;
    asyncReadFile.buffer         = {buffer, sizeof(buffer)};
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncReadFile.setThreadPoolAndTask(threadPool, asyncReadTask));
    }
    SC_TEST_EXPECT(asyncReadFile.start(eventLoop));

    // 9. Run the read operation and close the file
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(fd.close());

    // 10. Check Results
    StringView sv({params.readBuffer, sizeof(params.readBuffer)}, false, StringEncoding::Ascii);
    SC_TEST_EXPECT(sv.compare("test") == StringView::Comparison::Equals);

    // 11. Remove test files
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}

void SC::AsyncTest::fileEndOfFile(bool useThreadPool)
{
    // This tests a weird edge case where doing a single file read of the entire size of file
    // will not produce end of file flag

    // 1. Create ThreadPool and tasks
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(4));
    }

    // 2. Create EventLoop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // 3. Create some files on disk
    StringNative<255> filePath = StringEncoding::Native;
    StringNative<255> dirPath  = StringEncoding::Native;
    const StringView  name     = "AsyncTest";
    const StringView  fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    {
        char data[1024] = {0};
        SC_TEST_EXPECT(fs.write(fileName, {data, sizeof(data)}));
    }

    File::OpenOptions openOptions;
    openOptions.blocking = useThreadPool;

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    FileDescriptor         fd;
    SC_TEST_EXPECT(File(fd).open(filePath.view(), File::ReadOnly, openOptions));
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }
    SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

    struct Context
    {
        int    readCount = 0;
        size_t readSize  = 0;
    } context;
    AsyncFileRead       asyncReadFile;
    AsyncFileRead::Task asyncReadTask;
    asyncReadFile.setDebugName("FileRead");
    asyncReadFile.callback = [this, &context](AsyncFileRead::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        if (context.readCount == 0)
        {
            context.readSize += readData.sizeInBytes();
            res.reactivateRequest(true);
        }
        else if (context.readCount == 1)
        {
            context.readSize += readData.sizeInBytes();
        }
        else if (context.readCount == 2)
        {
            SC_TEST_EXPECT(res.completionData.endOfFile);
            SC_TEST_EXPECT(readData.empty()); // EOF
        }
        else
        {
            SC_TEST_EXPECT(context.readCount <= 3);
        }
        context.readCount++;
    };
    char buffer[512]             = {0};
    asyncReadFile.fileDescriptor = handle;
    asyncReadFile.buffer         = {buffer, sizeof(buffer)};
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncReadFile.setThreadPoolAndTask(threadPool, asyncReadTask));
    }
    SC_TEST_EXPECT(asyncReadFile.start(eventLoop));

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.readCount == 2);
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncReadFile.setThreadPoolAndTask(threadPool, asyncReadTask));
    }
    SC_TEST_EXPECT(asyncReadFile.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.readCount == 3);
    SC_TEST_EXPECT(fd.close());

    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}

void SC::AsyncTest::fileClose()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    StringNative<255> filePath = StringEncoding::Native;
    StringNative<255> dirPath  = StringEncoding::Native;
    const StringView  name     = "AsyncTest";
    const StringView  fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));
    SC_TEST_EXPECT(fs.write(filePath.view(), "test"));

    File::OpenOptions openOptions;
    openOptions.blocking = false;

    FileDescriptor fd;
    SC_TEST_EXPECT(File(fd).open(filePath.view(), File::WriteCreateTruncate, openOptions));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("handle")));
    AsyncFileClose asyncClose;
    asyncClose.callback = [this](auto& result) { SC_TEST_EXPECT(result.isValid()); };
    auto res            = asyncClose.start(eventLoop, handle);
    SC_TEST_EXPECT(res);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
    // file.close() will fail as the file was already closed but it also throws a Win32 exception that will
    // stop the debugger by default. Opting for a .detach()
    // SC_TEST_EXPECT(not fd.close());
    fd.detach();
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
SC_TRY(timeout.start(eventLoop, Time::Milliseconds(200)));
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
