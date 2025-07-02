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
