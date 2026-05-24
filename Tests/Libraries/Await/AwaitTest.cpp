// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Libraries/Await/Await.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"

#include <signal.h>
#include <stdlib.h>
#if SC_PLATFORM_APPLE
#include <sys/sysctl.h>
#endif
#if not SC_PLATFORM_WINDOWS
#include <unistd.h>
#endif

namespace
{
#if SC_PLATFORM_APPLE
static bool isDebuggerAttached()
{
    int               mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(::getpid())};
    struct kinfo_proc info   = {};
    size_t            size   = sizeof(info);
    if (::sysctl(mib, 4, &info, &size, nullptr, 0) != 0)
    {
        return false;
    }
    return (info.kp_proc.p_flag & P_TRACED) != 0;
}
#endif
} // namespace

namespace SC
{
struct AwaitTest;
}

#define SC_AWAIT_TEST_EVENT_LOOP(name, asyncLoop)                                                                      \
    static char    name##AllocatorMemory[512 * 1024] = {};                                                             \
    AwaitAllocator name##Allocator;                                                                                    \
    SC_TEST_EXPECT(name##Allocator.createFixed(name##AllocatorMemory));                                                \
    AwaitEventLoop name(asyncLoop, name##Allocator)

struct SC::AwaitTest : public SC::TestCase
{
    AwaitTest(SC::TestReport& report) : TestCase(report, "AwaitTest")
    {
        if (test_section("immediate task"))
        {
            immediateTask();
        }
        if (test_section("move task"))
        {
            moveTask();
        }
        if (test_section("task lifetime edge cases"))
        {
            taskLifetimeEdgeCases();
        }
        if (test_section("task cross loop guards"))
        {
            taskCrossLoopGuards();
        }
        if (test_section("move active task frames"))
        {
            moveActiveTaskFrames();
        }
        if (test_section("sleep twice"))
        {
            sleepTwice();
        }
        if (test_section("cancel sleep"))
        {
            cancelSleep();
        }
        if (test_section("cancel edge cases"))
        {
            cancelEdgeCases();
        }
        if (test_section("cancel suspended awaiters"))
        {
            cancelSuspendedAwaiters();
        }
        if (test_section("cancel before loop dispatch"))
        {
            cancelBeforeLoopDispatch();
        }
        if (test_section("cancel after completion"))
        {
            cancelAfterCompletion();
        }
        if (test_section("callback and coroutine coexist"))
        {
            callbackAndCoroutineCoexist();
        }
        if (test_section("loop wake up"))
        {
            loopWakeUp();
        }
        if (test_section("spawn wrong event loop"))
        {
            spawnWrongEventLoop();
        }
        if (test_section("socket accept"))
        {
            socketAccept();
        }
        if (test_section("socket connect"))
        {
            socketConnect();
        }
        if (test_section("socket send receive"))
        {
            socketSendReceive();
        }
        if (test_section("socket send to receive from"))
        {
            socketSendToReceiveFrom();
        }
        if (test_section("socket send all"))
        {
            socketSendAll();
        }
        if (test_section("socket receive exact"))
        {
            socketReceiveExact();
        }
        if (test_section("socket receive line"))
        {
            socketReceiveLine();
        }
        if (test_section("scatter gather operations"))
        {
            scatterGatherOperations();
        }
        if (test_section("tiny echo conversation"))
        {
            tinyEchoConversation();
        }
        if (test_section("file read write"))
        {
            fileReadWrite();
        }
        if (test_section("file send"))
        {
            fileSend();
        }
        if (test_section("file system operations"))
        {
            fileSystemOperations();
        }
        if (test_section("file system read write operations"))
        {
            fileSystemReadWriteOperations();
        }
        if (test_section("file open and socket send"))
        {
            fileOpenAndSocketSend();
        }
        if (test_section("file poll"))
        {
            filePoll();
        }
        if (test_section("unsupported awaiters fail fast"))
        {
            unsupportedAwaitersFailFast();
        }
        if (test_section("loop work"))
        {
            loopWork();
        }
        if (test_section("process exit"))
        {
            processExit();
        }
        if (test_section("cancel process exit"))
        {
            cancelProcessExit();
        }
        if (test_section("signal"))
        {
            signal();
        }
        if (test_section("cancel signal"))
        {
            cancelSignal();
        }
        if (test_section("child task"))
        {
            childTask();
        }
        if (test_section("spawn and wait child task"))
        {
            spawnAndWaitChildTask();
        }
        if (test_section("task registry"))
        {
            taskRegistry();
        }
        if (test_section("task group"))
        {
            taskGroup();
        }
        if (test_section("task group wait any"))
        {
            taskGroupWaitAny();
        }
        if (test_section("cancel child task"))
        {
            cancelChildTask();
        }
        if (test_section("wait for timeout"))
        {
            waitForTimeout();
        }
        if (test_section("cancel wait for"))
        {
            cancelWaitFor();
        }
        if (test_section("allocator"))
        {
            allocator();
        }
        if (test_section("allocator exhaustion"))
        {
            allocatorExhaustion();
        }
        if (test_section("allocator modes"))
        {
            allocatorModes();
        }
        if (test_section("child task teardown during callback"))
        {
            childTaskTeardownDuringCallback();
        }
    }

    static AwaitTask immediate(AwaitEventLoop& await)
    {
        (void)await;
        co_return Result(true);
    }

    struct FrameAddressProbe
    {
        void* beforeSuspend = nullptr;
        void* afterResume   = nullptr;
    };

    static AwaitTask captureFrameAddress(AwaitEventLoop& await, FrameAddressProbe& probe)
    {
        int frameLocal      = 0;
        probe.beforeSuspend = &frameLocal;
        SC_CO_TRY(co_await await.sleep(1_ms));
        probe.afterResume = &frameLocal;
        co_return Result(true);
    }

    static AwaitTask captureParentAndChildFrameAddress(AwaitEventLoop& await, AwaitTask& child,
                                                       FrameAddressProbe& parentProbe, FrameAddressProbe& childProbe)
    {
        int parentLocal           = 0;
        parentProbe.beforeSuspend = &parentLocal;
        child                     = captureFrameAddress(await, childProbe);
        SC_CO_TRY(await.spawn(child));
        SC_CO_TRY(co_await child);
        parentProbe.afterResume = &parentLocal;
        co_return Result(true);
    }

    static AwaitTask waitTwice(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1_ms));
        SC_CO_TRY(co_await await.sleep(1_ms));
        co_return Result(true);
    }

    static AwaitTask waitLong(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1000_ms));
        co_return Result(true);
    }

    static AwaitTask failSoon(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1_ms));
        co_return Result::Error("AwaitTest failSoon");
    }

    static AwaitTask wakeUpOnce(AwaitEventLoop& await, AwaitLoopWakeUp& wakeUp, AwaitLoopWakeUpResult& result)
    {
        SC_CO_TRY(co_await await.wakeUp(wakeUp, result));
        if (result.deliveryCount < 1)
        {
            co_return Result::Error("Await wakeUp did not deliver");
        }
        co_return Result(true);
    }

    static AwaitTask acceptOne(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                               SocketDescriptor& acceptedClient)
    {
        SC_CO_TRY(co_await await.accept(serverSocket, acceptedClient));
        co_return Result(true);
    }

    static AwaitTask connectOne(AwaitEventLoop& await, const SocketDescriptor& socket, SocketIPAddress address)
    {
        SC_CO_TRY(co_await await.connect(socket, address));
        co_return Result(true);
    }

    static AwaitTask sendReceiveOnce(AwaitEventLoop& await, const SocketDescriptor& sender,
                                     const SocketDescriptor& receiver)
    {
        const char sendBuffer[]      = {1, 2, 3};
        char       receiveBuffer[16] = {0};

        AwaitSocketSendResult sendResult;
        SC_CO_TRY(co_await await.send(sender, {sendBuffer, sizeof(sendBuffer)}, &sendResult));
        if (sendResult.numBytes != sizeof(sendBuffer))
        {
            co_return Result::Error("Await send wrote unexpected byte count");
        }

        AwaitSocketReceiveResult receiveResult;
        SC_CO_TRY(co_await await.receive(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        if (receiveResult.disconnected or receiveResult.data.sizeInBytes() != sizeof(sendBuffer) or
            receiveResult.data.data()[0] != sendBuffer[0] or receiveResult.data.data()[1] != sendBuffer[1] or
            receiveResult.data.data()[2] != sendBuffer[2])
        {
            co_return Result::Error("Await receive read unexpected data");
        }

        co_return Result(true);
    }

    static AwaitTask sendAllOnce(AwaitEventLoop& await, const SocketDescriptor& sender,
                                 const SocketDescriptor& receiver)
    {
        const char sendBuffer[]      = {1, 2, 3, 4, 5};
        char       receiveBuffer[16] = {0};

        AwaitSocketSendResult sendResult;
        SC_CO_TRY(co_await await.sendAll(sender, {sendBuffer, sizeof(sendBuffer)}, &sendResult));
        if (sendResult.numBytes != sizeof(sendBuffer))
        {
            co_return Result::Error("Await sendAll wrote unexpected byte count");
        }

        AwaitSocketReceiveResult receiveResult;
        SC_CO_TRY(co_await await.receive(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        if (receiveResult.disconnected or receiveResult.data.sizeInBytes() != sizeof(sendBuffer))
        {
            co_return Result::Error("Await sendAll receive length mismatch");
        }
        for (size_t idx = 0; idx < sizeof(sendBuffer); ++idx)
        {
            if (receiveResult.data.data()[idx] != sendBuffer[idx])
            {
                co_return Result::Error("Await sendAll receive data mismatch");
            }
        }

        co_return Result(true);
    }

    static AwaitTask sendSplitMessage(AwaitEventLoop& await, const SocketDescriptor& sender)
    {
        const char first[]  = {'h', 'e'};
        const char second[] = {'l', 'l', 'o'};

        SC_CO_TRY(co_await await.sendAll(sender, {first, sizeof(first)}));
        SC_CO_TRY(co_await await.sleep(1_ms));
        SC_CO_TRY(co_await await.sendAll(sender, {second, sizeof(second)}));

        co_return Result(true);
    }

    static AwaitTask receiveExactMessage(AwaitEventLoop& await, const SocketDescriptor& receiver,
                                         Span<char> receiveBuffer, AwaitSocketReceiveResult& receiveResult)
    {
        SC_CO_TRY(co_await await.receiveExact(receiver, receiveBuffer, &receiveResult));
        if (receiveResult.disconnected or receiveResult.data.sizeInBytes() != receiveBuffer.sizeInBytes() or
            StringView({receiveResult.data.data(), receiveResult.data.sizeInBytes()}, false, StringEncoding::Ascii) !=
                "hello")
        {
            co_return Result::Error("Await receiveExact data mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask receiveExactCancellable(AwaitEventLoop& await, const SocketDescriptor& receiver,
                                             Span<char> receiveBuffer, AwaitSocketReceiveResult& receiveResult)
    {
        SC_CO_TRY(co_await await.receiveExact(receiver, receiveBuffer, &receiveResult));
        co_return Result(true);
    }

    static AwaitTask receiveExactConversationOnce(AwaitEventLoop& await, const SocketDescriptor& sender,
                                                  const SocketDescriptor& receiver, Span<char> receiveBuffer,
                                                  AwaitSocketReceiveResult& receiveResult)
    {
        AwaitTask senderTask   = sendSplitMessage(await, sender);
        AwaitTask receiverTask = receiveExactMessage(await, receiver, receiveBuffer, receiveResult);

        AwaitTask*     storage[2] = {};
        AwaitTaskGroup group(await, storage);
        SC_CO_TRY(group.spawn(receiverTask));
        SC_CO_TRY(group.spawn(senderTask));
        SC_CO_TRY(co_await group.waitAll());

        co_return Result(true);
    }

    static AwaitTask sendSplitLine(AwaitEventLoop& await, const SocketDescriptor& sender)
    {
        const char first[]  = {'h', 'e', 'l'};
        const char second[] = {'l', 'o', '\r', '\n'};

        SC_CO_TRY(co_await await.sendAll(sender, {first, sizeof(first)}));
        SC_CO_TRY(co_await await.sleep(1_ms));
        SC_CO_TRY(co_await await.sendAll(sender, {second, sizeof(second)}));

        co_return Result(true);
    }

    static AwaitTask receiveLineMessage(AwaitEventLoop& await, const SocketDescriptor& receiver, Span<char> lineBuffer,
                                        AwaitSocketReceiveLineResult& lineResult)
    {
        SC_CO_TRY(co_await await.receiveLine(receiver, lineBuffer, lineResult));
        if (lineResult.disconnected or not lineResult.lineComplete or
            StringView({lineResult.line.data(), lineResult.line.sizeInBytes()}, false, StringEncoding::Ascii) !=
                "hello")
        {
            co_return Result::Error("Await receiveLine data mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask receiveLineCancellable(AwaitEventLoop& await, const SocketDescriptor& receiver,
                                            Span<char> lineBuffer, AwaitSocketReceiveLineResult& lineResult)
    {
        SC_CO_TRY(co_await await.receiveLine(receiver, lineBuffer, lineResult));
        co_return Result(true);
    }

    static AwaitTask receiveLineConversationOnce(AwaitEventLoop& await, const SocketDescriptor& sender,
                                                 const SocketDescriptor& receiver, Span<char> lineBuffer,
                                                 AwaitSocketReceiveLineResult& lineResult)
    {
        AwaitTask senderTask   = sendSplitLine(await, sender);
        AwaitTask receiverTask = receiveLineMessage(await, receiver, lineBuffer, lineResult);

        AwaitTask*     storage[2] = {};
        AwaitTaskGroup group(await, storage);
        SC_CO_TRY(group.spawn(receiverTask));
        SC_CO_TRY(group.spawn(senderTask));
        SC_CO_TRY(co_await group.waitAll());

        co_return Result(true);
    }

    static AwaitTask scatterGatherSocketOnce(AwaitEventLoop& await, const SocketDescriptor& sender,
                                             const SocketDescriptor& receiver)
    {
        const char       first[]   = {'A', 'w', 'a', 'i', 't'};
        const char       second[]  = {' ', 'S', 'G'};
        Span<const char> buffers[] = {{first, sizeof(first)}, {second, sizeof(second)}};

        AwaitSocketSendResult sendResult;
        Result                sendOperationResult = co_await await.sendAll(sender, buffers, &sendResult);
        if (not sendOperationResult)
        {
            co_return Result::Error("TCP scatter/gather sendAll failed");
        }
        if (sendResult.numBytes != sizeof(first) + sizeof(second))
        {
            co_return Result::Error("TCP scatter/gather send byte count mismatch");
        }

        char   receiveBuffer[16] = {};
        size_t receivedBytes     = 0;
        while (receivedBytes < sizeof(first) + sizeof(second))
        {
            Span<char> receiveStorage = {receiveBuffer, sizeof(receiveBuffer)};
            Span<char> remaining;
            if (not receiveStorage.sliceStart(receivedBytes, remaining))
            {
                co_return Result::Error("TCP scatter/gather receive slice failed");
            }

            AwaitSocketReceiveResult receiveResult;
            SC_CO_TRY(co_await await.receive(receiver, remaining, receiveResult));
            if (receiveResult.disconnected or receiveResult.data.empty())
            {
                co_return Result::Error("TCP scatter/gather receive failed");
            }
            receivedBytes += receiveResult.data.sizeInBytes();
        }
        if (receivedBytes != sizeof(first) + sizeof(second) or
            StringView({receiveBuffer, receivedBytes}, false, StringEncoding::Ascii) != "Await SG")
        {
            co_return Result::Error("TCP scatter/gather receive data mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask scatterGatherSocketSendOnly(AwaitEventLoop& await, const SocketDescriptor& sender)
    {
        const char       first[]   = {'c', 'a', 'n'};
        const char       second[]  = {'c', 'e', 'l'};
        Span<const char> buffers[] = {{first, sizeof(first)}, {second, sizeof(second)}};

        AwaitSocketSendResult sendResult;
        SC_CO_TRY(co_await await.sendAll(sender, buffers, &sendResult));

        co_return Result(true);
    }

    static AwaitTask scatterGatherDatagramOnce(AwaitEventLoop& await, const SocketDescriptor& sender,
                                               const SocketDescriptor& receiver, SocketIPAddress receiverAddress)
    {
        const char       first[]   = {'U', 'D', 'P'};
        const char       second[]  = {' ', 'S', 'G'};
        Span<const char> buffers[] = {{first, sizeof(first)}, {second, sizeof(second)}};

        AwaitSocketSendResult sendResult;
        Result sendOperationResult = co_await await.sendTo(sender, receiverAddress, buffers, &sendResult);
        if (not sendOperationResult)
        {
            co_return Result::Error("UDP scatter/gather sendTo failed");
        }
        if (sendResult.numBytes != sizeof(first) + sizeof(second))
        {
            co_return Result::Error("UDP scatter/gather send byte count mismatch");
        }

        char                         receiveBuffer[16] = {};
        AwaitSocketReceiveFromResult receiveResult;
        SC_CO_TRY(co_await await.receiveFrom(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        if (receiveResult.data.sizeInBytes() != sizeof(first) + sizeof(second) or
            StringView({receiveResult.data.data(), receiveResult.data.sizeInBytes()}, false, StringEncoding::Ascii) !=
                "UDP SG")
        {
            co_return Result::Error("UDP scatter/gather receive data mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask receiveForever(AwaitEventLoop& await, const SocketDescriptor& receiver)
    {
        char                     receiveBuffer[16] = {};
        AwaitSocketReceiveResult receiveResult;
        SC_CO_TRY(co_await await.receive(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        co_return Result(true);
    }

    static AwaitTask receiveFromCancellable(AwaitEventLoop& await, const SocketDescriptor& receiver)
    {
        char                         receiveBuffer[16] = {};
        AwaitSocketReceiveFromResult receiveResult;
        SC_CO_TRY(co_await await.receiveFrom(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        co_return Result(true);
    }

    static AwaitTask tinyEchoServer(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                                    SocketDescriptor& accepted)
    {
        char                     receiveBuffer[64] = {};
        AwaitSocketReceiveResult receiveResult;

        SC_CO_TRY(co_await await.accept(serverSocket, accepted));
        SC_CO_TRY(co_await await.receive(accepted, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        SC_CO_TRY(co_await await.sendAll(accepted, receiveResult.data));

        co_return Result(true);
    }

    static AwaitTask tinyEchoClient(AwaitEventLoop& await, const SocketDescriptor& client, SocketIPAddress address,
                                    Span<char> receiveBuffer, AwaitSocketReceiveResult& reply)
    {
        const char message[] = "niche readable await";

        SC_CO_TRY(co_await await.connect(client, address));
        SC_CO_TRY(co_await await.sendAll(client, {message, sizeof(message) - 1}));
        SC_CO_TRY(co_await await.receive(client, receiveBuffer, reply));

        co_return Result(true);
    }

    static AwaitTask tinyEchoConversationOnce(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                                              const SocketDescriptor& client, SocketIPAddress address,
                                              SocketDescriptor& accepted, Span<char> replyBuffer,
                                              AwaitSocketReceiveResult& reply)
    {
        AwaitTask server     = tinyEchoServer(await, serverSocket, accepted);
        AwaitTask clientTask = tinyEchoClient(await, client, address, replyBuffer, reply);

        AwaitTask*     storage[2] = {};
        AwaitTaskGroup group(await, storage);
        SC_CO_TRY(group.spawn(server));
        SC_CO_TRY(group.spawn(clientTask));
        SC_CO_TRY(co_await group.waitAll());

        co_return Result(true);
    }

    static AwaitTask sendToReceiveFromOnce(AwaitEventLoop& await, const SocketDescriptor& sender,
                                           const SocketDescriptor& receiver, SocketIPAddress receiverAddress)
    {
        const char sendBuffer[]      = {'P', 'I', 'N', 'G'};
        char       receiveBuffer[16] = {0};

        AwaitSocketSendResult sendResult;
        SC_CO_TRY(co_await await.sendTo(sender, receiverAddress, {sendBuffer, sizeof(sendBuffer)}, &sendResult));
        if (sendResult.numBytes != sizeof(sendBuffer))
        {
            co_return Result::Error("Await sendTo wrote unexpected byte count");
        }

        AwaitSocketReceiveFromResult receiveResult;
        SC_CO_TRY(co_await await.receiveFrom(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        if (receiveResult.disconnected or not receiveResult.sourceAddress.isValid() or
            receiveResult.data.sizeInBytes() != sizeof(sendBuffer))
        {
            co_return Result::Error("Await receiveFrom result mismatch");
        }
        for (size_t idx = 0; idx < sizeof(sendBuffer); ++idx)
        {
            if (receiveResult.data.data()[idx] != sendBuffer[idx])
            {
                co_return Result::Error("Await receiveFrom data mismatch");
            }
        }

        co_return Result(true);
    }

    static AwaitTask writeFileOnce(AwaitEventLoop& await, const FileDescriptor& file)
    {
        const char           writeBuffer[] = {'t', 'e', 's', 't'};
        AwaitFileWriteResult writeResult;
        SC_CO_TRY(co_await await.fileWrite(file, {writeBuffer, sizeof(writeBuffer)}, &writeResult));
        if (writeResult.numBytes != sizeof(writeBuffer))
        {
            co_return Result::Error("Await file write byte count mismatch");
        }
        co_return Result(true);
    }

    static AwaitTask writeFileScatterGatherOnce(AwaitEventLoop& await, const FileDescriptor& file)
    {
        const char       first[]   = {'f', 'i', 'l', 'e'};
        const char       second[]  = {'-', 's', 'g'};
        Span<const char> buffers[] = {{first, sizeof(first)}, {second, sizeof(second)}};

        AwaitFileWriteResult writeResult;
        Result               writeOperationResult = co_await await.fileWrite(file, buffers, &writeResult);
        if (not writeOperationResult)
        {
            co_return Result::Error("file scatter/gather write failed");
        }
        if (writeResult.numBytes != sizeof(first) + sizeof(second))
        {
            co_return Result::Error("Await file scatter/gather write byte count mismatch");
        }
        co_return Result(true);
    }

    static AwaitTask writeFileScatterGatherCancellable(AwaitEventLoop& await, const FileDescriptor& file)
    {
        const char       first[]   = {'c', 'a', 'n'};
        const char       second[]  = {'c', 'e', 'l'};
        Span<const char> buffers[] = {{first, sizeof(first)}, {second, sizeof(second)}};

        SC_CO_TRY(co_await await.fileWrite(file, buffers));

        co_return Result(true);
    }

    static AwaitTask readFileOnce(AwaitEventLoop& await, const FileDescriptor& file, Span<char> readBuffer,
                                  AwaitFileReadResult& readResult)
    {
        SC_CO_TRY(co_await await.fileRead(file, readBuffer, readResult));
        co_return Result(true);
    }

    static AwaitTask writeFileAtOffsetOnce(AwaitEventLoop& await, const FileDescriptor& file)
    {
        const char            writeBuffer[] = {'O', 'K'};
        AwaitFileWriteResult  writeResult;
        AwaitFileWriteOptions options;
        options.useOffset = true;
        options.offset    = 2;
        SC_CO_TRY(co_await await.fileWrite(file, {writeBuffer, sizeof(writeBuffer)}, &writeResult, options));
        if (writeResult.numBytes != sizeof(writeBuffer))
        {
            co_return Result::Error("Await file offset write byte count mismatch");
        }
        co_return Result(true);
    }

    static AwaitTask readFileAtOffsetOnce(AwaitEventLoop& await, const FileDescriptor& file, Span<char> readBuffer,
                                          AwaitFileReadResult& readResult)
    {
        AwaitFileReadOptions options;
        options.useOffset = true;
        options.offset    = 2;
        SC_CO_TRY(co_await await.fileRead(file, readBuffer, readResult, options));
        co_return Result(true);
    }

    static AwaitTask readFileUntilFullOrEOFOnce(AwaitEventLoop& await, const FileDescriptor& file,
                                                Span<char> readBuffer, AwaitFileReadResult& readResult)
    {
        SC_CO_TRY(co_await await.fileReadUntilFullOrEOF(file, readBuffer, readResult));
        co_return Result(true);
    }

    static AwaitTask fileSendOnce(AwaitEventLoop& await, const FileDescriptor& file, const SocketDescriptor& sender,
                                  const SocketDescriptor& receiver, ThreadPool& threadPool, Span<const char> expected)
    {
        char receiveBuffer[256] = {0};

        AwaitFileSendOptions sendOptions;
        sendOptions.length     = expected.sizeInBytes();
        sendOptions.threadPool = &threadPool;

        AwaitFileSendResult sendResult;
        SC_CO_TRY(co_await await.fileSend(file, sender, sendResult, sendOptions));
        if (sendResult.bytesTransferred != expected.sizeInBytes() or not sendResult.complete)
        {
            co_return Result::Error("Await fileSend transfer mismatch");
        }

        AwaitSocketReceiveResult receiveResult;
        SC_CO_TRY(co_await await.receive(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        if (receiveResult.disconnected or receiveResult.data.sizeInBytes() != expected.sizeInBytes())
        {
            co_return Result::Error("Await fileSend receive length mismatch");
        }
        for (size_t idx = 0; idx < expected.sizeInBytes(); ++idx)
        {
            if (receiveResult.data.data()[idx] != expected.data()[idx])
            {
                co_return Result::Error("Await fileSend receive data mismatch");
            }
        }

        co_return Result(true);
    }

    static AwaitTask fileSystemOperationsOnce(AwaitEventLoop& await, ThreadPool& threadPool, StringSpan sourcePath,
                                              StringSpan copyPath, StringSpan renamePath, StringSpan directoryPath,
                                              StringSpan directoryCopyPath, StringSpan directoryFilePath,
                                              StringSpan directoryCopyFilePath)
    {
        FileDescriptor openedFile;
        SC_CO_TRY(co_await await.fsOpen(threadPool, sourcePath, FileOpen::Read, openedFile));

        String text;
        SC_CO_TRY(openedFile.readUntilEOF(text));
        if (text.view() != "AwaitFileSystemOperations")
        {
            co_return Result::Error("Await fsOpen read text mismatch");
        }
        SC_CO_TRY(co_await await.fsClose(threadPool, openedFile));
        if (openedFile.isValid())
        {
            co_return Result::Error("Await fsClose left descriptor valid");
        }

        SC_CO_TRY(co_await await.fsCopyFile(threadPool, sourcePath, copyPath));
        SC_CO_TRY(co_await await.fsRename(threadPool, copyPath, renamePath));
        SC_CO_TRY(co_await await.fsCopyDirectory(threadPool, directoryPath, directoryCopyPath));
        SC_CO_TRY(co_await await.fsRemoveFile(threadPool, sourcePath));
        SC_CO_TRY(co_await await.fsRemoveFile(threadPool, renamePath));
        SC_CO_TRY(co_await await.fsRemoveFile(threadPool, directoryFilePath));
        SC_CO_TRY(co_await await.fsRemoveFile(threadPool, directoryCopyFilePath));
        SC_CO_TRY(co_await await.fsRemoveEmptyDirectory(threadPool, directoryPath));
        SC_CO_TRY(co_await await.fsRemoveEmptyDirectory(threadPool, directoryCopyPath));

        co_return Result(true);
    }

    static AwaitTask fileSystemReadWriteOnce(AwaitEventLoop& await, ThreadPool& threadPool, StringSpan path)
    {
        FileDescriptor file;
        SC_CO_TRY(co_await await.fsOpen(threadPool, path, FileOpen::ReadWrite, file));

        char                readBuffer[16] = {0};
        AwaitFileReadResult readResult;
        SC_CO_TRY(co_await await.fsRead(threadPool, file, readBuffer, readResult));
        if (readResult.data.sizeInBytes() != 6 or readResult.data.data()[0] != 'a' or
            readResult.data.data()[1] != 'b' or readResult.data.data()[2] != 'c')
        {
            co_return Result::Error("Await fsRead data mismatch");
        }

        const char           writeBuffer[] = {'X', 'Y', 'Z'};
        AwaitFileWriteResult writeResult;
        SC_CO_TRY(co_await await.fsWrite(threadPool, file, {writeBuffer, sizeof(writeBuffer)}, &writeResult, 3));
        if (writeResult.numBytes != sizeof(writeBuffer))
        {
            co_return Result::Error("Await fsWrite byte count mismatch");
        }

        SC_CO_TRY(co_await await.fsClose(threadPool, file));
        if (file.isValid())
        {
            co_return Result::Error("Await fsClose left descriptor valid");
        }

        co_return Result(true);
    }

    static AwaitTask readFileWithThreadPoolCancellable(AwaitEventLoop& await, const FileDescriptor& file,
                                                       ThreadPool& threadPool, Span<char> readBuffer,
                                                       AwaitFileReadResult& readResult)
    {
        AwaitFileReadOptions options;
        options.threadPool = &threadPool;
        SC_CO_TRY(co_await await.fileRead(file, readBuffer, readResult, options));
        co_return Result(true);
    }

    static AwaitTask fsOpenCancellable(AwaitEventLoop& await, ThreadPool& threadPool, StringSpan path,
                                       FileDescriptor& file)
    {
        SC_CO_TRY(co_await await.fsOpen(threadPool, path, FileOpen::Read, file));
        co_return Result(true);
    }

    static AwaitTask fsOpenCloseChild(AwaitEventLoop& await, ThreadPool& threadPool, StringSpan path)
    {
        FileDescriptor file;
        SC_CO_TRY(co_await await.fsOpen(threadPool, path, FileOpen::Read, file));
        SC_CO_TRY(co_await await.fsClose(threadPool, file));
        if (file.isValid())
        {
            co_return Result::Error("Await fsOpenCloseChild left descriptor valid");
        }
        co_return Result(true);
    }

    static AwaitTask spawnAndWaitFsChild(AwaitEventLoop& await, ThreadPool& threadPool, StringSpan path)
    {
        AwaitTask child = fsOpenCloseChild(await, threadPool, path);
        SC_CO_TRY(co_await await.spawnAndWait(child));
        SC_CO_TRY(child.result());
        co_return Result(true);
    }

    void startThreadPoolBlocker(ThreadPool& threadPool, ThreadPoolTask& blocker, Atomic<bool>& entered,
                                Atomic<bool>& release)
    {
        blocker.function = [&entered, &release]
        {
            entered.store(true);
            while (not release.load())
            {
                Thread::Sleep(1);
            }
        };
        SC_TEST_EXPECT(threadPool.queueTask(blocker));
        while (not entered.load())
        {
            Thread::Sleep(1);
        }
    }

    void releaseThreadPoolBlockerSoon(Thread& thread, Atomic<bool>& release)
    {
        SC_TEST_EXPECT(thread.start(
            [&release](Thread&)
            {
                Thread::Sleep(10);
                release.store(true);
            }));
    }

    static AwaitTask receiveExpectedOnce(AwaitEventLoop& await, const SocketDescriptor& receiver,
                                         Span<char> receiveBuffer, Span<const char> expected)
    {
        size_t receivedBytes = 0;
        while (receivedBytes < expected.sizeInBytes())
        {
            Span<char> remaining;
            if (not receiveBuffer.sliceStart(receivedBytes, remaining))
            {
                co_return Result::Error("Await receiveExpectedOnce slice failed");
            }

            AwaitSocketReceiveResult receiveResult;
            SC_CO_TRY(co_await await.receive(receiver, remaining, receiveResult));
            if (receiveResult.disconnected or receiveResult.data.empty())
            {
                co_return Result::Error("Await receiveExpectedOnce did not receive data");
            }
            receivedBytes += receiveResult.data.sizeInBytes();
        }

        if (receivedBytes != expected.sizeInBytes())
        {
            co_return Result::Error("Await receiveExpectedOnce length mismatch");
        }
        for (size_t idx = 0; idx < expected.sizeInBytes(); ++idx)
        {
            if (receiveBuffer.data()[idx] != expected.data()[idx])
            {
                co_return Result::Error("Await receiveExpectedOnce data mismatch");
            }
        }

        co_return Result(true);
    }

    static AwaitTask openFileAndSendToSocket(AwaitEventLoop& await, ThreadPool& threadPool, StringSpan path,
                                             const SocketDescriptor& sender, const SocketDescriptor& receiver,
                                             Span<const char> expected)
    {
        FileDescriptor file;
        SC_CO_TRY(co_await await.fsOpen(threadPool, path, FileOpen::Read, file));

        AwaitFileSendOptions sendOptions;
        sendOptions.length     = expected.sizeInBytes();
        sendOptions.threadPool = &threadPool;

        char      receiveBuffer[256] = {};
        AwaitTask receiveTask = receiveExpectedOnce(await, receiver, {receiveBuffer, sizeof(receiveBuffer)}, expected);
        SC_CO_TRY(await.spawn(receiveTask));

        AwaitFileSendResult sendResult;
        Result              sendStatus  = co_await await.fileSend(file, sender, sendResult, sendOptions);
        Result              closeStatus = file.close();
        SC_CO_TRY(sendStatus);
        SC_CO_TRY(closeStatus);
        if (sendResult.bytesTransferred != expected.sizeInBytes() or not sendResult.complete)
        {
            co_return Result::Error("Await openFileAndSendToSocket transfer mismatch");
        }
        SC_CO_TRY(co_await receiveTask);

        co_return Result(true);
    }

    static AwaitTask loopWorkOnce(AwaitEventLoop& await, ThreadPool& threadPool, Atomic<int>& workCount)
    {
        Function<Result()> work = [&workCount]
        {
            workCount.fetch_add(1);
            return Result(true);
        };
        SC_CO_TRY(co_await await.loopWork(threadPool, work));
        co_return Result(true);
    }

    static AwaitTask loopWorkCancellable(AwaitEventLoop& await, ThreadPool& threadPool, Atomic<int>& workCount)
    {
        Function<Result()> work = [&workCount]
        {
            workCount.fetch_add(1);
            return Result(true);
        };
        SC_CO_TRY(co_await await.loopWork(threadPool, work));
        co_return Result(true);
    }

    static AwaitTask filePollOnce(AwaitEventLoop& await, const FileDescriptor& file)
    {
#if SC_PLATFORM_WINDOWS
        Result pollResult = co_await await.filePoll(file);
        if (pollResult)
        {
            co_return Result::Error("Await filePoll unexpectedly succeeded on Windows");
        }
#else
        SC_CO_TRY(co_await await.filePoll(file));
#endif
        co_return Result(true);
    }

    static AwaitTask processExitOnce(AwaitEventLoop& await, Process& process, AwaitProcessExitResult& result)
    {
        SC_CO_TRY(co_await await.processExit(process.handle, result));
        co_return Result(true);
    }

    static AwaitTask signalOnce(AwaitEventLoop& await, int signalNumber, AwaitSignalResult& result)
    {
        SC_CO_TRY(co_await await.signal(signalNumber, result));
        co_return Result(true);
    }

    static AwaitTask awaitChild(AwaitEventLoop& await, AwaitTask& child)
    {
        SC_CO_TRY(await.spawn(child));
        SC_CO_TRY(co_await child);
        co_return Result(true);
    }

    static AwaitTask awaitExistingChild(AwaitEventLoop& await, AwaitTask& child)
    {
        (void)await;
        SC_CO_TRY(co_await child);
        co_return Result(true);
    }

    static AwaitTask spawnAndWaitExistingChild(AwaitEventLoop& await, AwaitTask& child)
    {
        SC_CO_TRY(co_await await.spawnAndWait(child));
        co_return Result(true);
    }

    static AwaitTask spawnAndWaitChild(AwaitEventLoop& await)
    {
        AwaitTask child = waitTwice(await);
        SC_CO_TRY(co_await await.spawnAndWait(child));
        SC_CO_TRY(child.result());
        co_return Result(true);
    }

    static AwaitTask spawnAndWaitLongChild(AwaitEventLoop& await, AwaitTask& child)
    {
        child = waitLong(await);
        SC_CO_TRY(co_await await.spawnAndWait(child));
        co_return Result(true);
    }

    static AwaitTask waitTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        SC_CO_TRY(registry.spawn(waitTwice(await)));
        SC_CO_TRY(registry.spawn(waitTwice(await)));
        SC_CO_TRY(co_await registry.waitAll());
        co_return Result(true);
    }

    static AwaitTask waitFailingTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        SC_CO_TRY(registry.spawn(waitTwice(await)));
        SC_CO_TRY(registry.spawn(failSoon(await)));
        Result result = co_await registry.waitAll();
        if (result)
        {
            co_return Result::Error("Await registry waitAll unexpectedly succeeded");
        }
        co_return Result(true);
    }

    static AwaitTask waitCancellableTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        SC_CO_TRY(registry.spawn(waitLong(await)));
        SC_CO_TRY(registry.spawn(waitLong(await)));
        SC_CO_TRY(co_await registry.waitAll());
        co_return Result(true);
    }

    static AwaitTask waitEmptyTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        (void)await;
        SC_CO_TRY(co_await registry.waitAll());
        co_return Result(true);
    }

    static AwaitTask waitAnyTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        SC_CO_TRY(registry.spawn(waitTwice(await)));
        SC_CO_TRY(registry.spawn(waitLong(await)));

        AwaitTaskRegistryWaitAnyResult waitAnyResult;
        SC_CO_TRY(co_await registry.waitAny(waitAnyResult));
        if (waitAnyResult.index != 0 or waitAnyResult.task != registry.taskAt(0) or not waitAnyResult.task->result() or
            not registry.taskAt(1)->isCompleted() or not AwaitIsCancelled(registry.taskAt(1)->result()))
        {
            co_return Result::Error("Await registry waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyFailingTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        SC_CO_TRY(registry.spawn(failSoon(await)));
        SC_CO_TRY(registry.spawn(waitLong(await)));

        AwaitTaskRegistryWaitAnyResult waitAnyResult;
        Result                         waitResult = co_await registry.waitAny(waitAnyResult);
        if (waitResult or waitAnyResult.index != 0 or waitAnyResult.task != registry.taskAt(0) or
            waitAnyResult.task->result() or not registry.taskAt(1)->isCompleted() or
            not AwaitIsCancelled(registry.taskAt(1)->result()))
        {
            co_return Result::Error("Await registry failing waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyLeaveRunningTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        SC_CO_TRY(registry.spawn(waitTwice(await)));
        SC_CO_TRY(registry.spawn(waitLong(await)));

        AwaitTaskRegistryWaitAnyResult waitAnyResult;
        SC_CO_TRY(co_await registry.waitAny(waitAnyResult, AwaitTaskRegistryWaitAnyPolicy::LeaveRemainingRunning));
        if (waitAnyResult.index != 0 or waitAnyResult.task != registry.taskAt(0) or not waitAnyResult.task->result() or
            not registry.taskAt(1)->isActive())
        {
            co_return Result::Error("Await registry leave-running waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyEmptyTaskRegistry(AwaitEventLoop& await, AwaitTaskRegistry& registry)
    {
        (void)await;
        AwaitTaskRegistryWaitAnyResult waitAnyResult;
        Result                         waitResult = co_await registry.waitAny(waitAnyResult);
        if (waitResult or waitAnyResult.task != nullptr)
        {
            co_return Result::Error("Await registry empty waitAny result mismatch");
        }
        co_return Result(true);
    }

    static AwaitTask waitForExistingChild(AwaitEventLoop& await, AwaitTask& child)
    {
        AwaitTimeoutResult timeoutResult;
        SC_CO_TRY(co_await await.waitFor(child, 100_ms, &timeoutResult));
        co_return Result(true);
    }

    static AwaitTask waitTaskGroupExistingChild(AwaitEventLoop& await, AwaitTask& child)
    {
        AwaitTask*     groupStorage[1] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(child));
        SC_CO_TRY(co_await group.waitAll());
        co_return Result(true);
    }

    static AwaitTask waitTaskGroup(AwaitEventLoop& await)
    {
        AwaitTask childA = waitTwice(await);
        AwaitTask childB = waitTwice(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(childA));
        SC_CO_TRY(group.spawn(childB));
        if (group.size() != 2)
        {
            co_return Result::Error("Await task group size mismatch");
        }
        SC_CO_TRY(co_await group.waitAll());
        SC_CO_TRY(childA.result());
        SC_CO_TRY(childB.result());

        co_return Result(true);
    }

    static AwaitTask waitTaskGroupAndCollectResults(AwaitEventLoop& await, AwaitTask& childA, AwaitTask& childB)
    {
        childA = waitTwice(await);
        childB = failSoon(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(childA));
        SC_CO_TRY(group.spawn(childB));

        Result waitResult = co_await group.waitAll();
        if (waitResult)
        {
            co_return Result::Error("Await task group waitAll unexpectedly succeeded");
        }

        Result shortResults[1] = {Result(true)};
        if (group.collectResults(shortResults))
        {
            co_return Result::Error("Await task group short collect unexpectedly succeeded");
        }

        Result                      results[2] = {Result(true), Result(true)};
        AwaitTaskGroupResultSummary summary;
        Result                      aggregate = group.collectResults(results, &summary);
        if (aggregate or not results[0] or results[1] or summary.numTasks != 2 or summary.numCompleted != 2 or
            summary.numSucceeded != 1 or summary.numFailed != 1 or summary.firstFailureIndex != 1 or
            summary.firstFailureTask != &childB or summary.firstFailure)
        {
            co_return Result::Error("Await task group collected result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitCancellableTaskGroup(AwaitEventLoop& await, AwaitTask& childA, AwaitTask& childB)
    {
        childA = waitLong(await);
        childB = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(childA));
        SC_CO_TRY(group.spawn(childB));

        Result groupResult = co_await group.waitAll();
        if (groupResult)
        {
            co_return Result::Error("Await cancellable task group unexpectedly succeeded");
        }

        co_return groupResult;
    }

    static AwaitTask waitAnyTaskGroup(AwaitEventLoop& await, AwaitTask& slowChild)
    {
        AwaitTask fastChild = waitTwice(await);
        slowChild           = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(fastChild));
        SC_CO_TRY(group.spawn(slowChild));

        AwaitTaskGroupWaitAnyResult waitAnyResult;
        SC_CO_TRY(co_await group.waitAny(waitAnyResult));
        if (waitAnyResult.index != 0 or waitAnyResult.task != &fastChild or not fastChild.result() or
            not slowChild.isCompleted() or slowChild.result())
        {
            co_return Result::Error("Await task group waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyEmptyTaskGroup(AwaitEventLoop& await)
    {
        Span<AwaitTask*>            emptyStorage;
        AwaitTaskGroup              group(await, emptyStorage);
        AwaitTaskGroupWaitAnyResult waitAnyResult;
        Result                      waitResult = co_await group.waitAny(waitAnyResult);
        if (waitResult or waitAnyResult.task != nullptr)
        {
            co_return Result::Error("Await empty task group waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyCompletedTaskGroup(AwaitEventLoop& await)
    {
        AwaitTask completed = immediate(await);
        AwaitTask slow      = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(completed));
        SC_CO_TRY(group.spawn(slow));

        AwaitTaskGroupWaitAnyResult waitAnyResult;
        SC_CO_TRY(co_await group.waitAny(waitAnyResult));
        if (waitAnyResult.index != 0 or waitAnyResult.task != &completed or not completed.result() or
            not slow.isCompleted() or slow.result())
        {
            co_return Result::Error("Await completed task group waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyFailingWinnerTaskGroup(AwaitEventLoop& await, AwaitTask& slowChild)
    {
        AwaitTask failing = failSoon(await);
        slowChild         = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(failing));
        SC_CO_TRY(group.spawn(slowChild));

        AwaitTaskGroupWaitAnyResult waitAnyResult;
        Result                      waitResult = co_await group.waitAny(waitAnyResult);
        if (waitResult or waitAnyResult.index != 0 or waitAnyResult.task != &failing or not failing.isCompleted() or
            failing.result() or not slowChild.isCompleted() or slowChild.result())
        {
            co_return Result::Error("Await failing task group waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyLeaveRemainingRunningTaskGroup(AwaitEventLoop& await, AwaitTask& slowChild)
    {
        AwaitTask fastChild = waitTwice(await);
        slowChild           = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(fastChild));
        SC_CO_TRY(group.spawn(slowChild));

        AwaitTaskGroupWaitAnyResult waitAnyResult;
        SC_CO_TRY(co_await group.waitAny(waitAnyResult, AwaitTaskGroupWaitAnyPolicy::LeaveRemainingRunning));
        if (waitAnyResult.index != 0 or waitAnyResult.task != &fastChild or not fastChild.result() or
            not slowChild.isActive())
        {
            co_return Result::Error("Await leave-running task group waitAny result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitAnyCancellableTaskGroup(AwaitEventLoop& await, AwaitTask& childA, AwaitTask& childB)
    {
        childA = waitLong(await);
        childB = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(childA));
        SC_CO_TRY(group.spawn(childB));

        AwaitTaskGroupWaitAnyResult waitAnyResult;
        Result                      waitResult = co_await group.waitAny(waitAnyResult);
        if (waitResult or waitAnyResult.task != nullptr)
        {
            co_return Result::Error("Await cancellable task group waitAny result mismatch");
        }

        co_return waitResult;
    }

    static AwaitTask waitAllLeaveChildrenRunning(AwaitEventLoop& await, AwaitTask& child)
    {
        child = waitLong(await);

        AwaitTask*     groupStorage[1] = {};
        AwaitTaskGroup group(await, groupStorage, AwaitTaskGroupCancelPolicy::LeaveChildrenRunning);
        SC_CO_TRY(group.spawn(child));

        Result groupResult = co_await group.waitAll();
        if (groupResult)
        {
            co_return Result::Error("Await leave-children-running task group unexpectedly succeeded");
        }

        co_return groupResult;
    }

    static AwaitTask waitNestedCancellableTaskGroup(AwaitEventLoop& await, AwaitTask& leafA, AwaitTask& leafB)
    {
        leafA = waitLong(await);
        leafB = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(leafA));
        SC_CO_TRY(group.spawn(leafB));

        Result groupResult = co_await group.waitAll();
        if (groupResult)
        {
            co_return Result::Error("Await nested cancellable task group unexpectedly succeeded");
        }

        co_return groupResult;
    }

    static AwaitTask waitOuterCancellableTaskGroup(AwaitEventLoop& await, AwaitTask& nested, AwaitTask& leafA,
                                                   AwaitTask& leafB, AwaitTask& sibling)
    {
        nested  = waitNestedCancellableTaskGroup(await, leafA, leafB);
        sibling = waitLong(await);

        AwaitTask*     groupStorage[2] = {};
        AwaitTaskGroup group(await, groupStorage);
        SC_CO_TRY(group.spawn(nested));
        SC_CO_TRY(group.spawn(sibling));

        Result groupResult = co_await group.waitAll();
        if (groupResult)
        {
            co_return Result::Error("Await outer cancellable task group unexpectedly succeeded");
        }

        co_return groupResult;
    }

    static AwaitTask waitForCompletedChild(AwaitEventLoop& await)
    {
        AwaitTask child = waitTwice(await);
        SC_CO_TRY(await.spawn(child));

        AwaitTimeoutResult timeoutResult;
        SC_CO_TRY(co_await await.waitFor(child, 100_ms, &timeoutResult));
        if (timeoutResult.timedOut)
        {
            co_return Result::Error("Await waitFor completed child timed out");
        }
        SC_CO_TRY(child.result());

        co_return Result(true);
    }

    static AwaitTask waitForTimedOutChild(AwaitEventLoop& await)
    {
        AwaitTask child = waitLong(await);
        SC_CO_TRY(await.spawn(child));

        AwaitTimeoutResult timeoutResult;
        Result             waitResult = co_await await.waitFor(child, 1_ms, &timeoutResult);
        if (waitResult or not timeoutResult.timedOut or not child.isCompleted() or child.result())
        {
            co_return Result::Error("Await waitFor timed-out child result mismatch");
        }

        co_return Result(true);
    }

    static AwaitTask waitForCancellableChild(AwaitEventLoop& await, AwaitTask& child, AwaitTimeoutResult& timeoutResult)
    {
        child = waitLong(await);
        SC_CO_TRY(await.spawn(child));

        Result waitResult = co_await await.waitFor(child, 10000_ms, &timeoutResult);
        if (waitResult or timeoutResult.timedOut)
        {
            co_return Result::Error("Await cancellable waitFor result mismatch");
        }

        co_return waitResult;
    }

    static AwaitTask allocatorWait(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1_ms));
        co_return Result(true);
    }

    void createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client, SocketDescriptor& serverSideClient)
    {
        SocketDescriptor serverSocket;
        const uint16_t   tcpPort        = report.mapPort(5050);
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

        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(client));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(serverSideClient));
    }

    void createTCPServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket, SocketIPAddress& nativeAddress)
    {
        const uint16_t tcpPort = report.mapPort(5051);

        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }
    }

    void immediateTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);
        SC_TEST_EXPECT(await.allocator().isOpen());

        AwaitTask task = immediate(await);
        SC_TEST_EXPECT(task.isValid());
        SC_TEST_EXPECT(not task.isStarted());
        SC_TEST_EXPECT(not task.isCompleted());

        SC_TEST_EXPECT(await.spawn(task));

        SC_TEST_EXPECT(task.isStarted());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.isActive());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
    }

    void moveTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask task  = waitTwice(await);
        AwaitTask moved = move(task);
        SC_TEST_EXPECT(moved.isValid());
        SC_TEST_EXPECT(awaitAllocator.used() < awaitAllocator.capacity());

        SC_TEST_EXPECT(not task.isValid());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(moved.isValid());

        SC_TEST_EXPECT(await.spawn(moved));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(moved.result());
        SC_TEST_EXPECT(async.close());
    }

    void taskLifetimeEdgeCases()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask invalid;
        SC_TEST_EXPECT(not invalid.isValid());
        SC_TEST_EXPECT(not invalid.result());
        SC_TEST_EXPECT(not invalid.cancel(await));
        SC_TEST_EXPECT(not await.spawn(invalid));

        AwaitTask task  = waitTwice(await);
        AwaitTask moved = move(task);
        SC_TEST_EXPECT(not task.isValid());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(not task.cancel(await));
        SC_TEST_EXPECT(not await.spawn(task));

        AwaitTask invalidChildParent = spawnAndWaitExistingChild(await, task);
        SC_TEST_EXPECT(awaitAllocator.statistics().numAllocationFailures == 0);
        SC_TEST_EXPECT(await.spawn(invalidChildParent));
        SC_TEST_EXPECT(invalidChildParent.isCompleted());
        SC_TEST_EXPECT(not invalidChildParent.result());

        SC_TEST_EXPECT(await.spawn(moved));
        AwaitTask activeMoved = move(moved);
        SC_TEST_EXPECT(not moved.isValid());
        SC_TEST_EXPECT(activeMoved.isActive());
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(activeMoved.result());

        {
            AwaitTask completed = immediate(await);
            SC_TEST_EXPECT(await.spawn(completed));
            SC_TEST_EXPECT(completed.result());
        }

        {
            char           storage[1024] = {};
            AwaitAllocator allocator;
            SC_TEST_EXPECT(allocator.createFixed(storage));
            AwaitEventLoop allocatorAwait(async, allocator);

            {
                AwaitTask allocatorTask = allocatorWait(allocatorAwait);
                SC_TEST_EXPECT(allocatorAwait.spawn(allocatorTask));
                SC_TEST_EXPECT(allocatorAwait.run());
                SC_TEST_EXPECT(allocatorTask.result());
                SC_TEST_EXPECT(allocator.used() > 0);
            }

            SC_TEST_EXPECT(allocator.used() == 0);
            SC_TEST_EXPECT(allocator.peakUsed() > 0);
            SC_TEST_EXPECT(allocator.close());
        }

        SC_TEST_EXPECT(async.close());
    }

    void taskCrossLoopGuards()
    {
        AsyncEventLoop asyncA;
        AsyncEventLoop asyncB;
        SC_TEST_EXPECT(asyncA.create());
        SC_TEST_EXPECT(asyncB.create());

        char           allocatorMemoryA[64 * 1024] = {};
        AwaitAllocator allocatorA;
        SC_TEST_EXPECT(allocatorA.createFixed(allocatorMemoryA));
        AwaitEventLoop awaitA(asyncA, allocatorA);

        char           allocatorMemoryB[64 * 1024] = {};
        AwaitAllocator allocatorB;
        SC_TEST_EXPECT(allocatorB.createFixed(allocatorMemoryB));
        AwaitEventLoop awaitB(asyncB, allocatorB);

        AwaitTask task             = waitTwice(awaitA);
        Result    wrongSpawnResult = awaitB.spawn(task);
        SC_TEST_EXPECT(not wrongSpawnResult);
        SC_TEST_EXPECT(AwaitIsWrongEventLoop(wrongSpawnResult));
        SC_TEST_EXPECT(not task.isStarted());

        AwaitTask active = waitTwice(awaitA);
        SC_TEST_EXPECT(awaitA.spawn(active));
        SC_TEST_EXPECT(active.isActive());

        AwaitTask spawnAndWaitParent = spawnAndWaitExistingChild(awaitB, active);
        SC_TEST_EXPECT(awaitB.spawn(spawnAndWaitParent));
        SC_TEST_EXPECT(spawnAndWaitParent.isCompleted());
        SC_TEST_EXPECT(AwaitIsWrongEventLoop(spawnAndWaitParent.result()));

        AwaitTask waitForParent = waitForExistingChild(awaitB, active);
        SC_TEST_EXPECT(awaitB.spawn(waitForParent));
        SC_TEST_EXPECT(waitForParent.isCompleted());
        SC_TEST_EXPECT(AwaitIsWrongEventLoop(waitForParent.result()));

        AwaitTask directAwaitParent = awaitExistingChild(awaitB, active);
        SC_TEST_EXPECT(awaitB.spawn(directAwaitParent));
        SC_TEST_EXPECT(directAwaitParent.isCompleted());
        SC_TEST_EXPECT(not directAwaitParent.result());

        AwaitTask groupParent = waitTaskGroupExistingChild(awaitB, active);
        SC_TEST_EXPECT(awaitB.spawn(groupParent));
        SC_TEST_EXPECT(groupParent.isCompleted());
        SC_TEST_EXPECT(AwaitIsWrongEventLoop(groupParent.result()));

        Result wrongCancelResult = active.cancel(awaitB);
        SC_TEST_EXPECT(not wrongCancelResult);
        SC_TEST_EXPECT(AwaitIsWrongEventLoop(wrongCancelResult));
        SC_TEST_EXPECT(active.isActive());

        SC_TEST_EXPECT(active.cancel(awaitA));
        SC_TEST_EXPECT(awaitA.run());
        SC_TEST_EXPECT(not active.result());

        SC_TEST_EXPECT(asyncA.close());
        SC_TEST_EXPECT(asyncB.close());
    }

    void moveActiveTaskFrames()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        FrameAddressProbe standaloneProbe;
        AwaitTask         standalone = captureFrameAddress(await, standaloneProbe);
        SC_TEST_EXPECT(awaitAllocator.statistics().numAllocationFailures == 0);
        SC_TEST_EXPECT(standalone.isValid());
        SC_TEST_EXPECT(await.spawn(standalone));
        SC_TEST_EXPECT(standalone.isActive());

        AwaitTask movedStandalone = move(standalone);
        SC_TEST_EXPECT(not standalone.isValid());
        SC_TEST_EXPECT(movedStandalone.isActive());

        FrameAddressProbe parentProbe;
        FrameAddressProbe childProbe;
        AwaitTask         child;
        AwaitTask         parent = captureParentAndChildFrameAddress(await, child, parentProbe, childProbe);
        SC_TEST_EXPECT(await.spawn(parent));
        SC_TEST_EXPECT(parent.isActive());
        SC_TEST_EXPECT(child.isActive());

        AwaitTask movedParent = move(parent);
        SC_TEST_EXPECT(not parent.isValid());
        SC_TEST_EXPECT(movedParent.isActive());

        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(movedStandalone.result());
        SC_TEST_EXPECT(movedParent.result());
        SC_TEST_EXPECT(child.result());
        SC_TEST_EXPECT(standaloneProbe.beforeSuspend == standaloneProbe.afterResume);
        SC_TEST_EXPECT(parentProbe.beforeSuspend == parentProbe.afterResume);
        SC_TEST_EXPECT(childProbe.beforeSuspend == childProbe.afterResume);
        SC_TEST_EXPECT(async.close());
    }

    void sleepTwice()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask task = waitTwice(await);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.isStarted());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(not task.isCompleted());

        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.isActive());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
    }

    void cancelSleep()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask task = waitLong(await);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(task.cancel(await));
        SC_TEST_EXPECT(task.isCancellationRequested());

        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.isCompleted());
        Result taskResult = task.result();
        SC_TEST_EXPECT(not taskResult);
        SC_TEST_EXPECT(AwaitIsCancelled(taskResult));
        SC_TEST_EXPECT(async.close());
    }

    void cancelEdgeCases()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask notStarted = waitTwice(await);
        SC_TEST_EXPECT(not notStarted.cancel(await));
        SC_TEST_EXPECT(not notStarted.isCancellationRequested());

        AwaitTask completed = immediate(await);
        SC_TEST_EXPECT(await.spawn(completed));
        SC_TEST_EXPECT(completed.isCompleted());
        SC_TEST_EXPECT(completed.cancel(await));

        AwaitTask active = waitLong(await);
        SC_TEST_EXPECT(await.spawn(active));
        SC_TEST_EXPECT(active.cancel(await));
        SC_TEST_EXPECT(active.cancel(await));
        SC_TEST_EXPECT(active.isCancellationRequested());

        SC_TEST_EXPECT(await.run());
        Result activeResult = active.result();
        SC_TEST_EXPECT(not activeResult);
        SC_TEST_EXPECT(AwaitIsCancelled(activeResult));
        SC_TEST_EXPECT(async.close());
    }

    void cancelSuspendedAwaiters()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitLoopWakeUp       wakeUp;
            AwaitLoopWakeUpResult result;
            AwaitTask             task = wakeUpOnce(await, wakeUp, result);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            PipeDescriptor pipe;
            PipeOptions    pipeOptions;
            pipeOptions.blocking = false;
            SC_TEST_EXPECT(pipe.createPipe(pipeOptions));
            SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(pipe.readPipe));

            AwaitTask task = filePollOnce(await, pipe.readPipe);
            SC_TEST_EXPECT(await.spawn(task));
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(task.result());
#else
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
#endif
            SC_TEST_EXPECT(pipe.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(async, client, serverSideClient);

            AwaitTask task = receiveForever(await, serverSideClient);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SocketDescriptor serverSocket;
            SocketIPAddress  nativeAddress;
            createTCPServer(async, serverSocket, nativeAddress);

            SocketDescriptor client;
            SC_TEST_EXPECT(async.createAsyncTCPSocket(nativeAddress.getAddressFamily(), client));

            AwaitTask task = connectOne(await, client, nativeAddress);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSocket.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SocketDescriptor serverSocket;
            SocketIPAddress  nativeAddress;
            createTCPServer(async, serverSocket, nativeAddress);

            SocketDescriptor acceptedClient;
            AwaitTask        task = acceptOne(await, serverSocket, acceptedClient);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(serverSocket.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(async, client, serverSideClient);

            AwaitTask task = scatterGatherSocketSendOnly(await, client);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SmallStringNative<255> filePath = StringEncoding::Native;
            SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory.view(), "await-cancel-sg.txt"}));

            FileDescriptor file;
            FileOpen       writeOpen(FileOpen::Write);
            writeOpen.blocking = false;
            SC_TEST_EXPECT(file.open(filePath.view(), writeOpen));
            SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(file));

            AwaitTask task = writeFileScatterGatherCancellable(await, file);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(file.close());

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
            (void)fs.removeFile("await-cancel-sg.txt");
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            ThreadPool threadPool;
            SC_TEST_EXPECT(threadPool.create(1));

            ThreadPoolTask blocker;
            Atomic<bool>   blockerEntered = false;
            Atomic<bool>   releaseBlocker = false;
            startThreadPoolBlocker(threadPool, blocker, blockerEntered, releaseBlocker);

            SmallStringNative<255> filePath = StringEncoding::Native;
            SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory.view(), "await-cancel-tp-file.txt"}));

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
            SC_TEST_EXPECT(fs.writeString("await-cancel-tp-file.txt", "cancel"));

            FileDescriptor file;
            FileOpen       readOpen(FileOpen::Read);
            readOpen.blocking = true;
            SC_TEST_EXPECT(file.open(filePath.view(), readOpen));

            char                readBuffer[16] = {};
            AwaitFileReadResult readResult;
            AwaitTask task = readFileWithThreadPoolCancellable(await, file, threadPool, readBuffer, readResult);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());

            Thread releaser;
            releaseThreadPoolBlockerSoon(releaser, releaseBlocker);
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(releaser.join());
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(AwaitIsCancelled(task.result()));

            SC_TEST_EXPECT(file.close());
            SC_TEST_EXPECT(threadPool.destroy());
            (void)fs.removeFile("await-cancel-tp-file.txt");
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            ThreadPool threadPool;
            SC_TEST_EXPECT(threadPool.create(1));

            ThreadPoolTask blocker;
            Atomic<bool>   blockerEntered = false;
            Atomic<bool>   releaseBlocker = false;
            startThreadPoolBlocker(threadPool, blocker, blockerEntered, releaseBlocker);

            SmallStringNative<255> filePath = StringEncoding::Native;
            SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory.view(), "await-cancel-fs-open.txt"}));

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
            SC_TEST_EXPECT(fs.writeString("await-cancel-fs-open.txt", "cancel"));

            FileDescriptor file;
            AwaitTask      task = fsOpenCancellable(await, threadPool, filePath.view(), file);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());

            Thread releaser;
            releaseThreadPoolBlockerSoon(releaser, releaseBlocker);
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(releaser.join());
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(AwaitIsCancelled(task.result()));
            SC_TEST_EXPECT(not file.isValid());

            SC_TEST_EXPECT(threadPool.destroy());
            (void)fs.removeFile("await-cancel-fs-open.txt");
            SC_TEST_EXPECT(async.close());
        }
    }

    void cancelBeforeLoopDispatch()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(async, client, serverSideClient);

            char                     receiveBuffer[5] = {};
            AwaitSocketReceiveResult receiveResult;
            AwaitTask task = receiveExactCancellable(await, serverSideClient, receiveBuffer, receiveResult);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(AwaitIsCancelled(task.result()));
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(async, client, serverSideClient);

            char                         lineBuffer[8] = {};
            AwaitSocketReceiveLineResult lineResult;
            AwaitTask                    task = receiveLineCancellable(await, serverSideClient, lineBuffer, lineResult);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(AwaitIsCancelled(task.result()));
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            const uint16_t port = report.mapPort(5054);

            SocketIPAddress bindAddress;
            SC_TEST_EXPECT(bindAddress.fromAddressPort("0.0.0.0", port));

            SocketDescriptor receiver;
            SC_TEST_EXPECT(async.createAsyncUDPSocket(bindAddress.getAddressFamily(), receiver));
            SC_TEST_EXPECT(SocketServer(receiver).bind(bindAddress));

            AwaitTask task = receiveFromCancellable(await, receiver);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(AwaitIsCancelled(task.result()));
            SC_TEST_EXPECT(receiver.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            ThreadPool threadPool;
            SC_TEST_EXPECT(threadPool.create(1));

            ThreadPoolTask blocker;
            Atomic<bool>   blockerEntered = false;
            Atomic<bool>   releaseBlocker = false;
            startThreadPoolBlocker(threadPool, blocker, blockerEntered, releaseBlocker);

            Atomic<int> workCount = 0;
            AwaitTask   task      = loopWorkCancellable(await, threadPool, workCount);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());

            Thread releaser;
            releaseThreadPoolBlockerSoon(releaser, releaseBlocker);
            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(releaser.join());
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(AwaitIsCancelled(task.result()));
            SC_TEST_EXPECT(threadPool.destroy());
            SC_TEST_EXPECT(async.close());
        }
    }

    void cancelAfterCompletion()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitLoopWakeUp       wakeUp;
            AwaitLoopWakeUpResult wakeUpResult;
            AwaitTask             task = wakeUpOnce(await, wakeUp, wakeUpResult);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(wakeUp.wakeUp(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(task.result());

            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(not task.isCancellationRequested());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask task = waitTwice(await);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(task.result());

            SC_TEST_EXPECT(task.cancel(await));
            SC_TEST_EXPECT(not task.isCancellationRequested());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(async.close());
        }
    }

    void callbackAndCoroutineCoexist()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        int              callbackCount = 0;
        AsyncLoopTimeout callbackTimeout;
        callbackTimeout.callback = [&](AsyncLoopTimeout::Result& result)
        {
            SC_TEST_EXPECT(result.isValid());
            callbackCount++;
        };

        AwaitTask task = waitTwice(await);
        SC_TEST_EXPECT(callbackTimeout.start(async, 1_ms));
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(callbackCount == 1);
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
    }

    void loopWakeUp()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitLoopWakeUp       wakeUp;
        AwaitLoopWakeUpResult result;
        AwaitTask             task = wakeUpOnce(await, wakeUp, result);
        SC_TEST_EXPECT(await.spawn(task));

        Thread senderThread;
        struct ThreadContext
        {
            AwaitEventLoop*  await;
            AwaitLoopWakeUp* wakeUp;
            Result           wakeUpResult = Result(false);
        } threadContext = {&await, &wakeUp};

        auto sender = [&threadContext](Thread& thread)
        {
            thread.setThreadName(SC_NATIVE_STR("await-wakeup"));
            threadContext.wakeUpResult = threadContext.wakeUp->wakeUp(*threadContext.await);
        };

        SC_TEST_EXPECT(senderThread.start(sender));
        SC_TEST_EXPECT(senderThread.join());
        SC_TEST_EXPECT(threadContext.wakeUpResult);
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(result.deliveryCount == 1);
        SC_TEST_EXPECT(async.close());
    }

    void spawnWrongEventLoop()
    {
        AsyncEventLoop asyncA;
        AsyncEventLoop asyncB;
        SC_TEST_EXPECT(asyncA.create());
        SC_TEST_EXPECT(asyncB.create());

        char           allocatorMemoryA[64 * 1024] = {};
        AwaitAllocator allocatorA;
        SC_TEST_EXPECT(allocatorA.createFixed(allocatorMemoryA));
        AwaitEventLoop awaitA(asyncA, allocatorA);

        char           allocatorMemoryB[64 * 1024] = {};
        AwaitAllocator allocatorB;
        SC_TEST_EXPECT(allocatorB.createFixed(allocatorMemoryB));
        AwaitEventLoop awaitB(asyncB, allocatorB);

        AwaitTask task = waitTwice(awaitA);
        SC_TEST_EXPECT(not awaitB.spawn(task));
        SC_TEST_EXPECT(not task.isStarted());

        SC_TEST_EXPECT(asyncA.close());
        SC_TEST_EXPECT(asyncB.close());
    }

    void socketAccept()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SocketDescriptor serverSocket;
        SocketIPAddress  nativeAddress;
        createTCPServer(async, serverSocket, nativeAddress);

        SocketDescriptor acceptedClient;
        AwaitTask        task = acceptOne(await, serverSocket, acceptedClient);
        SC_TEST_EXPECT(await.spawn(task));

        SocketDescriptor client;
        SC_TEST_EXPECT(client.create(nativeAddress.getAddressFamily()));
        SC_TEST_EXPECT(SocketClient(client).connect("127.0.0.1", nativeAddress.getPort()));

        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(acceptedClient.isValid());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(async.close());
    }

    void socketConnect()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SocketDescriptor serverSocket;
        SocketIPAddress  nativeAddress;
        createTCPServer(async, serverSocket, nativeAddress);

        SocketDescriptor acceptedClient;
        AwaitTask        acceptTask = acceptOne(await, serverSocket, acceptedClient);
        SC_TEST_EXPECT(await.spawn(acceptTask));

        SocketDescriptor client;
        SC_TEST_EXPECT(async.createAsyncTCPSocket(nativeAddress.getAddressFamily(), client));

        AwaitTask connectTask = connectOne(await, client, nativeAddress);
        SC_TEST_EXPECT(await.spawn(connectTask));

        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(acceptTask.result());
        SC_TEST_EXPECT(connectTask.result());
        SC_TEST_EXPECT(acceptedClient.isValid());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(async.close());
    }

    void socketSendReceive()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(async, client, serverSideClient);

        AwaitTask task = sendReceiveOnce(await, client, serverSideClient);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(async.close());
    }

    void socketSendToReceiveFrom()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        const uint16_t port = report.mapPort(5052);

        SocketIPAddress bindAddress;
        SC_TEST_EXPECT(bindAddress.fromAddressPort("0.0.0.0", port));

        SocketIPAddress receiverAddress;
        SC_TEST_EXPECT(receiverAddress.fromAddressPort("127.0.0.1", port));

        SocketDescriptor receiver;
        SocketDescriptor sender;
        SC_TEST_EXPECT(async.createAsyncUDPSocket(bindAddress.getAddressFamily(), receiver));
        SC_TEST_EXPECT(async.createAsyncUDPSocket(receiverAddress.getAddressFamily(), sender));
        SC_TEST_EXPECT(SocketServer(receiver).bind(bindAddress));

        AwaitTask task = sendToReceiveFromOnce(await, sender, receiver, receiverAddress);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(receiver.close());
        SC_TEST_EXPECT(sender.close());
        SC_TEST_EXPECT(async.close());
    }

    void socketSendAll()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(async, client, serverSideClient);

        AwaitTask task = sendAllOnce(await, client, serverSideClient);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(async.close());
    }

    void socketReceiveExact()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(async, client, serverSideClient);

        char                     receiveBuffer[5] = {};
        AwaitSocketReceiveResult receiveResult;
        AwaitTask task = receiveExactConversationOnce(await, client, serverSideClient, receiveBuffer, receiveResult);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(async.close());
    }

    void socketReceiveLine()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(async, client, serverSideClient);

        char                         lineBuffer[8] = {};
        AwaitSocketReceiveLineResult lineResult;
        AwaitTask task = receiveLineConversationOnce(await, client, serverSideClient, lineBuffer, lineResult);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(async.close());
    }

    void scatterGatherOperations()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(async, client, serverSideClient);

            AwaitTask task = scatterGatherSocketOnce(await, client, serverSideClient);
            SC_TEST_EXPECT(awaitAllocator.statistics().numAllocationFailures == 0);
            SC_TEST_EXPECT(task.isValid());
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());

            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            const uint16_t port = report.mapPort(5053);

            SocketIPAddress bindAddress;
            SC_TEST_EXPECT(bindAddress.fromAddressPort("0.0.0.0", port));

            SocketIPAddress receiverAddress;
            SC_TEST_EXPECT(receiverAddress.fromAddressPort("127.0.0.1", port));

            SocketDescriptor receiver;
            SocketDescriptor sender;
            SC_TEST_EXPECT(async.createAsyncUDPSocket(bindAddress.getAddressFamily(), receiver));
            SC_TEST_EXPECT(async.createAsyncUDPSocket(receiverAddress.getAddressFamily(), sender));
            SC_TEST_EXPECT(SocketServer(receiver).bind(bindAddress));

            AwaitTask task = scatterGatherDatagramOnce(await, sender, receiver, receiverAddress);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());

            SC_TEST_EXPECT(receiver.close());
            SC_TEST_EXPECT(sender.close());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            SmallStringNative<255> filePath = StringEncoding::Native;
            const StringView       fileName = "await-file-sg.txt";
            SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory.view(), fileName}));

            FileDescriptor file;
            FileOpen       writeOpen(FileOpen::Write);
            writeOpen.blocking = false;
            SC_TEST_EXPECT(file.open(filePath.view(), writeOpen));
            SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(file));

            AwaitTask task = writeFileScatterGatherOnce(await, file);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(file.close());

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
            String text;
            SC_TEST_EXPECT(fs.read(fileName, text));
            SC_TEST_EXPECT(text.view() == "file-sg");
            SC_TEST_EXPECT(fs.removeFile(fileName));
            SC_TEST_EXPECT(async.close());
        }
    }

    void tinyEchoConversation()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SocketDescriptor serverSocket;
        SocketIPAddress  nativeAddress;
        createTCPServer(async, serverSocket, nativeAddress);

        SocketDescriptor client;
        SC_TEST_EXPECT(async.createAsyncTCPSocket(nativeAddress.getAddressFamily(), client));

        SocketDescriptor         acceptedClient;
        char                     replyBuffer[64] = {};
        AwaitSocketReceiveResult reply;
        AwaitTask                task =
            tinyEchoConversationOnce(await, serverSocket, client, nativeAddress, acceptedClient, replyBuffer, reply);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.result());
        const StringView expected = "niche readable await";
        SC_TEST_EXPECT(reply.data.sizeInBytes() == expected.sizeInBytes());
        SC_TEST_EXPECT(StringView({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii) ==
                       expected);

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(async.close());
    }

    void fileReadWrite()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        SmallStringNative<255> filePath = StringEncoding::Native;
        SmallStringNative<255> dirPath  = StringEncoding::Native;
        const StringView       name     = "AwaitTest";
        const StringView       fileName = "file-read-write.txt";
        SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), name}));
        SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));

        FileDescriptor file;
        FileOpen       writeOpen(FileOpen::Write);
        writeOpen.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), writeOpen));
        SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(file));

        AwaitTask writeTask = writeFileOnce(await, file);
        SC_TEST_EXPECT(await.spawn(writeTask));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(writeTask.result());
        SC_TEST_EXPECT(file.close());

        FileOpen readOpen(FileOpen::Read);
        readOpen.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), readOpen));
        SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(file));

        char                readBuffer[16] = {0};
        AwaitFileReadResult readResult;
        AwaitTask           readTask = readFileOnce(await, file, {readBuffer, sizeof(readBuffer)}, readResult);
        SC_TEST_EXPECT(await.spawn(readTask));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(readTask.result());
        SC_TEST_EXPECT(readResult.data.sizeInBytes() == 4);
        SC_TEST_EXPECT(readResult.data.data()[0] == 't');
        SC_TEST_EXPECT(readResult.data.data()[1] == 'e');
        SC_TEST_EXPECT(readResult.data.data()[2] == 's');
        SC_TEST_EXPECT(readResult.data.data()[3] == 't');
        SC_TEST_EXPECT(file.close());

        SC_TEST_EXPECT(file.open(filePath.view(), readOpen));
        SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(file));

        char                readUntilEOFBuffer[8] = {};
        AwaitFileReadResult readUntilEOFResult;
        AwaitTask readUntilEOFTask = readFileUntilFullOrEOFOnce(await, file, readUntilEOFBuffer, readUntilEOFResult);
        SC_TEST_EXPECT(await.spawn(readUntilEOFTask));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(readUntilEOFTask.result());
        SC_TEST_EXPECT(readUntilEOFResult.endOfFile);
        SC_TEST_EXPECT(readUntilEOFResult.data.sizeInBytes() == 4);
        SC_TEST_EXPECT(readUntilEOFResult.data.data()[0] == 't');
        SC_TEST_EXPECT(readUntilEOFResult.data.data()[1] == 'e');
        SC_TEST_EXPECT(readUntilEOFResult.data.data()[2] == 's');
        SC_TEST_EXPECT(readUntilEOFResult.data.data()[3] == 't');
        SC_TEST_EXPECT(file.close());

        FileOpen readWriteOpen(FileOpen::ReadWrite);
        readWriteOpen.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), readWriteOpen));
        SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(file));

        AwaitTask offsetWriteTask = writeFileAtOffsetOnce(await, file);
        SC_TEST_EXPECT(await.spawn(offsetWriteTask));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(offsetWriteTask.result());
        SC_TEST_EXPECT(file.close());

        SC_TEST_EXPECT(file.open(filePath.view(), readOpen));
        SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(file));

        char                offsetReadBuffer[4] = {};
        AwaitFileReadResult offsetReadResult;
        AwaitTask           offsetReadTask = readFileAtOffsetOnce(await, file, {offsetReadBuffer, 2}, offsetReadResult);
        SC_TEST_EXPECT(await.spawn(offsetReadTask));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(offsetReadTask.result());
        SC_TEST_EXPECT(offsetReadResult.data.sizeInBytes() == 2);
        SC_TEST_EXPECT(offsetReadResult.data.data()[0] == 'O');
        SC_TEST_EXPECT(offsetReadResult.data.data()[1] == 'K');
        SC_TEST_EXPECT(file.close());

        SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
        SC_TEST_EXPECT(fs.removeFile(fileName));
        SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
        SC_TEST_EXPECT(async.close());
    }

    void fileSend()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        ThreadPool threadPool;
        SC_TEST_EXPECT(threadPool.create(2));

        SmallStringNative<255> filePath = StringEncoding::Native;
        SmallStringNative<255> dirPath  = StringEncoding::Native;
        const StringView       name     = "AwaitTest";
        const StringView       fileName = "file-send.txt";
        SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), name}));
        SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));
        SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));

        const char fileContent[] = "Await file send";
        SC_TEST_EXPECT(fs.write(fileName, {fileContent, sizeof(fileContent) - 1}));

        SocketDescriptor receiver;
        SocketDescriptor sender;
        createTCPSocketPair(async, receiver, sender);

        FileDescriptor file;
        FileOpen       readOpen(FileOpen::Read);
        readOpen.blocking = true;
        SC_TEST_EXPECT(file.open(filePath.view(), readOpen));

        AwaitTask task =
            fileSendOnce(await, file, sender, receiver, threadPool, {fileContent, sizeof(fileContent) - 1});
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(receiver.close());
        SC_TEST_EXPECT(sender.close());
        SC_TEST_EXPECT(async.close());
        SC_TEST_EXPECT(threadPool.destroy());

        SC_TEST_EXPECT(fs.removeFile(fileName));
        SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
    }

    void fileSystemOperations()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        ThreadPool threadPool;
        SC_TEST_EXPECT(threadPool.create(1));

        SmallStringNative<255> sourcePath      = StringEncoding::Native;
        SmallStringNative<255> copyPath        = StringEncoding::Native;
        SmallStringNative<255> renamePath      = StringEncoding::Native;
        SmallStringNative<255> dirPath         = StringEncoding::Native;
        SmallStringNative<255> dirCopyPath     = StringEncoding::Native;
        SmallStringNative<255> dirFilePath     = StringEncoding::Native;
        SmallStringNative<255> dirCopyFilePath = StringEncoding::Native;
        const StringView       sourceName      = "await-fs-source.txt";
        const StringView       copyName        = "await-fs-copy.txt";
        const StringView       renameName      = "await-fs-renamed.txt";
        const StringView       dirName         = "await-fs-dir";
        const StringView       dirCopyName     = "await-fs-dir-copy";
        const StringView       dirFileName     = "await-fs-dir/file.txt";
        const StringView       dirCopyFileName = "await-fs-dir-copy/file.txt";

        SC_TEST_EXPECT(Path::join(sourcePath, {report.applicationRootDirectory.view(), sourceName}));
        SC_TEST_EXPECT(Path::join(copyPath, {report.applicationRootDirectory.view(), copyName}));
        SC_TEST_EXPECT(Path::join(renamePath, {report.applicationRootDirectory.view(), renameName}));
        SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), dirName}));
        SC_TEST_EXPECT(Path::join(dirCopyPath, {report.applicationRootDirectory.view(), dirCopyName}));
        SC_TEST_EXPECT(Path::join(dirFilePath, {report.applicationRootDirectory.view(), dirFileName}));
        SC_TEST_EXPECT(Path::join(dirCopyFilePath, {report.applicationRootDirectory.view(), dirCopyFileName}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.writeString(sourceName, "AwaitFileSystemOperations"));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(dirName));
        SC_TEST_EXPECT(fs.writeString(dirFileName, "AwaitDirectoryFile"));

        AwaitTask task =
            fileSystemOperationsOnce(await, threadPool, sourcePath.view(), copyPath.view(), renamePath.view(),
                                     dirPath.view(), dirCopyPath.view(), dirFilePath.view(), dirCopyFilePath.view());
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(not fs.existsAndIsFile(sourceName));
        SC_TEST_EXPECT(not fs.existsAndIsFile(copyName));
        SC_TEST_EXPECT(not fs.existsAndIsFile(renameName));
        SC_TEST_EXPECT(not fs.existsAndIsDirectory(dirName));
        SC_TEST_EXPECT(not fs.existsAndIsDirectory(dirCopyName));
        SC_TEST_EXPECT(async.close());
        SC_TEST_EXPECT(threadPool.destroy());
    }

    void fileSystemReadWriteOperations()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        ThreadPool threadPool;
        SC_TEST_EXPECT(threadPool.create(1));

        SmallStringNative<255> filePath = StringEncoding::Native;
        const StringView       fileName = "await-fs-read-write.txt";
        SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory.view(), fileName}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.writeString(fileName, "abcdef"));

        AwaitTask task = fileSystemReadWriteOnce(await, threadPool, filePath.view());
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.result());

        String text;
        SC_TEST_EXPECT(fs.read(fileName, text));
        SC_TEST_EXPECT(text.view() == "abcXYZ");

        SC_TEST_EXPECT(async.close());
        SC_TEST_EXPECT(threadPool.destroy());
        SC_TEST_EXPECT(fs.removeFile(fileName));
    }

    void fileOpenAndSocketSend()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        ThreadPool threadPool;
        SC_TEST_EXPECT(threadPool.create(2));

        SmallStringNative<255> filePath = StringEncoding::Native;
        const StringView       fileName = "await-open-send.txt";
        SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory.view(), fileName}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

        const char fileContent[] = "Await open then send";
        SC_TEST_EXPECT(fs.write(fileName, {fileContent, sizeof(fileContent) - 1}));

        SocketDescriptor receiver;
        SocketDescriptor sender;
        createTCPSocketPair(async, receiver, sender);

        AwaitTask task = openFileAndSendToSocket(await, threadPool, filePath.view(), sender, receiver,
                                                 {fileContent, sizeof(fileContent) - 1});
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(receiver.close());
        SC_TEST_EXPECT(sender.close());
        SC_TEST_EXPECT(async.close());
        SC_TEST_EXPECT(threadPool.destroy());
        SC_TEST_EXPECT(fs.removeFile(fileName));
    }

    void filePoll()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        PipeDescriptor pipe;
        PipeOptions    pipeOptions;
        pipeOptions.blocking = false;
        SC_TEST_EXPECT(pipe.createPipe(pipeOptions));
        SC_TEST_EXPECT(async.associateExternallyCreatedFileDescriptor(pipe.readPipe));

        AwaitTask task = filePollOnce(await, pipe.readPipe);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(pipe.writePipe.writeString("x"));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(pipe.close());
        SC_TEST_EXPECT(async.close());
    }

    void unsupportedAwaitersFailFast()
    {
#if SC_PLATFORM_WINDOWS
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        FileDescriptor invalidFile;
        AwaitTask      task = filePollOnce(await, invalidFile);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
#else
        SC_TEST_EXPECT(true);
#endif
    }

    void loopWork()
    {
        ThreadPool threadPool;
        SC_TEST_EXPECT(threadPool.create(2));

        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        Atomic<int> workCount = 0;
        AwaitTask   task      = loopWorkOnce(await, threadPool, workCount);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(workCount.load() == 1);
        SC_TEST_EXPECT(async.close());
        SC_TEST_EXPECT(threadPool.destroy());
    }

    void processExit()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        Process processSuccess;
        Process processFailure;
