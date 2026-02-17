// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"

namespace SC
{
struct FileTest;
}

struct SC::FileTest : public SC::TestCase
{
    FileTest(SC::TestReport& report) : TestCase(report, "FileTest")
    {
        using namespace SC;
        if (test_section("open"))
        {
            testOpen();
        }
        if (test_section("open stdhandles"))
        {
            testOpenStdHandles();
        }
        if (test_section("named pipe create/connect/accept"))
        {
            testNamedPipeCreateConnectAccept();
        }
    }
    inline void testOpen();
    inline void testOpenStdHandles();
    inline void testNamedPipeCreateConnectAccept();

    Result snippetForUniqueHandle();
    Result snippetForNamedPipeServer();
};

void SC::FileTest::testOpen()
{
    SmallStringNative<255> filePath = StringEncoding::Native;
    SmallStringNative<255> dirPath  = StringEncoding::Native;
    // Setup the test
    FileSystem fs;

    const StringView name     = "FileTest";
    const StringView fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectory(name));
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));

    //! [FileSnippet]
    // Open a file, write and close it
    FileDescriptor fd;
    SC_TEST_EXPECT(fd.open(filePath.view(), FileOpen::Write));
    SC_TEST_EXPECT(fd.write(StringView("test").toCharSpan()));
    SC_TEST_EXPECT(fd.close());

    // Re-open the file for read
    SC_TEST_EXPECT(fd.open(filePath.view(), FileOpen::Read));

    // Read some data from the file
    char       buffer[4] = {0};
    Span<char> spanOut;
    SC_TEST_EXPECT(fd.read({buffer, sizeof(buffer)}, spanOut));
    //! [FileSnippet]

    size_t expectedSize;
    SC_TEST_EXPECT(fd.sizeInBytes(expectedSize) and expectedSize == 4);
    SC_TEST_EXPECT(fd.seek(FileDescriptor::SeekEnd, 0));
    size_t position;
    SC_TEST_EXPECT(fd.currentPosition(position) and position == expectedSize);
    SC_TEST_EXPECT(fd.seek(FileDescriptor::SeekCurrent, -1));
    SC_TEST_EXPECT(fd.currentPosition(position) and position == expectedSize - 1);
    SC_TEST_EXPECT(fd.seek(FileDescriptor::SeekStart, 0));

    SC_TEST_EXPECT(fd.close());
    // Check if read content matches
    StringView sv(spanOut, false, StringEncoding::Ascii);
    SC_TEST_EXPECT(sv.compare("test") == StringView::Comparison::Equals);

    // Shutdown test
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}

void SC::FileTest::testOpenStdHandles()
{
    FileDescriptor fd[3];
    SC_TEST_EXPECT(fd[0].openStdInDuplicate());
    SC_TEST_EXPECT(fd[1].openStdOutDuplicate());
    SC_TEST_EXPECT(fd[2].openStdErrDuplicate());
}

