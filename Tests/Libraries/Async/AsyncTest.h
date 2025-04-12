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

    void loopFreeSubmittingOnClose();
    void loopFreeActiveOnClose();
    void loopInterrupt();
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
