// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/File/File.h"
#include "Libraries/Common/TypeTraits.h"
#include "Libraries/File/FileErrorFormatter.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <string.h>

namespace
{
#if !SC_PLATFORM_WINDOWS
static bool standardFileDescriptorIsOpen(int fileDescriptor)
{
    errno = 0;
    return ::fcntl(fileDescriptor, F_GETFD) != -1 or errno != EBADF;
}
#endif
} // namespace

namespace SC
{
struct FileTest;
}

struct SC::FileTest : public SC::TestCase
{
    FileTest(SC::TestReport& report) : TestCase(report, "FileTest")
    {
        using namespace SC;
        if (test_section("structured errors and formatter"))
        {
            structuredErrorsAndFormatter();
        }
        if (test_section("structured producer failures"))
        {
            structuredProducerFailures();
        }
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
        if (test_section("descriptor operations"))
        {
            testDescriptorOperations();
        }
        if (test_section("descriptor invalid handle"))
        {
            testDescriptorInvalidHandle();
        }
    }
    inline void testOpen();
    inline void structuredErrorsAndFormatter();
    inline void structuredProducerFailures();
    inline void testOpenStdHandles();
    inline void testNamedPipeCreateConnectAccept();
    inline void testDescriptorOperations();
    inline void testDescriptorInvalidHandle();

    Result snippetForUniqueHandle();
    Result snippetForNamedPipeServer();
};

void SC::FileTest::structuredErrorsAndFormatter()
{
    static_assert(FileResultCategory.value == 10, "File category is registry value 10");
    static_assert(static_cast<uint32_t>(FileError::InvalidHandle) == 1, "File error values are append-only");
    static_assert(static_cast<uint32_t>(FileError::PipeDisconnected) == 31, "File error values are append-only");
    static_assert(static_cast<uint16_t>(FileErrorDetail::None) == 0, "File detail zero is reserved for no detail");
    static_assert(static_cast<uint16_t>(FileErrorContextKind::None) == 0,
                  "File context kind zero is reserved for no context");
    static_assert(sizeof(Result) != 16 or sizeof(ResultFile) == 24,
                  "ResultFile bridge layout must retain its 24-byte size");
    static_assert(sizeof(Result) != 8 or sizeof(ResultFile) == 16, "ResultFile must meet the final 16-byte target");
    static_assert(__is_standard_layout(ResultFile), "ResultFile must remain standard-layout");
    static_assert(TypeTraits::IsTriviallyCopyable<ResultFile>::value, "ResultFile must remain trivially copyable");

    const ResultFile detailed =
        ResultFile::withNativeError(FileError::ReadFailed, FileErrorDetail::ReadDescriptor, 12345);
    const ResultFile copied = detailed;
    SC_TEST_EXPECT(copied.isError(FileError::ReadFailed));
    SC_TEST_EXPECT(copied.detail == FileErrorDetail::ReadDescriptor);
    SC_TEST_EXPECT(copied.contextKind == FileErrorContextKind::NativeError);
    SC_TEST_EXPECT(copied.context.nativeError == 12345);

    const Result plain = detailed;
    SC_TEST_EXPECT(plain.isError(FileResultCategory, FileError::ReadFailed));
    const ResultFile fromPlain(plain);
    SC_TEST_EXPECT(fromPlain.detail == FileErrorDetail::None);
    SC_TEST_EXPECT(fromPlain.contextKind == FileErrorContextKind::None);
    SC_TEST_EXPECT(fromPlain.context.nativeError == 0);

    const Result     foreignResult = Result::Error(ResultCategory(8), 1);
    const ResultFile foreign(foreignResult);
    SC_TEST_EXPECT(not foreign);
    SC_TEST_EXPECT(foreign.toResult().isError(ResultCategory(8), 1));
    SC_TEST_EXPECT(foreign.detail == FileErrorDetail::None);
    SC_TEST_EXPECT(foreign.contextKind == FileErrorContextKind::None);

    const ResultFile required =
        ResultFile::withRequiredBytes(FileError::PathCapacityExceeded, FileErrorDetail::BuildTransportPath, 1031);
    SC_TEST_EXPECT(required.contextKind == FileErrorContextKind::RequiredBytes);
    SC_TEST_EXPECT(required.context.requiredBytes == 1031);
    const ResultFile actual =
        ResultFile::withActualBytes(FileError::IncompleteWrite, FileErrorDetail::WriteDescriptor, 3);
    SC_TEST_EXPECT(actual.contextKind == FileErrorContextKind::ActualBytes);
    SC_TEST_EXPECT(actual.context.actualBytes == 3);

    const auto propagate = [](ResultFile result) -> ResultFile
    {
        SC_TRY(result);
        return ResultFile(true);
    };
    const ResultFile propagated = propagate(detailed);
    SC_TEST_EXPECT(propagated.isError(FileError::ReadFailed));
    SC_TEST_EXPECT(propagated.detail == FileErrorDetail::ReadDescriptor);
    SC_TEST_EXPECT(propagated.contextKind == FileErrorContextKind::NativeError);
    SC_TEST_EXPECT(propagated.context.nativeError == 12345);

    constexpr char    expected[] = "Failed to read file descriptor (detail: read descriptor) (native error: 12345)";
    char              message[sizeof(expected)];
    ResultErrorFormat formatted = formatFileError(detailed, message);
    SC_TEST_EXPECT(formatted);
    SC_TEST_EXPECT(formatted.requiredCapacity == sizeof(expected));
    SC_TEST_EXPECT(::memcmp(message, expected, sizeof(expected)) == 0);

    formatted = formatFileError(FileError::PathMustBeAbsolute, {});
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::InsufficientCapacity);
    SC_TEST_EXPECT(formatted.requiredCapacity > 1);
    char tooSmall[2] = {'x', 0};
    formatted        = formatFileError(FileError::PathMustBeAbsolute, tooSmall);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::InsufficientCapacity);
    SC_TEST_EXPECT(tooSmall[0] == 0);
    SC_TEST_EXPECT(formatFileError(Result(true), message).status == ResultErrorFormatStatus::NotAnError);
    SC_TEST_EXPECT(formatFileError(Result::Error(ResultCategory(99), 1), message).status ==
                   ResultErrorFormatStatus::ForeignCategory);
    SC_TEST_EXPECT(formatFileError(Result::Error(FileResultCategory, 999), message).status ==
                   ResultErrorFormatStatus::UnknownError);
    SC_TEST_EXPECT(
        formatFileError(ResultFile(FileError::ReadFailed, static_cast<FileErrorDetail>(999)), message).status ==
        ResultErrorFormatStatus::UnknownError);
    SC_TEST_EXPECT(formatFileError(ResultFile(FileError::ReadFailed, FileErrorDetail::None,
                                              static_cast<FileErrorContextKind>(999), {}),
                                   message)
                       .status == ResultErrorFormatStatus::UnknownError);
}

