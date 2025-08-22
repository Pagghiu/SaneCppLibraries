// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Async/Async.h"
#include "Libraries/Testing/Testing.h"

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
    void processInputOutput(bool useThreadPool);
    void processInputOutputChild();

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
    void socketTCPSendReceiveError();

    // UDP Sockets
    void socketUDPSendReceive();

    // File System Operations
    void fileSystemOperations();
    void fileSystemOperationOpen();
    void fileSystemOperationClose();
    void fileSystemOperationRead();
    void fileSystemOperationWrite();
    void fileSystemOperationCopy();
    void fileSystemOperationCopyDirectory();
    void fileSystemOperationRename();
    void fileSystemOperationRemoveEmptyDirectory();
    void fileSystemOperationRemoveFile();
};
