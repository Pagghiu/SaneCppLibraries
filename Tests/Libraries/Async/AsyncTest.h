// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Async/Async.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystem/Path.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h" // EventObject

namespace SC
{
struct AsyncTest;
}

struct SC::AsyncTest : public SC::TestCase
{
    AsyncEventLoop::Options options;

    AsyncTest(SC::TestReport& report);

    void createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client, SocketDescriptor& serverSideClient);

    // Loop
    void loopFreeSubmittingOnClose();
    void loopFreeActiveOnClose();
    void loopInterrupt();

    // Loop Work
    void loopWork();

    // Timeouts
    void loopTimeout();

    // Loop WakeUp
    void loopWakeUpFromExternalThread();
    void loopWakeUp();
    void loopWakeUpEventObject();

    // Processes
    void processExit();

    // Files
    void fileReadWrite(bool useThreadPool);
    void fileEndOfFile(bool useThreadPool);
    void fileWriteMultiple(bool useThreadPool);
    void fileClose();

    // TCP Sockets
    void socketTCPAccept();
    void socketTCPConnect();
    void socketTCPSendReceive();
    void socketTCPSendMultiple();
    void socketTCPClose();
    void socketTCPSendReceiveError();

    // UDP Sockets
    void socketUDPSendReceive();
};