void SC::FileTest::structuredProducerFailures()
{
#if !SC_PLATFORM_WINDOWS
    FileDescriptor descriptor;
    ResultFile     result = descriptor.open("relative.txt", FileOpen::Read);
    SC_TEST_EXPECT(result.isError(FileError::PathMustBeAbsolute));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::ValidateAbsolutePath);
    SC_TEST_EXPECT(result.contextKind == FileErrorContextKind::None);

    result = descriptor.open("/__sc_file_result_missing_5423__", FileOpen::Read);
    SC_TEST_EXPECT(result.isError(FileError::OpenFailed));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::OpenFile);
    SC_TEST_EXPECT(result.contextKind == FileErrorContextKind::NativeError);
    SC_TEST_EXPECT(result.context.nativeError == ENOENT);

    FileDescriptorStat statInfo;
    result = descriptor.stat(statInfo);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::QueryDescriptorMetadata);
    result = descriptor.chmod(0644u);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::ChangeDescriptorPermissions);
    result = descriptor.chown(0, 0);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::ChangeDescriptorOwnership);
    result = descriptor.sync();
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::SynchronizeDescriptor);
    result = descriptor.syncData();
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::SynchronizeDescriptorData);
    result = descriptor.truncate(1);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::TruncateDescriptor);

    PipeDescriptor pipe;
    PipeOptions    options;
    options.blocking = false;
    SC_TEST_EXPECT(pipe.createPipe(options));
    char fill[4096] = {};
    for (size_t iteration = 0; iteration < 4096; ++iteration)
    {
        result = pipe.writePipe.write({fill, sizeof(fill)});
        if (not result)
            break;
    }
    SC_TEST_EXPECT(result.isError(FileError::WouldBlock));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::WriteDescriptor);
    SC_TEST_EXPECT(result.contextKind == FileErrorContextKind::NativeError);
    SC_TEST_EXPECT(result.context.nativeError == EAGAIN or result.context.nativeError == EWOULDBLOCK);

    char       drainedStorage[4096];
    Span<char> drained;
    SC_TEST_EXPECT(pipe.readPipe.read(drainedStorage, drained));
    SC_TEST_EXPECT(drained.sizeInBytes() > 0);
    char largeWrite[65536] = {};
    result                 = pipe.writePipe.write(largeWrite);
    SC_TEST_EXPECT(result.isError(FileError::IncompleteWrite));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::WriteDescriptor);
    SC_TEST_EXPECT(result.contextKind == FileErrorContextKind::ActualBytes);
    SC_TEST_EXPECT(result.context.actualBytes > 0);
    SC_TEST_EXPECT(result.context.actualBytes < sizeof(largeWrite));
    SC_TEST_EXPECT(pipe.close());