#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(processSuccess.launch({"where", "where.exe"}));
        SC_TEST_EXPECT(processFailure.launch({"cmd", "/C", "dir /DOCTORS"}));
#else
        SC_TEST_EXPECT(processSuccess.launch({"sleep", "0.2"}));
        SC_TEST_EXPECT(processFailure.launch({"ls", "/~"}));
#endif

        AwaitProcessExitResult successResult;
        AwaitProcessExitResult failureResult;
        AwaitTask              successTask = processExitOnce(await, processSuccess, successResult);
        AwaitTask              failureTask = processExitOnce(await, processFailure, failureResult);
        SC_TEST_EXPECT(await.spawn(successTask));
        SC_TEST_EXPECT(await.spawn(failureTask));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(successTask.result());
        SC_TEST_EXPECT(successResult.exitStatus == 0);
        SC_TEST_EXPECT(failureTask.result());
        SC_TEST_EXPECT(failureResult.exitStatus != 0);
        SC_TEST_EXPECT(async.close());
    }

    void cancelProcessExit()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(async.close());
#else
        Process process;
        SC_TEST_EXPECT(process.launch({"sleep", "0.3"}));

        AwaitProcessExitResult result;
        AwaitTask              task = processExitOnce(await, process, result);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(task.cancel(await));

        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(async.close());
        SC_TEST_EXPECT(process.waitForExitSync());
        SC_TEST_EXPECT(process.getExitStatus() == 0);
