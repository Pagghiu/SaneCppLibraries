// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that copies a file, then sends it over a socket with Await.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitFileCourier` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Common/Deferred.h"
#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Socket/Socket.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringView.h"
#include "../../Libraries/Threading/ThreadPool.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/File/FileErrorFormatter.h"
#include "../../Libraries/FileSystem/FileSystemErrorFormatter.h"
#include "../../Libraries/Socket/SocketErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitFileCourierResultCategory = ResultCategory(0x80000017u);
enum class AwaitFileCourierError : uint32_t
{
    IncompleteFileSend = 1,
    CurrentDirectoryUnavailable,
};
static constexpr Result AwaitFileCourierFailure(AwaitFileCourierError error)
{
    return Result::Error(AwaitFileCourierResultCategory, error);
}
static ResultErrorFormat formatAwaitFileCourierError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() == SocketResultCategory)
        return formatSocketError(result, output);
    if (result.category() == FileSystemResultCategory)
        return formatFileSystemError(result, output);
    if (result.category() == FileResultCategory)
        return formatFileError(result, output);
    if (result.category() != AwaitFileCourierResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitFileCourierError>(result.errorValue()))
    {
    case AwaitFileCourierError::IncompleteFileSend: formatter.append("Incomplete fileSend"); break;
    case AwaitFileCourierError::CurrentDirectoryUnavailable:
        formatter.append("Could not resolve current working directory");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static Result createServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket, SocketIPAddress& address)
{
    constexpr uint16_t firstPort = 39171;
    constexpr uint16_t numPorts  = 64;

    Result lastError = Result::Error(SocketResultCategory, SocketError::BindFailed);
    for (uint16_t offset = 0; offset < numPorts; ++offset)
    {
        const uint16_t port = static_cast<uint16_t>(firstPort + offset);
        SC_TRY(address.fromAddressPort("127.0.0.1", port));

        SocketDescriptor candidate;
        SC_TRY(eventLoop.createAsyncTCPSocket(address.getAddressFamily(), candidate));

        SocketServer server(candidate);
        lastError = server.bind(address);
        if (lastError)
        {
            SC_TRY(server.listen(1));
            return Result::Explicit(serverSocket.assign(move(candidate)));
        }
    }
    return lastError;
}

static Result createSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& sender, SocketDescriptor& receiver)
{
    SocketDescriptor serverSocket;
    SocketIPAddress  address;
    SC_TRY(createServer(eventLoop, serverSocket, address));
    auto closeServer = MakeDeferred([&serverSocket] { (void)serverSocket.close(); });

    SocketDescriptor client;
    SC_TRY(client.create(address.getAddressFamily()));
    SC_TRY(SocketClient(client).connect("127.0.0.1", address.getPort()));

    SocketDescriptor accepted;
    SC_TRY(SocketServer(serverSocket).accept(address.getAddressFamily(), accepted));
    SC_TRY(client.setBlocking(false));
    SC_TRY(accepted.setBlocking(false));
    SC_TRY(eventLoop.associateExternallyCreatedSocket(client));
    SC_TRY(eventLoop.associateExternallyCreatedSocket(accepted));

    SC_TRY(sender.assign(move(accepted)));
    SC_TRY(receiver.assign(move(client)));
    return Result(true);
}

static AwaitTask copyThenSend(AwaitEventLoop& await, ThreadPool& threadPool, StringSpan sourcePath, StringSpan copyPath,
                              SocketDescriptor& sender, SocketDescriptor& receiver, Span<char> receiveBuffer,
                              AwaitSocketReceiveResult& received)
{
    SC_CO_TRY(co_await await.fsCopyFile(threadPool, sourcePath, copyPath));

    FileDescriptor copiedFile;
    SC_CO_TRY(co_await await.fsOpen(threadPool, copyPath, FileOpen::Read, copiedFile));

    AwaitFileSendOptions sendOptions;
    sendOptions.threadPool = &threadPool;
    sendOptions.length     = receiveBuffer.sizeInBytes();

    AwaitFileSendResult sendResult;
    SC_CO_TRY(co_await await.fileSend(copiedFile, sender, sendResult, sendOptions));
    SC_CO_TRY(co_await await.fsClose(threadPool, copiedFile));

    if (not sendResult.complete or sendResult.bytesTransferred != receiveBuffer.sizeInBytes())
    {
        co_return AwaitFileCourierFailure(AwaitFileCourierError::IncompleteFileSend);
    }

    SC_CO_TRY(co_await await.receiveExact(receiver, receiveBuffer, &received));
    co_return Result(true);
}

static Result runAwaitFileCourier()
{
    Console console;
    Console::tryAttachingToParentConsole();
    SocketNetworking::initNetworking();
    auto shutdownNetworking = MakeDeferred([] { SocketNetworking::shutdownNetworking(); });

    StringPath workingDirectory;
    StringSpan cwd = FileSystem::Operations::getCurrentWorkingDirectory(workingDirectory);
    if (cwd.isEmpty())
    {
        return AwaitFileCourierFailure(AwaitFileCourierError::CurrentDirectoryUnavailable);
    }

    StringPath sourcePath;
    StringPath copyPath;
    SC_TRY(Path::join(sourcePath, {cwd, "await-courier-source.txt"}));
    SC_TRY(Path::join(copyPath, {cwd, "await-courier-copy.txt"}));

    const char payload[] = "Await file courier payload";

    FileSystem fs;
    SC_TRY(fs.write(sourcePath.view(), {payload, sizeof(payload) - 1}));
    auto cleanup = MakeDeferred(
        [&fs, &sourcePath, &copyPath]
        {
            (void)fs.removeFile(sourcePath.view());
            (void)fs.removeFile(copyPath.view());
        });

    ThreadPool threadPool;
    SC_TRY(threadPool.create(2));
    auto destroyThreadPool = MakeDeferred([&threadPool] { (void)threadPool.destroy(); });

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    char           allocatorStorage[16 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    SocketDescriptor sender;
    SocketDescriptor receiver;
    SC_TRY(createSocketPair(async, sender, receiver));
    auto closeSender   = MakeDeferred([&sender] { (void)sender.close(); });
    auto closeReceiver = MakeDeferred([&receiver] { (void)receiver.close(); });

    char                     receiveBuffer[sizeof(payload) - 1] = {};
    AwaitSocketReceiveResult received;
    AwaitTask task = copyThenSend(await, threadPool, sourcePath.view(), copyPath.view(), sender, receiver,
                                  {receiveBuffer, sizeof(receiveBuffer)}, received);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result() : runResult;
    }
    SC_TRY(task.result());

    StringView text({received.data.data(), received.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("AwaitFileCourier copied and sent: {}\n", text);
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitFileCourier();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitFileCourierError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitFileCourier failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitFileCourier failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