#else
    FileDescriptor descriptor;
    ResultFile     result = descriptor.open(L"relative.txt", FileOpen::Read);
    SC_TEST_EXPECT(result.isError(FileError::PathMustBeAbsolute));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::ValidateAbsolutePath);
    SC_TEST_EXPECT(result.contextKind == FileErrorContextKind::None);

    result = descriptor.open(L"\\\\?\\malformed", FileOpen::Read);
    SC_TEST_EXPECT(result.isError(FileError::InvalidPath));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::NormalizePath);

    wchar_t oversizedPath[StringPath::StorageCapacity + 2];
    for (size_t index = 0; index < StringPath::StorageCapacity + 1; ++index)
        oversizedPath[index] = L'a';
    oversizedPath[StringPath::StorageCapacity + 1] = L'\0';
    result = descriptor.open(StringSpan({oversizedPath, StringPath::StorageCapacity + 1}, true, StringEncoding::Utf16),
                             FileOpen::Read);
    SC_TEST_EXPECT(result.isError(FileError::PathCapacityExceeded));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::NormalizePath);

    FileDescriptorStat statInfo;
    result = descriptor.stat(statInfo);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::QueryDescriptorMetadata);
    result = descriptor.chmod(0644u);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::ChangeDescriptorPermissions);
    result = descriptor.chown(0, 0);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::ChangeDescriptorOwnership);
    result = descriptor.sync();
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::SynchronizeDescriptor);
    result = descriptor.syncData();
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::SynchronizeDescriptorData);
    result = descriptor.truncate(1);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::TruncateDescriptor);
#endif

    size_t position = 0;
    result          = descriptor.seek(FileDescriptor::SeekStart, 0);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::SeekDescriptor);
    result = descriptor.currentPosition(position);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::QueryDescriptorPosition);
    result = descriptor.sizeInBytes(position);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::QueryDescriptorSize);
    char       byte = 0;
    Span<char> read;
    result = descriptor.read({&byte, 1}, read);
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::ReadDescriptor);
    result = descriptor.write({&byte, 1});
    SC_TEST_EXPECT(result.isError(FileError::InvalidHandle));
    SC_TEST_EXPECT(result.detail == FileErrorDetail::WriteDescriptor);

    PipeDescriptor closeFailurePipe;
    SC_TEST_EXPECT(closeFailurePipe.createPipe());
    FileDescriptor::Handle externallyClosedRead = FileDescriptor::Invalid;
    SC_TEST_EXPECT(closeFailurePipe.readPipe.get(externallyClosedRead, Result(false)));
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(::CloseHandle(externallyClosedRead) != FALSE);
#else
    SC_TEST_EXPECT(::close(externallyClosedRead) == 0);
