// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FileSystemIterator/FileSystemIterator.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemIterator/FileSystemIteratorErrorFormatter.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

#include <string.h>
namespace SC
{
struct FileSystemIteratorTest;
}

struct SC::FileSystemIteratorTest : public SC::TestCase
{
    FileSystemIteratorTest(SC::TestReport& report) : TestCase(report, "FileSystemIteratorTest")
    {
        using namespace SC;
        if (test_section("recursive manual"))
        {
            walkRecursiveManual();
        }
        if (test_section("recursive"))
        {
            walkRecursive();
        }
        if (test_section("not enough"))
        {
            walkNotEnough();
        }
        if (test_section("completion and sticky errors"))
        {
            completionAndStickyErrors();
        }
        if (test_section("structured errors and formatter"))
        {
            structuredErrorsAndFormatter();
        }
#if SC_PLATFORM_WINDOWS
        if (test_section("prefixed input logical output"))
        {
            prefixedInputLogicalOutput();
        }
#endif
    };
    inline void walkRecursiveManual();
    inline void walkRecursive();
    inline void walkNotEnough();
    inline void completionAndStickyErrors();
    inline void structuredErrorsAndFormatter();
#if SC_PLATFORM_WINDOWS
    inline void prefixedInputLogicalOutput();
#endif
};

void SC::FileSystemIteratorTest::walkRecursive()
{
    //! [walkRecursiveSnippet]
    FileSystemIterator::FolderState entries[16];

    FileSystemIterator fsIterator;
    fsIterator.options.recursive = true;
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory.view(), entries));
    while (fsIterator.enumerateNext())
    {
        report.console.printLine(fsIterator.get().path);
    }
    SC_TEST_EXPECT(fsIterator.checkErrors());
    //! [walkRecursiveSnippet]
}

void SC::FileSystemIteratorTest::walkNotEnough()
{
    //! [walkNotEnoughSnippet]
    // Test that unsufficient number of folderStates leads to failure to iterate
    // One FolderState is needed for current directory, plus one for each level
    // of recursion done when iterating.

    FileSystemIterator::FolderState folderStates[1];

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectory("test"));
    FileSystemIterator fsIterator;
    fsIterator.options.recursive = true;
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory.view(), folderStates));
    while (fsIterator.enumerateNext())
    {
        report.console.printLine(fsIterator.get().path);
    }
    const ResultFileSystemIterator traversalResult = fsIterator.checkErrors();
    SC_TEST_EXPECT(traversalResult.isError(FileSystemIteratorError::RecursionLimitExceeded));
    SC_TEST_EXPECT(traversalResult.depth == 1);
    SC_TEST_EXPECT(traversalResult.detail == FileSystemIteratorErrorDetail::PushRecursionState);
    SC_TEST_EXPECT(fsIterator.checkErrors().isError(FileSystemIteratorError::RecursionLimitExceeded));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("test"));
    //! [walkNotEnoughSnippet]
}

void SC::FileSystemIteratorTest::completionAndStickyErrors()
{
    FileSystemIterator beforeInit;
    SC_TEST_EXPECT(not beforeInit.enumerateNext());
    SC_TEST_EXPECT(beforeInit.checkErrors().isError(FileSystemIteratorError::NotInitialized));
    SC_TEST_EXPECT(not beforeInit.enumerateNext());
    SC_TEST_EXPECT(beforeInit.checkErrors().isError(FileSystemIteratorError::NotInitialized));

    FileSystemIterator             failedInit;
    const ResultFileSystemIterator missing = failedInit.init("FileSystemIteratorMissingDirectory", {});
    SC_TEST_EXPECT(missing.isError(FileSystemIteratorError::RecursionLimitExceeded));
    SC_TEST_EXPECT(failedInit.checkErrors().isError(FileSystemIteratorError::RecursionLimitExceeded));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    constexpr StringView emptyDirectory = "FileSystemIteratorEmpty";
    (void)fs.removeEmptyDirectory(emptyDirectory);
    SC_TEST_EXPECT(fs.makeDirectory(emptyDirectory));

    StringPath emptyPath;
    SC_TEST_EXPECT(Path::join(emptyPath, {report.applicationRootDirectory.view(), emptyDirectory}));
    FileSystemIterator::FolderState entries[1];

    StringPath missingPath;
    SC_TEST_EXPECT(
        Path::join(missingPath, {report.applicationRootDirectory.view(), "FileSystemIteratorMissingDirectory"}));
    const ResultFileSystemIterator openFailure = failedInit.init(missingPath.view(), entries);
    SC_TEST_EXPECT(openFailure.isError(FileSystemIteratorError::OpenDirectoryFailed));
    SC_TEST_EXPECT(openFailure.nativeError != 0);
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(openFailure.detail == FileSystemIteratorErrorDetail::WindowsFindFirstFile);
#else
    SC_TEST_EXPECT(openFailure.detail == FileSystemIteratorErrorDetail::PosixOpen);
#endif
    SC_TEST_EXPECT(failedInit.checkErrors().isError(FileSystemIteratorError::OpenDirectoryFailed));

    SC_TEST_EXPECT(failedInit.init(emptyPath.view(), entries));
    SC_TEST_EXPECT(not failedInit.enumerateNext());
    SC_TEST_EXPECT(failedInit.checkErrors());
    SC_TEST_EXPECT(not failedInit.enumerateNext());
    SC_TEST_EXPECT(failedInit.checkErrors());

    FileSystemIterator invalidRecursion;
    invalidRecursion.options.recursive = true;
    SC_TEST_EXPECT(invalidRecursion.init(emptyPath.view(), entries));
    const ResultFileSystemIterator recursionResult = invalidRecursion.recurseSubdirectory();
    SC_TEST_EXPECT(recursionResult.isError(FileSystemIteratorError::InvalidRecursionState));
    SC_TEST_EXPECT(invalidRecursion.checkErrors().isError(FileSystemIteratorError::InvalidRecursionState));

    SC_TEST_EXPECT(fs.removeEmptyDirectory(emptyDirectory));
}

