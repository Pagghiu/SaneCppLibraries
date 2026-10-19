// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Threading/ThreadPool.h"
#include "Libraries/Time/Time.h"
#include <stdio.h>
#include <stdlib.h>

void SC::AsyncTest::loopFreeSubmittingOnClose()
{
    // This test checks that on close asyncs being submitted are being removed for submission queue and set as Free.
    AsyncLoopTimeout  loopTimeout[2];
    AsyncLoopWakeUp   loopWakeUp[2];
    AsyncSocketAccept socketAccept[2];

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    SC_TEST_EXPECT(loopTimeout[0].start(eventLoop, 12_ms));
    SC_TEST_EXPECT(loopTimeout[1].start(eventLoop, 122_ms));
    SC_TEST_EXPECT(loopWakeUp[0].start(eventLoop));
    SC_TEST_EXPECT(loopWakeUp[1].start(eventLoop));
    constexpr uint32_t numWaitingConnections = 2;
    SocketDescriptor   serverSocket[2];
    SocketIPAddress    serverAddress[2];
    const uint16_t     firstPort  = report.mapPort(5052);
    const uint16_t     secondPort = report.mapPort(5053);
    SC_TEST_EXPECT(serverAddress[0].fromAddressPort("127.0.0.1", firstPort));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[0].getAddressFamily(), serverSocket[0]));
    {
        SocketServer server(serverSocket[0]);
        SC_TEST_EXPECT(server.bind(serverAddress[0]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(serverAddress[1].fromAddressPort("127.0.0.1", secondPort));
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
    SC_TEST_EXPECT(loopTimeout[0].start(eventLoop, 12_ms));
    SC_TEST_EXPECT(loopTimeout[1].start(eventLoop, 123_ms));
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
    const uint16_t     firstPort  = report.mapPort(5052);
    const uint16_t     secondPort = report.mapPort(5053);
    SC_TEST_EXPECT(serverAddress[0].fromAddressPort("127.0.0.1", firstPort));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[0].getAddressFamily(), serverSocket[0]));
    {
        SocketServer server(serverSocket[0]);
        SC_TEST_EXPECT(server.bind(serverAddress[0]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(serverAddress[1].fromAddressPort("127.0.0.1", secondPort));
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
    // Do not forget to re-associate the sockets with newly created loop!
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(serverSocket[0]));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(serverSocket[1]));
    SC_TEST_EXPECT(asyncAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(asyncAccept[1].start(eventLoop, serverSocket[1]));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::loopInterrupt()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;

    SC_TEST_EXPECT(eventLoop.create());
    int numTimeouts  = 0;
    timeout.callback = [&](AsyncLoopTimeout::Result& res)
    {
        res.reactivateRequest(true);
        numTimeouts++;
        if (numTimeouts == 2)
        {
            eventLoop.interrupt();
        }
    };
    SC_TEST_EXPECT(timeout.start(eventLoop, 1_ms));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(numTimeouts == 2);
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() + eventLoop.getNumberOfSubmittedRequests() == 1);
}

void SC::AsyncTest::loopInterruptCollectedCompletion()
{
    Atomic<bool> finished = false;
    Thread       guard;
    SC_TEST_EXPECT(guard.start(
        [&finished](Thread&)
        {
            const auto started = Time::Monotonic::now().getMonotonicMilliseconds();
            while (not finished.load() and Time::Monotonic::now().getMonotonicMilliseconds() - started < 4000)
                Thread::Sleep(1);
            if (not finished.load())
            {
                fprintf(stderr, "Interrupted-loop completion/close exceeded its independent bound\n");
                ::_Exit(124);
            }
        }));
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create(options));
    SocketDescriptor client, peer;
    createTCPSocketPair(loop, client, peer);
    char               bytes[1] = {};
    AsyncSocketReceive request;
    int                callbacks = 0;
    request.callback             = [&](AsyncSocketReceive::Result& result)
    {
        ++callbacks;
        SC_TEST_EXPECT(result.isValid());
    };
    SC_TEST_EXPECT(SocketClient(peer).write({"X", 1}));
    SC_TEST_EXPECT(request.start(loop, client, bytes));
    uint64_t          storage[1024];
    AsyncKernelEvents events;
    events.eventsMemory = {reinterpret_cast<uint8_t*>(storage), sizeof(storage)};
    SC_TEST_EXPECT(loop.submitRequests(events));
    SC_TEST_EXPECT(loop.blockingPoll(events));
    SC_TEST_EXPECT(callbacks == 0);
    loop.interrupt();
    SC_TEST_EXPECT(loop.dispatchCompletions(events));
    SC_TEST_EXPECT(callbacks == 1 and request.isFree() and bytes[0] == 'X');
    SC_TEST_EXPECT(loop.run()); // Consume the interruption even when only excluded internal work remains.
    // Close must drain native cancellation even if an application interrupted before asking to close.
    SC_TEST_EXPECT(request.start(loop, client, bytes));
    SC_TEST_EXPECT(loop.submitRequests(events));
    loop.interrupt();
    SC_TEST_EXPECT(loop.close());
    SC_TEST_EXPECT(request.isFree() and callbacks == 1);
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(peer.close());
    finished.store(true);
    SC_TEST_EXPECT(guard.join());
}

void SC::AsyncTest::loopInterruptBeforeBlockingPoll()
{
    for (int mode = 0; mode < 2; ++mode)
    {
        const bool     fromListener = mode != 0;
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create(options));
        struct State
        {
            AsyncEventLoop& loop;
            Result          error{true};
            bool            callback = false;
            bool            expired  = false;
        } state{loop};
        AsyncLoopTimeout sentinel;
        sentinel.callback = [&state](AsyncLoopTimeout::Result&) { state.expired = true; };
        SC_TEST_EXPECT(sentinel.start(loop, {1000}));
        AsyncEventLoopListeners listeners;
        ThreadPool              missingPool;
        AsyncLoopWork           work;
        if (fromListener)
        {
            listeners.beforeBlockingPoll = [&state](AsyncEventLoop&)
            {
                state.callback = true;
                state.loop.interrupt();
            };
            loop.setListeners(&listeners);
        }
        else
        {
            work.work     = []() { return Result(true); };
            work.callback = [&state](AsyncLoopWork::Result& result)
            {
                state.error    = result.isValid();
                state.callback = true;
                state.loop.interrupt();
            };
            SC_TEST_EXPECT(work.setThreadPool(missingPool, AsyncThreadPoolMode::ForceThreadPool));
            SC_TEST_EXPECT(work.start(loop));
        }
        SC_TEST_EXPECT(loop.run());
        SC_TEST_EXPECT(state.callback);
        // The callback runs before kernel polling. Interrupt must return without waiting for unrelated timers.
        SC_TEST_EXPECT(not state.expired);
        if (not fromListener)
            SC_TEST_EXPECT(state.error.isError(ThreadingResultCategory, ThreadingError::ThreadPoolNotCreated));
        loop.setListeners(nullptr);
        if (not sentinel.isFree())
            SC_TEST_EXPECT(sentinel.unschedule(loop));
        SC_TEST_EXPECT(loop.close());
    }
}