#endif
    const ResultFile closeResult = closeFailurePipe.close();
    SC_TEST_EXPECT(closeResult.isError(FileError::CloseFailed));
    SC_TEST_EXPECT(closeResult.detail == FileErrorDetail::CloseDescriptor);
    SC_TEST_EXPECT(closeResult.contextKind == FileErrorContextKind::NativeError);
    SC_TEST_EXPECT(not closeFailurePipe.readPipe.isValid());
    SC_TEST_EXPECT(not closeFailurePipe.writePipe.isValid());

    NamedPipeServer  uncreatedServer;
    PipeDescriptor   unacceptedConnection;
    const ResultFile acceptResult = uncreatedServer.accept(unacceptedConnection);
    SC_TEST_EXPECT(acceptResult.isError(FileError::InvalidState));
    SC_TEST_EXPECT(acceptResult.detail == FileErrorDetail::AcceptNamedPipeConnection);

    PipeDescriptor eofPipe;
    SC_TEST_EXPECT(eofPipe.createPipe());
    SC_TEST_EXPECT(eofPipe.writePipe.close());
    Span<char> eofRead;
    SC_TEST_EXPECT(eofPipe.readPipe.read({&byte, 1}, eofRead));
    SC_TEST_EXPECT(eofRead.empty());
    SC_TEST_EXPECT(eofPipe.close());
}

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

#if SC_PLATFORM_WINDOWS
    StringPath prefixedFilePath;
    SC_TEST_EXPECT(prefixedFilePath.assign("\\\\?\\"_a8));
    SC_TEST_EXPECT(prefixedFilePath.append(filePath.view()));

    SC_TEST_EXPECT(fd.open(prefixedFilePath.view(), FileOpen::Read));
    SC_TEST_EXPECT(fd.close());
#endif

    // Shutdown test
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}

void SC::FileTest::testOpenStdHandles()
{
    FileDescriptor fd[3];
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(fd[0].openStdInDuplicate());
    SC_TEST_EXPECT(fd[1].openStdOutDuplicate());
    SC_TEST_EXPECT(fd[2].openStdErrDuplicate());
#else
    if (standardFileDescriptorIsOpen(STDIN_FILENO))
    {
        SC_TEST_EXPECT(fd[0].openStdInDuplicate());
    }
    if (standardFileDescriptorIsOpen(STDOUT_FILENO))
    {
        SC_TEST_EXPECT(fd[1].openStdOutDuplicate());
    }
    if (standardFileDescriptorIsOpen(STDERR_FILENO))
    {
        SC_TEST_EXPECT(fd[2].openStdErrDuplicate());
    }
#endif
}