void SC::FileSystemIteratorTest::structuredErrorsAndFormatter()
{
    static_assert(sizeof(void*) != 8 or sizeof(ResultFileSystemIterator) == 24,
                  "The migration bridge temporarily expands ResultFileSystemIterator");
    static_assert(sizeof(void*) != 8 or sizeof(ResultFileSystemIterator) == sizeof(Result) + 8,
                  "ResultFileSystemIterator detail must fit the final 16-byte target");
    static_assert(__is_standard_layout(ResultFileSystemIterator),
                  "ResultFileSystemIterator must remain standard-layout");
    static_assert(TypeTraits::IsTriviallyCopyable<ResultFileSystemIterator>::value,
                  "ResultFileSystemIterator must remain trivially copyable");

    constexpr char expected[] = "Failed to open directory (detail: POSIX open directory, native error: 2, depth: 3)";
    ResultFileSystemIterator detailed(FileSystemIteratorError::OpenDirectoryFailed,
                                      FileSystemIteratorErrorDetail::PosixOpen, 2, 3);
    char                     message[sizeof(expected)];
    ResultErrorFormat        formatted = formatFileSystemIteratorError(detailed, message);
    SC_TEST_EXPECT(formatted);
    SC_TEST_EXPECT(formatted.requiredCapacity == sizeof(expected));
    SC_TEST_EXPECT(::memcmp(message, expected, sizeof(expected)) == 0);

    const Result plain = detailed;
    SC_TEST_EXPECT(plain.isError(FileSystemIteratorResultCategory, FileSystemIteratorError::OpenDirectoryFailed));
    SC_TEST_EXPECT(detailed.detail == FileSystemIteratorErrorDetail::PosixOpen);

    constexpr char           expectedRoot[] = "Directory path is too long (detail: build traversal path, depth: 0)";
    ResultFileSystemIterator rootFailure(FileSystemIteratorError::PathTooLong, FileSystemIteratorErrorDetail::BuildPath,
                                         0, 0);
    char                     rootMessage[sizeof(expectedRoot)];
    formatted = formatFileSystemIteratorError(rootFailure, rootMessage);
    SC_TEST_EXPECT(formatted);
    SC_TEST_EXPECT(formatted.requiredCapacity == sizeof(expectedRoot));
    SC_TEST_EXPECT(::memcmp(rootMessage, expectedRoot, sizeof(expectedRoot)) == 0);

    ResultFileSystemIterator unknownDetail(FileSystemIteratorError::PathTooLong,
                                           static_cast<FileSystemIteratorErrorDetail>(999), 0, 0);
    formatted = formatFileSystemIteratorError(unknownDetail, rootMessage);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::UnknownError);
    SC_TEST_EXPECT(rootMessage[0] == '\0');

    const ResultFileSystemIterator legacy(Result::Error("legacy iterator result"));
    SC_TEST_EXPECT(not legacy);
    SC_TEST_EXPECT(legacy.nativeError == 0);
    SC_TEST_EXPECT(legacy.depth == 0);
    SC_TEST_EXPECT(legacy.detail == FileSystemIteratorErrorDetail::None);
    SC_TEST_EXPECT(legacy.result.hasLegacyError());
    formatted = formatFileSystemIteratorError(legacy, message);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::ForeignCategory);

    formatted = formatFileSystemIteratorError(Result(true), message);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::NotAnError);
    formatted = formatFileSystemIteratorError(Result::Error(ResultCategory(1234), 1), message);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::ForeignCategory);
    formatted = formatFileSystemIteratorError(Result::Error(FileSystemIteratorResultCategory, 9999), message);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::UnknownError);
}
void SC::FileSystemIteratorTest::walkRecursiveManual()
{
    //! [walkRecursiveManualSnippet]
    FileSystemIterator::FolderState entries[16];

    FileSystemIterator fsIterator;
    fsIterator.options.recursive = false; // As we manually call recurseSubdirectory
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory.view(), entries));
    while (fsIterator.enumerateNext())
    {
        const FileSystemIterator::Entry& entry = fsIterator.get();
        report.console.printLine(entry.path);
        // Only recurse directories not ending with "someExcludePattern"
        if (entry.isDirectory() and not StringView(entry.name).endsWith("someExcludePattern"))
        {
            SC_TEST_EXPECT(fsIterator.recurseSubdirectory());
        }
    }
    SC_TEST_EXPECT(fsIterator.checkErrors());
    //! [walkRecursiveManualSnippet]
}

#if SC_PLATFORM_WINDOWS
void SC::FileSystemIteratorTest::prefixedInputLogicalOutput()
{
    FileSystemIterator::FolderState entries[16];

    StringPath prefixedRoot;
    SC_TEST_EXPECT(prefixedRoot.assign("\\\\?\\"_a8));
    SC_TEST_EXPECT(prefixedRoot.append(report.applicationRootDirectory.view()));

    FileSystemIterator fsIterator;
    SC_TEST_EXPECT(fsIterator.init(prefixedRoot.view(), entries));
    SC_TEST_EXPECT(fsIterator.enumerateNext());
    SC_TEST_EXPECT(not StringView(fsIterator.get().path).startsWith("\\\\?\\"_a8));
    SC_TEST_EXPECT(fsIterator.checkErrors());
}
#endif

namespace SC
{
void runFileSystemIteratorTest(SC::TestReport& report) { FileSystemIteratorTest test(report); }
} // namespace SC