#endif
    }

    void signal()
    {
#if SC_PLATFORM_APPLE
        if (isDebuggerAttached())
        {
            report.console.printLine("AwaitTest - Skipping signal section while debugger is attached");
            return;
        }
#endif

        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

#if SC_PLATFORM_WINDOWS
        AwaitSignalResult result;
        AwaitTask         task = signalOnce(await, 2, result);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.cancel(await));
        SC_TEST_EXPECT(await.runNoWait());
        SC_TEST_EXPECT(async.close());
#else
        AwaitSignalResult result;
        AwaitTask         task = signalOnce(await, SIGINT, result);
        SC_TEST_EXPECT(await.spawn(task));

        AsyncLoopTimeout sendSignal;
        sendSignal.callback = [this](AsyncLoopTimeout::Result&) { SC_TEST_EXPECT(::kill(::getpid(), SIGINT) == 0); };
        SC_TEST_EXPECT(sendSignal.start(async, 10_ms));

        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(result.signalNumber == SIGINT);
        SC_TEST_EXPECT(result.deliveryCount >= 1);
        SC_TEST_EXPECT(async.close());
#endif
    }

    void cancelSignal()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitSignalResult result;
        AwaitTask         task = signalOnce(await, SIGINT, result);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(task.cancel(await));

        SC_TEST_EXPECT(await.runNoWait());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(async.close());
    }

    void childTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask child  = waitTwice(await);
        AwaitTask parent = awaitChild(await, child);

        SC_TEST_EXPECT(await.spawn(parent));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(child.result());
        SC_TEST_EXPECT(parent.result());
        SC_TEST_EXPECT(async.close());
    }

    void spawnAndWaitChildTask()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask parent = spawnAndWaitChild(await);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask child;
            AwaitTask parent = spawnAndWaitLongChild(await, child);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isActive());
            SC_TEST_EXPECT(child.isActive());
            SC_TEST_EXPECT(parent.cancel(await));
            SC_TEST_EXPECT(parent.cancel(await));
            SC_TEST_EXPECT(child.cancel(await));

            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(child.isCompleted());
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(not child.result());
            SC_TEST_EXPECT(not parent.result());
            SC_TEST_EXPECT(async.close());
        }
    }

    void taskRegistry()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);

            AwaitTaskRegistrySpawnResult first;
            SC_TEST_EXPECT(registry.spawn(waitTwice(await), &first));
            SC_TEST_EXPECT(first.index == 0);
            SC_TEST_EXPECT(first.task == &storage[0]);
            SC_TEST_EXPECT(registry.size() == 1);
            SC_TEST_EXPECT(registry.activeCount() == 1);
            SC_TEST_EXPECT(registry.completedCount() == 0);
            SC_TEST_EXPECT(registry.capacity() == 2);

            AwaitTaskRegistrySpawnResult second;
            SC_TEST_EXPECT(registry.spawn(failSoon(await), &second));
            SC_TEST_EXPECT(second.index == 1);
            SC_TEST_EXPECT(second.task == &storage[1]);

            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(registry.size() == 2);
            SC_TEST_EXPECT(registry.activeCount() == 0);
            SC_TEST_EXPECT(registry.completedCount() == 2);

            AwaitTaskGroupResultSummary summary;
            SC_TEST_EXPECT(registry.clearCompleted(&summary) == 2);
            SC_TEST_EXPECT(summary.numTasks == 2);
            SC_TEST_EXPECT(summary.numCompleted == 2);
            SC_TEST_EXPECT(summary.numSucceeded == 1);
            SC_TEST_EXPECT(summary.numFailed == 1);
            SC_TEST_EXPECT(summary.firstFailureIndex == 1);
            SC_TEST_EXPECT(not summary.firstFailure);
            SC_TEST_EXPECT(registry.size() == 0);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitEmptyTaskRegistry(await, registry);

            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(registry.cancelAll());
            SC_TEST_EXPECT(registry.clearCompleted() == 0);
            SC_TEST_EXPECT(registry.size() == 0);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitTaskRegistry(await, registry);
            SC_TEST_EXPECT(parent.isValid());
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(registry.completedCount() == 2);
            SC_TEST_EXPECT(registry.clearCompleted() == 2);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitFailingTaskRegistry(await, registry);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(registry.completedCount() == 2);
            SC_TEST_EXPECT(registry.clearCompleted() == 2);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitCancellableTaskRegistry(await, registry);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isActive());
            SC_TEST_EXPECT(registry.activeCount() == 2);
            SC_TEST_EXPECT(parent.cancel(await));

            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(not parent.result());
            SC_TEST_EXPECT(registry.completedCount() == 2);
            SC_TEST_EXPECT(AwaitIsCancelled(storage[0].result()));
            SC_TEST_EXPECT(AwaitIsCancelled(storage[1].result()));
            SC_TEST_EXPECT(registry.clearCompleted() == 2);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitAnyTaskRegistry(await, registry);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(registry.completedCount() == 2);
            SC_TEST_EXPECT(registry.clearCompleted() == 2);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitAnyFailingTaskRegistry(await, registry);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(registry.completedCount() == 2);
            SC_TEST_EXPECT(registry.clearCompleted() == 2);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitAnyLeaveRunningTaskRegistry(await, registry);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(await.runOnce());
            SC_TEST_EXPECT(await.runOnce());
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(storage[0].isCompleted());
            SC_TEST_EXPECT(storage[1].isActive());
            SC_TEST_EXPECT(registry.cancelAll());
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(AwaitIsCancelled(storage[1].result()));
            SC_TEST_EXPECT(registry.clearCompleted() == 2);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[1];
            AwaitTaskRegistry registry(await, storage);
            AwaitTask         parent = waitAnyEmptyTaskRegistry(await, registry);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(parent.result());
            SC_TEST_EXPECT(registry.size() == 0);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[1];
            AwaitTaskRegistry registry(await, storage);

            SC_TEST_EXPECT(registry.spawn(waitLong(await)));
            SC_TEST_EXPECT(registry.activeCount() == 1);
            SC_TEST_EXPECT(not registry.spawn(waitTwice(await)));
            SC_TEST_EXPECT(registry.size() == 1);
            SC_TEST_EXPECT(registry.taskAt(0) == &storage[0]);
            SC_TEST_EXPECT(registry.taskAt(1) == nullptr);

            SC_TEST_EXPECT(registry.cancelAll());
            SC_TEST_EXPECT(registry.cancelAll());
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(storage[0].isCompleted());
            SC_TEST_EXPECT(AwaitIsCancelled(storage[0].result()));
            SC_TEST_EXPECT(registry.clearCompleted() == 1);
            SC_TEST_EXPECT(registry.size() == 0);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[2];
            AwaitTaskRegistry registry(await, storage);

            SC_TEST_EXPECT(registry.spawn(waitLong(await)));
            SC_TEST_EXPECT(registry.spawn(waitLong(await)));
            SC_TEST_EXPECT(registry.activeCount() == 2);
            SC_TEST_EXPECT(registry.cancelAll());
            SC_TEST_EXPECT(registry.cancelAll());
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(registry.completedCount() == 2);
            SC_TEST_EXPECT(AwaitIsCancelled(storage[0].result()));
            SC_TEST_EXPECT(AwaitIsCancelled(storage[1].result()));

            AwaitTaskGroupResultSummary summary;
            SC_TEST_EXPECT(registry.clearCompleted(&summary) == 2);
            SC_TEST_EXPECT(summary.numTasks == 2);
            SC_TEST_EXPECT(summary.numCompleted == 2);
            SC_TEST_EXPECT(summary.numSucceeded == 0);
            SC_TEST_EXPECT(summary.numFailed == 2);
            SC_TEST_EXPECT(summary.firstFailureIndex == 0);
            SC_TEST_EXPECT(AwaitIsCancelled(summary.firstFailure));
            SC_TEST_EXPECT(registry.clearCompleted(&summary) == 0);
            SC_TEST_EXPECT(registry.cancelAll());
            SC_TEST_EXPECT(registry.size() == 0);
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask         storage[1];
            AwaitTaskRegistry registry(await, storage);

            SC_TEST_EXPECT(not registry.spawn(AwaitTask()));
            SC_TEST_EXPECT(registry.size() == 0);
            SC_TEST_EXPECT(async.close());
        }
    }

    void taskGroup()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask task = waitTaskGroup(await);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask childA;
            AwaitTask childB;
            AwaitTask task = waitTaskGroupAndCollectResults(await, childA, childB);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(childA.result());
            SC_TEST_EXPECT(not childB.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask childA;
            AwaitTask childB;
            AwaitTask parent = waitCancellableTaskGroup(await, childA, childB);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isActive());
            SC_TEST_EXPECT(childA.isActive());
            SC_TEST_EXPECT(childB.isActive());
            SC_TEST_EXPECT(parent.cancel(await));
            SC_TEST_EXPECT(parent.cancel(await));
            SC_TEST_EXPECT(childA.cancel(await));
            SC_TEST_EXPECT(childB.cancel(await));

            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(childA.isCompleted());
            SC_TEST_EXPECT(childB.isCompleted());
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(not childA.result());
            SC_TEST_EXPECT(not childB.result());
            SC_TEST_EXPECT(not parent.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask nested;
            AwaitTask leafA;
            AwaitTask leafB;
            AwaitTask sibling;
            AwaitTask parent = waitOuterCancellableTaskGroup(await, nested, leafA, leafB, sibling);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isActive());
            SC_TEST_EXPECT(nested.isActive());
            SC_TEST_EXPECT(leafA.isActive());
            SC_TEST_EXPECT(leafB.isActive());
            SC_TEST_EXPECT(sibling.isActive());
            SC_TEST_EXPECT(parent.cancel(await));

            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(nested.isCompleted());
            SC_TEST_EXPECT(leafA.isCompleted());
            SC_TEST_EXPECT(leafB.isCompleted());
            SC_TEST_EXPECT(sibling.isCompleted());
            SC_TEST_EXPECT(not parent.result());
            SC_TEST_EXPECT(not nested.result());
            SC_TEST_EXPECT(not leafA.result());
            SC_TEST_EXPECT(not leafB.result());
            SC_TEST_EXPECT(not sibling.result());
            SC_TEST_EXPECT(async.close());
        }
    }

    void taskGroupWaitAny()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask task = waitAnyEmptyTaskGroup(await);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask task = waitAnyCompletedTaskGroup(await);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask slowChild;
            AwaitTask task = waitAnyTaskGroup(await, slowChild);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(slowChild.isCompleted());
            SC_TEST_EXPECT(not slowChild.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask slowChild;
            AwaitTask task = waitAnyFailingWinnerTaskGroup(await, slowChild);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(slowChild.isCompleted());
            SC_TEST_EXPECT(not slowChild.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask slowChild;
            AwaitTask task = waitAnyLeaveRemainingRunningTaskGroup(await, slowChild);
            SC_TEST_EXPECT(await.spawn(task));
            while (task.isActive())
            {
                SC_TEST_EXPECT(await.runOnce());
            }
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(slowChild.isActive());
            SC_TEST_EXPECT(slowChild.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(not slowChild.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask childA;
            AwaitTask childB;
            AwaitTask parent = waitAnyCancellableTaskGroup(await, childA, childB);
            SC_TEST_EXPECT(parent.isValid());
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isActive());
            SC_TEST_EXPECT(childA.isActive());
            SC_TEST_EXPECT(childB.isActive());
            SC_TEST_EXPECT(parent.cancel(await));
            SC_TEST_EXPECT(parent.cancel(await));
            SC_TEST_EXPECT(childA.cancel(await));
            SC_TEST_EXPECT(childB.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(childA.isCompleted());
            SC_TEST_EXPECT(childB.isCompleted());
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(not childA.result());
            SC_TEST_EXPECT(not childB.result());
            SC_TEST_EXPECT(not parent.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask child;
            AwaitTask parent = waitAllLeaveChildrenRunning(await, child);
            SC_TEST_EXPECT(await.spawn(parent));
            SC_TEST_EXPECT(parent.isActive());
            SC_TEST_EXPECT(child.isActive());
            SC_TEST_EXPECT(parent.cancel(await));
            SC_TEST_EXPECT(parent.isCompleted());
            SC_TEST_EXPECT(not parent.result());
            SC_TEST_EXPECT(child.isActive());

            SC_TEST_EXPECT(child.cancel(await));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(child.isCompleted());
            SC_TEST_EXPECT(not child.result());
            SC_TEST_EXPECT(async.close());
        }
    }

    void cancelChildTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask child  = waitLong(await);
        AwaitTask parent = awaitChild(await, child);

        SC_TEST_EXPECT(await.spawn(parent));
        SC_TEST_EXPECT(parent.isActive());
        SC_TEST_EXPECT(child.isActive());
        SC_TEST_EXPECT(parent.cancel(await));
        SC_TEST_EXPECT(parent.cancel(await));
        SC_TEST_EXPECT(child.cancel(await));
        SC_TEST_EXPECT(child.isCancellationRequested());

        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(child.isCompleted());
        SC_TEST_EXPECT(parent.isCompleted());
        SC_TEST_EXPECT(not child.result());
        SC_TEST_EXPECT(not parent.result());
        SC_TEST_EXPECT(async.close());
    }

    void waitForTimeout()
    {
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask task = waitForCompletedChild(await);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(async.close());
        }
        {
            AsyncEventLoop async;
            SC_TEST_EXPECT(async.create());
            SC_AWAIT_TEST_EVENT_LOOP(await, async);

            AwaitTask task = waitForTimedOutChild(await);
            SC_TEST_EXPECT(await.spawn(task));
            SC_TEST_EXPECT(await.run());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(async.close());
        }
    }

    void cancelWaitFor()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        AwaitTask          child;
        AwaitTimeoutResult timeoutResult;
        AwaitTask          parent = waitForCancellableChild(await, child, timeoutResult);
        SC_TEST_EXPECT(await.spawn(parent));
        SC_TEST_EXPECT(parent.isActive());
        SC_TEST_EXPECT(child.isActive());
        SC_TEST_EXPECT(parent.cancel(await));
        SC_TEST_EXPECT(parent.cancel(await));
        SC_TEST_EXPECT(child.cancel(await));

        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(child.isCompleted());
        SC_TEST_EXPECT(parent.isCompleted());
        SC_TEST_EXPECT(not child.result());
        SC_TEST_EXPECT(not parent.result());
        SC_TEST_EXPECT(not timeoutResult.timedOut);
        SC_TEST_EXPECT(async.close());
    }

    struct TrackingAllocatorInterface : public AwaitAllocatorInterface
    {
        void*  lastOwner      = nullptr;
        size_t lastNumBytes   = 0;
        size_t lastAlignment  = 0;
        size_t numAllocations = 0;
        size_t numReleases    = 0;

        virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) override
        {
            lastOwner     = const_cast<void*>(owner);
            lastNumBytes  = numBytes;
            lastAlignment = alignment;
            numAllocations++;
            return ::malloc(numBytes);
        }

        virtual void releaseImpl(void* memory) override
        {
            numReleases++;
            ::free(memory);
        }
    };

    void allocator()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());

        char           allocatorStorage[16 * 1024] = {0};
        AwaitAllocator allocator;
        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        AwaitEventLoop await(async, allocator);

        AwaitTask task = allocatorWait(await);
        SC_TEST_EXPECT(allocator.used() > 0);
        SC_TEST_EXPECT(allocator.peakUsed() >= allocator.used());
        SC_TEST_EXPECT(allocator.failedAllocationSize() == 0);

        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());

        char           diagnosticsMemory[16] = {};
        AwaitAllocator diagnostics;
        SC_TEST_EXPECT(diagnostics.createFixed(diagnosticsMemory));
        SC_TEST_EXPECT(diagnostics.capacity() == sizeof(diagnosticsMemory));
        SC_TEST_EXPECT(diagnostics.used() == 0);
        SC_TEST_EXPECT(diagnostics.peakUsed() == 0);
        SC_TEST_EXPECT(diagnostics.failedAllocationSize() == 0);
        SC_TEST_EXPECT(diagnostics.allocate(nullptr, 4, 1) == nullptr);
        SC_TEST_EXPECT(diagnostics.used() == 0);
        SC_TEST_EXPECT(diagnostics.peakUsed() == 0);
        SC_TEST_EXPECT(diagnostics.allocate(nullptr, 8, 1) == nullptr);
        SC_TEST_EXPECT(diagnostics.used() == 0);
        SC_TEST_EXPECT(diagnostics.peakUsed() == 0);
        SC_TEST_EXPECT(diagnostics.allocate(nullptr, 8, 1) == nullptr);
        SC_TEST_EXPECT(diagnostics.used() == 0);
        SC_TEST_EXPECT(diagnostics.peakUsed() == 0);
        SC_TEST_EXPECT(diagnostics.failedAllocationSize() == 8);
        SC_TEST_EXPECT(diagnostics.close());
    }

    void allocatorExhaustion()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());

        char           allocatorStorage[1] = {0};
        AwaitAllocator allocator;
        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        AwaitEventLoop await(async, allocator);

        AwaitTask task = allocatorWait(await);
        SC_TEST_EXPECT(not task.isValid());
        SC_TEST_EXPECT(not await.spawn(task));
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.peakUsed() == 0);
        SC_TEST_EXPECT(allocator.failedAllocationSize() > 0);
        SC_TEST_EXPECT(async.close());
    }

    void allocatorModes()
    {
        char fixedMemory[2048] = {};

        AwaitAllocator fixed;
        SC_TEST_EXPECT(fixed.createFixed(fixedMemory));
        void* fixedA = fixed.allocate(nullptr, 64, alignof(void*));
        void* fixedB = fixed.allocate(nullptr, 32, alignof(void*));
        SC_TEST_EXPECT(fixedA != nullptr);
        SC_TEST_EXPECT(fixedB != nullptr);
        SC_TEST_EXPECT(fixed.used() > 0);
        fixed.release(fixedA);
        fixed.release(fixedB);
        SC_TEST_EXPECT(fixed.used() == 0);
        SC_TEST_EXPECT(fixed.statistics().numAllocations == 2);
        SC_TEST_EXPECT(fixed.statistics().numReleases == 2);
        SC_TEST_EXPECT(fixed.close());

        AwaitAllocator mallocAllocator;
        SC_TEST_EXPECT(mallocAllocator.createMalloc());
        void* mallocMemory = mallocAllocator.allocate(nullptr, 128, alignof(void*));
        SC_TEST_EXPECT(mallocMemory != nullptr);
        SC_TEST_EXPECT(mallocAllocator.statistics().requestedBytesAllocated == 128);
        mallocAllocator.release(mallocMemory);
        SC_TEST_EXPECT(mallocAllocator.statistics().requestedBytesReleased == 128);
        SC_TEST_EXPECT(mallocAllocator.used() == 0);
        SC_TEST_EXPECT(mallocAllocator.close());

        TrackingAllocatorInterface tracking;
        AwaitAllocator             polymorphic;
        int                        owner = 0;
        SC_TEST_EXPECT(polymorphic.createPolymorphic(tracking));
        void* polymorphicMemory = polymorphic.allocate(&owner, 24, alignof(void*));
        SC_TEST_EXPECT(polymorphicMemory != nullptr);
        SC_TEST_EXPECT(tracking.lastOwner == &owner);
        SC_TEST_EXPECT(tracking.lastNumBytes >= 24);
        polymorphic.release(polymorphicMemory);
        SC_TEST_EXPECT(tracking.numAllocations == 1);
        SC_TEST_EXPECT(tracking.numReleases == 1);
        SC_TEST_EXPECT(polymorphic.close());

        AwaitAllocator virtualAllocator;
        SC_TEST_EXPECT(not virtualAllocator.createVirtual({}));
        SC_TEST_EXPECT(virtualAllocator.createVirtual({64 * 1024, 0}));
        SC_TEST_EXPECT(virtualAllocator.mode() == AwaitAllocatorMode::Virtual);
        SC_TEST_EXPECT(virtualAllocator.reservedBytes() >= 64 * 1024);
        SC_TEST_EXPECT(virtualAllocator.committedBytes() == 0);
        void* virtualMemory = virtualAllocator.allocate(nullptr, 1024, alignof(void*));
        SC_TEST_EXPECT(virtualMemory != nullptr);
        SC_TEST_EXPECT(virtualAllocator.committedBytes() > 0);
        virtualAllocator.release(virtualMemory);
        SC_TEST_EXPECT(virtualAllocator.used() == 0);
        SC_TEST_EXPECT(virtualAllocator.close());
    }

    void childTaskTeardownDuringCallback()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        SC_AWAIT_TEST_EVENT_LOOP(await, async);

        ThreadPool threadPool;
        SC_TEST_EXPECT(threadPool.create(1));

        SmallStringNative<255> filePath = StringEncoding::Native;
        const StringView       fileName = "await-child-callback-teardown.txt";
        SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory.view(), fileName}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.writeString(fileName, "callback teardown"));

        AwaitTask parent = spawnAndWaitFsChild(await, threadPool, filePath.view());
        SC_TEST_EXPECT(await.spawn(parent));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(parent.result());

        SC_TEST_EXPECT(threadPool.destroy());
        SC_TEST_EXPECT(async.close());
        SC_TEST_EXPECT(fs.removeFile(fileName));
    }
};

namespace SC
{
void runAwaitTest(SC::TestReport& report) { AwaitTest test(report); }
} // namespace SC