void SC::FileTest::testNamedPipeCreateConnectAccept()
{
    SmallString<64> logicalName;
    SC_TEST_EXPECT(StringBuilder::format(logicalName, "sc-file-test-{}", report.mapPort(5420)));

    StringPath pipePath;
    SC_TEST_EXPECT(NamedPipeName::build(logicalName.view(), pipePath));

    StringPath outputPath;
    ResultFile nameResult = NamedPipeName::build("", outputPath);
    SC_TEST_EXPECT(nameResult.isError(FileError::InvalidNamedPipeName));
    SC_TEST_EXPECT(nameResult.detail == FileErrorDetail::ValidateNamedPipeLogicalName);
    nameResult = NamedPipeName::build("invalid/name", outputPath);
    SC_TEST_EXPECT(nameResult.isError(FileError::InvalidNamedPipeName));
    nameResult = NamedPipeName::build("invalid\\name", outputPath);
    SC_TEST_EXPECT(nameResult.isError(FileError::InvalidNamedPipeName));

#if !SC_PLATFORM_WINDOWS
    NamedPipeNameOptions nameOptions;
    nameOptions.posixDirectory = "/tmp";
    SC_TEST_EXPECT(NamedPipeName::build("sc-file-test-custom", outputPath, nameOptions));
    nameOptions.posixDirectory = "relative";
    nameResult                 = NamedPipeName::build("sc-file-test-custom", outputPath, nameOptions);
    SC_TEST_EXPECT(nameResult.isError(FileError::PathMustBeAbsolute));
    SC_TEST_EXPECT(nameResult.detail == FileErrorDetail::ValidateAbsolutePath);
#endif

    NamedPipeServer        server;
    NamedPipeServerOptions options;
    options.posix.removeEndpointBeforeCreate = true;
    SC_TEST_EXPECT(server.create(pipePath.view(), options));
    const ResultFile alreadyCreated = server.create(pipePath.view(), options);
    SC_TEST_EXPECT(alreadyCreated.isError(FileError::InvalidState));
    SC_TEST_EXPECT(alreadyCreated.detail == FileErrorDetail::CreateNamedPipeServer);

    NamedPipeServer  duplicateServer;
    const ResultFile duplicateCreate = duplicateServer.create(pipePath.view());
    SC_TEST_EXPECT(duplicateCreate.isError(FileError::NamedPipeCreationFailed));
    SC_TEST_EXPECT(duplicateCreate.contextKind == FileErrorContextKind::NativeError);

#if SC_PLATFORM_WINDOWS
    const ResultFile invalidCreate = duplicateServer.create("invalid");
    SC_TEST_EXPECT(invalidCreate.isError(FileError::InvalidNamedPipeName));
    SC_TEST_EXPECT(invalidCreate.detail == FileErrorDetail::ValidateNamedPipePath);
#else
    const ResultFile invalidCreate = duplicateServer.create("relative/path.sock");
    SC_TEST_EXPECT(invalidCreate.isError(FileError::PathMustBeAbsolute));
    SC_TEST_EXPECT(invalidCreate.detail == FileErrorDetail::ValidateAbsolutePath);
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

    PipeDescriptor   shouldFail;
    const ResultFile connectAfterClose = NamedPipeClient::connect(pipePath.view(), shouldFail);
    SC_TEST_EXPECT(connectAfterClose.isError(FileError::NamedPipeConnectionFailed));
    SC_TEST_EXPECT(connectAfterClose.detail == FileErrorDetail::ConnectNamedPipeClient);
    SC_TEST_EXPECT(connectAfterClose.contextKind == FileErrorContextKind::NativeError);

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

void SC::FileTest::testDescriptorOperations()
{
    SmallStringNative<255> filePath = StringEncoding::Native;
    SmallStringNative<255> dirPath  = StringEncoding::Native;

    FileSystem fs;

    const StringView directoryName = "FileDescriptorOperationsTest";
    const StringView fileName      = "descriptor.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), directoryName}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    if (fs.existsAndIsDirectory(directoryName))
    {
        SC_TEST_EXPECT(fs.removeDirectoryRecursive(directoryName));
    }
    SC_TEST_EXPECT(fs.makeDirectory(directoryName));

    FileDescriptor fd;
    SC_TEST_EXPECT(fd.open(filePath.view(), FileOpen::WriteRead));
    SC_TEST_EXPECT(fd.writeString("abcdef"));
    SC_TEST_EXPECT(fd.sync());
    SC_TEST_EXPECT(fd.syncData());

    FileDescriptorStat statInfo;
    SC_TEST_EXPECT(fd.stat(statInfo));
    SC_TEST_EXPECT(statInfo.entryType == FileDescriptorEntryType::File);
    SC_TEST_EXPECT(statInfo.fileSize == 6);
    SC_TEST_EXPECT(statInfo.hardLinkCount >= 1);
    SC_TEST_EXPECT(statInfo.modifiedTime.milliseconds > 0);
    SC_TEST_EXPECT(statInfo.accessedTime.milliseconds > 0);

#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(fd.seek(FileDescriptor::SeekStart, 0xffffffffLL));
    size_t largePosition = 0;
    SC_TEST_EXPECT(fd.currentPosition(largePosition));
    SC_TEST_EXPECT(largePosition == 0xffffffffULL);
    SC_TEST_EXPECT(fd.seek(FileDescriptor::SeekStart, 6));

    SC_TEST_EXPECT(statInfo.creationTime.milliseconds > 0);
    SC_TEST_EXPECT(statInfo.windows.attributes != 0);

    SC_TEST_EXPECT(fd.chmod(0));
    FileDescriptorStat readOnlyStat;
    SC_TEST_EXPECT(fd.stat(readOnlyStat));
    SC_TEST_EXPECT((readOnlyStat.windows.attributes & FILE_ATTRIBUTE_READONLY) != 0);

    SC_TEST_EXPECT(fd.chmod(0200u));
    FileDescriptorStat writableStat;
    SC_TEST_EXPECT(fd.stat(writableStat));
    SC_TEST_EXPECT((writableStat.windows.attributes & FILE_ATTRIBUTE_READONLY) == 0);

    const ResultFile chownResult = fd.chown(123, 456);
    SC_TEST_EXPECT(chownResult.isError(FileError::OperationUnsupported));
    SC_TEST_EXPECT(chownResult.detail == FileErrorDetail::ChangeDescriptorOwnership);
#else
    int nativeHandle = -1;
    SC_TEST_EXPECT(fd.get(nativeHandle, Result::Error("native handle")));

    struct stat nativeStat;
    SC_TEST_EXPECT(::fstat(nativeHandle, &nativeStat) == 0);
    SC_TEST_EXPECT(statInfo.posix.mode == static_cast<uint32_t>(nativeStat.st_mode));
    SC_TEST_EXPECT(statInfo.posix.uid == static_cast<uint32_t>(nativeStat.st_uid));
    SC_TEST_EXPECT(statInfo.posix.gid == static_cast<uint32_t>(nativeStat.st_gid));

    SC_TEST_EXPECT(fd.chmod(0640u));
    FileDescriptorStat chmodStat;
    SC_TEST_EXPECT(fd.stat(chmodStat));
    SC_TEST_EXPECT((chmodStat.posix.mode & 0777u) == 0640u);

    SC_TEST_EXPECT(fd.chown(chmodStat.posix.uid, chmodStat.posix.gid));
    FileDescriptorStat afterChownStat;
    SC_TEST_EXPECT(fd.stat(afterChownStat));
    SC_TEST_EXPECT(afterChownStat.posix.uid == chmodStat.posix.uid);
    SC_TEST_EXPECT(afterChownStat.posix.gid == chmodStat.posix.gid);
#endif

    SC_TEST_EXPECT(fd.truncate(3));
    FileDescriptorStat truncatedStat;
    SC_TEST_EXPECT(fd.stat(truncatedStat));
    SC_TEST_EXPECT(truncatedStat.fileSize == 3);

    SC_TEST_EXPECT(fd.seek(FileDescriptor::SeekStart, 0));
    char       truncatedContent[8] = {0};
    Span<char> truncatedRead;
    SC_TEST_EXPECT(fd.read({truncatedContent, sizeof(truncatedContent)}, truncatedRead));
    SC_TEST_EXPECT(truncatedRead.sizeInBytes() == 3);
    SC_TEST_EXPECT(truncatedContent[0] == 'a');
    SC_TEST_EXPECT(truncatedContent[1] == 'b');
    SC_TEST_EXPECT(truncatedContent[2] == 'c');

    SC_TEST_EXPECT(fd.truncate(8));
    FileDescriptorStat expandedStat;
    SC_TEST_EXPECT(fd.stat(expandedStat));
    SC_TEST_EXPECT(expandedStat.fileSize == 8);

    SC_TEST_EXPECT(fd.seek(FileDescriptor::SeekStart, 0));
    char       expandedContent[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    Span<char> expandedRead;
    SC_TEST_EXPECT(fd.readUntilFullOrEOF({expandedContent, sizeof(expandedContent)}, expandedRead));
    SC_TEST_EXPECT(expandedRead.sizeInBytes() == 8);
    SC_TEST_EXPECT(expandedContent[0] == 'a');
    SC_TEST_EXPECT(expandedContent[1] == 'b');
    SC_TEST_EXPECT(expandedContent[2] == 'c');
    SC_TEST_EXPECT(expandedContent[3] == 0);
    SC_TEST_EXPECT(expandedContent[4] == 0);
    SC_TEST_EXPECT(expandedContent[5] == 0);
    SC_TEST_EXPECT(expandedContent[6] == 0);
    SC_TEST_EXPECT(expandedContent[7] == 0);

    SC_TEST_EXPECT(fd.close());
    SC_TEST_EXPECT(fs.removeFile(filePath.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(dirPath.view()));
}

void SC::FileTest::testDescriptorInvalidHandle()
{
    FileDescriptor     fd;
    FileDescriptorStat statInfo;

    SC_TEST_EXPECT(not fd.stat(statInfo));
    SC_TEST_EXPECT(not fd.chmod(0644u));
    SC_TEST_EXPECT(not fd.chown(0, 0));
    SC_TEST_EXPECT(not fd.sync());
    SC_TEST_EXPECT(not fd.syncData());
    SC_TEST_EXPECT(not fd.truncate(1));
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
