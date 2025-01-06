// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../File/File.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Process/Process.h"
#include "../../Socket/Socket.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"
#include "../../Threading/Threading.h" // EventObject
#include "../Async.h"

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