void SC::FileTest::testNamedPipeCreateConnectAccept()
{
    SmallString<64> logicalName;
    SC_TEST_EXPECT(StringBuilder::format(logicalName, "sc-file-test-{}", report.mapPort(5420)));

    StringPath pipePath;
    SC_TEST_EXPECT(NamedPipeName::build(logicalName.view(), pipePath));

    StringPath outputPath;
    SC_TEST_EXPECT(not NamedPipeName::build("", outputPath));
    SC_TEST_EXPECT(not NamedPipeName::build("invalid/name", outputPath));
    SC_TEST_EXPECT(not NamedPipeName::build("invalid\\name", outputPath));

#if !SC_PLATFORM_WINDOWS
    NamedPipeNameOptions nameOptions;
    nameOptions.posixDirectory = "/tmp";
    SC_TEST_EXPECT(NamedPipeName::build("sc-file-test-custom", outputPath, nameOptions));
    nameOptions.posixDirectory = "relative";
    SC_TEST_EXPECT(not NamedPipeName::build("sc-file-test-custom", outputPath, nameOptions));
#endif

    NamedPipeServer        server;
    NamedPipeServerOptions options;
    options.posix.removeEndpointBeforeCreate = true;
    SC_TEST_EXPECT(server.create(pipePath.view(), options));

    NamedPipeServer duplicateServer;
    SC_TEST_EXPECT(not duplicateServer.create(pipePath.view()));

#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(not duplicateServer.create("invalid"));
#else
    SC_TEST_EXPECT(not duplicateServer.create("relative/path.sock"));
#endif

    auto acceptAndConnect =
        [&](NamedPipeServer& namedPipeServer, StringSpan namedPipePath, StringSpan payload, StringSpan expected)
    {
        PipeDescriptor accepted;
        EventObject    acceptedEvent;
        Result         acceptResult = Result::Error("accept not completed");
        struct AcceptContext
        {
            NamedPipeServer* server;
            PipeDescriptor*  accepted;
            Result*          result;
            EventObject*     event;
        } acceptContext = {&namedPipeServer, &accepted, &acceptResult, &acceptedEvent};
        Thread acceptThread;
        SC_TEST_EXPECT(acceptThread.start(
            [&acceptContext](Thread&)
            {
                *acceptContext.result = acceptContext.server->accept(*acceptContext.accepted);
                acceptContext.event->signal();
            }));

        PipeDescriptor client;
        SC_TEST_EXPECT(NamedPipeClient::connect(namedPipePath, client));

        acceptedEvent.wait();
        SC_TEST_EXPECT(acceptThread.join());
        SC_TEST_EXPECT(acceptResult);

        SC_TEST_EXPECT(client.writePipe.writeString(payload));

        char       buffer[64] = {0};
        Span<char> readData;
        SC_TEST_EXPECT(accepted.readPipe.read({buffer, payload.sizeInBytes()}, readData));
        StringSpan received(readData, false, StringEncoding::Ascii);
        SC_TEST_EXPECT(received == expected);

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(accepted.close());
    };

    acceptAndConnect(server, pipePath.view(), "first", "first");
    acceptAndConnect(server, pipePath.view(), "second", "second");

    SC_TEST_EXPECT(server.close());

    PipeDescriptor shouldFail;
    SC_TEST_EXPECT(not NamedPipeClient::connect(pipePath.view(), shouldFail));

#if SC_PLATFORM_WINDOWS
    SmallString<128> alternatePathASCII;
    SC_TEST_EXPECT(StringBuilder::format(alternatePathASCII, "\\\\?\\pipe\\sc-file-test-alt-{}", report.mapPort(5421)));
    StringPath alternatePath;
    SC_TEST_EXPECT(alternatePath.assign(alternatePathASCII.view()));

    NamedPipeServer        alternateServer;
    NamedPipeServerOptions alternateOptions;
    SC_TEST_EXPECT(alternateServer.create(alternatePath.view(), alternateOptions));
    acceptAndConnect(alternateServer, alternatePath.view(), "alt!", "alt!");
    SC_TEST_EXPECT(alternateServer.close());
#endif
}

//! [NamedPipeServerSnippet]
SC::Result SC::FileTest::snippetForNamedPipeServer()
{
    StringPath pipePath;
    SC_TRY(NamedPipeName::build("sc-doc-example", pipePath));

    NamedPipeServer        server;
    NamedPipeServerOptions serverOptions;
    serverOptions.connectionOptions.blocking       = true;
    serverOptions.posix.removeEndpointBeforeCreate = true;
    SC_TRY(server.create(pipePath.view(), serverOptions));

    PipeDescriptor clientConnection;
    SC_TRY(NamedPipeClient::connect(pipePath.view(), clientConnection));

    PipeDescriptor serverConnection;
    SC_TRY(server.accept(serverConnection));

    SC_TRY(clientConnection.writePipe.writeString("ping"));

    char       readBuffer[4] = {0};
    Span<char> readData;
    SC_TRY(serverConnection.readPipe.read({readBuffer, sizeof(readBuffer)}, readData));

    StringSpan received(readData, false, StringEncoding::Ascii);
    SC_TRY_MSG(received == "ping", "Unexpected payload");

    SC_TRY(clientConnection.close());
    SC_TRY(serverConnection.close());
    SC_TRY(server.close());
    return Result(true);
}
//! [NamedPipeServerSnippet]

#if !SC_PLATFORM_WINDOWS
//! [UniqueHandleExampleSnippet]
#include <fcntl.h> // for open
SC::Result SC::FileTest::snippetForUniqueHandle()
{
    StringPath filePath;
    SC_TRY(filePath.assign("someFile.txt"));

    const int flags  = O_RDWR | O_CREAT | O_TRUNC; // Open for read/write, create if not exists, truncate if exists
    const int access = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // Read/write for owner, read for group and others

    FileDescriptor myDescriptor;

    const int nativeFd = ::open(filePath.view().bytesIncludingTerminator(), flags, access);

    // Assign the native handle to UniqueHandle (will release the existing one, if any)
    SC_TRY(myDescriptor.assign(nativeFd));

    // UniqueHandle can only be moved, but not copied
    FileDescriptor otherDescriptor = move(myDescriptor);
    // FileDescriptor otherDescriptor = myDescriptor; // <- Doesn't compile

    // Explicitly close (or it will be automatically released on scope close / destructor)
    SC_TRY(otherDescriptor.close());

    // If detach() is called, the handle will be made invalid without releasing it
    otherDescriptor.detach();

    // Check handle for validity
    if (otherDescriptor.isValid())
    {
        // ... do something
    }
    return Result(true);
}
//! [UniqueHandleExampleSnippet]
#endif
namespace SC
{
void runFileTest(SC::TestReport& report) { FileTest test(report); }
} // namespace SC
