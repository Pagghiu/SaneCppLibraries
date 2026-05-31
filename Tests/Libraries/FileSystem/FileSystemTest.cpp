// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

namespace SC
{
namespace FileSystemTestWindowsDetail
{
#include "Libraries/Common/WindowsPath.inl"
}
} // namespace SC
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace SC
{
struct FileSystemTest;
}

struct SC::FileSystemTest : public SC::TestCase
{
    FileSystemTest(SC::TestReport& report) : TestCase(report, "FileSystemTest")
    {
        using namespace SC;
        if (test_section("formatError"))
        {
            formatError();
        }
        if (test_section("makeDirectory / isDirectory / removeEmptyDirectory"))
        {
            makeRemoveIsDirectory();
        }
        if (test_section("makeDirectoryRecursive"))
        {
            makeDirectoryRecursive();
        }
        if (test_section("write / read / removeFile"))
        {
            writeReadRemoveFile();
        }
        if (test_section("copyFile/existsAndIsFile"))
        {
            copyExistsFile();
        }
        if (test_section("Copy Directory (recursive)"))
        {
            copyDirectoryRecursive();
        }
        if (test_section("Remove Directory (recursive)"))
        {
            removeDirectoryRecursive();
        }
        if (test_section("Rename File"))
        {
            renameFile();
        }
        if (test_section("Rename Directory"))
        {
            renameDirectory();
        }
        if (test_section("Symbolic Link"))
        {
            symbolicLink();
        }
        if (test_section("Hard Link"))
        {
            hardLink();
        }
        if (test_section("Access"))
        {
            access();
        }
        if (test_section("Move Directory"))
        {
            moveDirectory();
        }
        if (test_section("stat / lstat"))
        {
            statAndLStat();
        }
        if (test_section("permissions"))
        {
            permissions();
        }
        if (test_section("executableFile / applicationRootDirectory"))
        {
            StringPath stringPath;
            report.console.print("executableFile=\"{}\"\n", FileSystem::Operations::getExecutablePath(stringPath));
            report.console.print("applicationRootDirectory=\"{}\"\n",
                                 FileSystem::Operations::getApplicationRootDirectory(stringPath));
        }
        if (test_section("getCurrentWorkingDirectory"))
        {
            StringPath stringPath;
            report.console.print("currentWorkingDirectory=\"{}\"\n",
                                 FileSystem::Operations::getCurrentWorkingDirectory(stringPath));
        }
#if SC_PLATFORM_WINDOWS
        if (test_section("prefixed path input"))
        {
            prefixedPathInput();
        }
        if (test_section("windows long-path helper"))
        {
            windowsLongPathHelper();
        }
#endif
    }

    void formatError();
    void makeRemoveIsDirectory();
    void makeDirectoryRecursive();
    void writeReadRemoveFile();
    void copyExistsFile();
    void copyDirectoryRecursive();
    void removeDirectoryRecursive();
    void renameFile();
    void renameDirectory();
    void symbolicLink();
    void hardLink();
    void access();
    void moveDirectory();
    void statAndLStat();
    void permissions();
    void prefixedPathInput();
#if SC_PLATFORM_WINDOWS
    void windowsLongPathHelper();
#endif
    void snippet();
};

void SC::FileSystemTest::formatError()
{
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    fs.preciseErrorMessages = true;

    Result res = fs.removeEmptyDirectory("randomNonExistingDirectory");
    SC_TEST_EXPECT(not res);
    fs.preciseErrorMessages = false;

    res = fs.removeEmptyDirectory("randomNonExistingDirectory");
    SC_TEST_EXPECT(not res);
}

void SC::FileSystemTest::makeRemoveIsDirectory()
{
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    // Make directory and check that it's a directory and not a file
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("Test0"));
    SC_TEST_EXPECT(fs.makeDirectory("Test0"));
    SC_TEST_EXPECT(fs.exists("Test0"));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("Test0"));
    SC_TEST_EXPECT(not fs.existsAndIsFile("Test0"));

    // Create two additional directories and check they exist and are actual directories
    SC_TEST_EXPECT(fs.makeDirectories({"Test1", "Test2"}));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("Test1"));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("Test2"));

    // Remove all directories
    SC_TEST_EXPECT(fs.removeEmptyDirectory("Test0"));
    SC_TEST_EXPECT(fs.removeEmptyDirectories({"Test1", "Test2"}));

    // Check that all directories have been removed
    SC_TEST_EXPECT(not fs.exists("Test0"));
    SC_TEST_EXPECT(not fs.existsAndIsFile("Test0"));
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("Test0"));
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("Test1"));
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("Test2"));
}

void SC::FileSystemTest::makeDirectoryRecursive()
{
    //! [makeDirectoryRecursive]
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    // Create a directory with 2 levels of nesting
    SC_TEST_EXPECT(fs.makeDirectoryRecursive("Test3/Subdir"));

    // Check that both levels have been created
    SC_TEST_EXPECT(fs.existsAndIsDirectory("Test3"));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("Test3/Subdir"));

    // Remove both levels of directory
    SC_TEST_EXPECT(fs.removeEmptyDirectory("Test3/Subdir"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("Test3"));
    //! [makeDirectoryRecursive]
}

void SC::FileSystemTest::writeReadRemoveFile()
{
    //! [writeReadRemoveFileSnippet]
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    StringView content = "ASDF content";

    // Check that file doesn't exists before write-ing it and then check that it exist
    SC_TEST_EXPECT(not fs.exists("file.txt"));
    SC_TEST_EXPECT(fs.writeString("file.txt", content));
    SC_TEST_EXPECT(fs.existsAndIsFile("file.txt"));

    // Read the file and check its content
    String newString = StringEncoding::Ascii;
    SC_TEST_EXPECT(fs.read("file.txt", newString));
    SC_TEST_EXPECT(newString.view() == content);

    // Remove all files created by the test
    SC_TEST_EXPECT(fs.removeFile("file.txt"));
    SC_TEST_EXPECT(not fs.exists("file.txt"));
    //! [writeReadRemoveFileSnippet]
}

void SC::FileSystemTest::copyExistsFile()
{
    //! [copyExistsFileSnippet]
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    // Create a File names 'sourceFile.txt'
    StringView contentSource = "this is some content";
    SC_TEST_EXPECT(not fs.exists("sourceFile.txt"));
    SC_TEST_EXPECT(fs.writeString("sourceFile.txt", contentSource));

    // Check that 'sourceFile.txt' exist, but not 'destinationFile.txt'
    SC_TEST_EXPECT(fs.existsAndIsFile("sourceFile.txt"));
    SC_TEST_EXPECT(not fs.exists("destinationFile.txt"));

    // Ask to copy sourceFile.txt to destinationFile.txt (eventually overwriting, but without cloning)
    SC_TEST_EXPECT(fs.copyFile("sourceFile.txt", "destinationFile.txt",
                               FileSystem::CopyFlags().setOverwrite(true).setUseCloneIfSupported(false)));

    // Now read the destinationFile.txt content and check if it's the same as source
    String content = StringEncoding::Ascii;
    SC_TEST_EXPECT(fs.read("destinationFile.txt", content));
    SC_TEST_EXPECT(content.view() == contentSource);

    // Copy again sourceFile.txt to destinationFile.txt but using clone this time
    SC_TEST_EXPECT(fs.copyFile("sourceFile.txt", "destinationFile.txt",
                               FileSystem::CopyFlags().setOverwrite(true).setUseCloneIfSupported(true)));

    // Check again if file exists and its content
    SC_TEST_EXPECT(fs.existsAndIsFile("destinationFile.txt"));
    SC_TEST_EXPECT(fs.read("destinationFile.txt", content));
    SC_TEST_EXPECT(content.view() == contentSource);

    // Remove all files created by the test
    SC_TEST_EXPECT(fs.removeFiles({"sourceFile.txt", "destinationFile.txt"}));
    SC_TEST_EXPECT(not fs.exists("sourceFile.txt"));
    SC_TEST_EXPECT(not fs.exists("destinationFile.txt"));
    //! [copyExistsFileSnippet]
}

void SC::FileSystemTest::copyDirectoryRecursive()
{
    //! [copyDirectoryRecursiveSnippet]
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    // Create a nested directory structure with some files too
    SC_TEST_EXPECT(fs.makeDirectory("copyDirectory"));
    SC_TEST_EXPECT(fs.write("copyDirectory/testFile.txt", "asdf"));
    SC_TEST_EXPECT(fs.existsAndIsFile("copyDirectory/testFile.txt"));
    SC_TEST_EXPECT(fs.makeDirectory("copyDirectory/subdirectory"));
    SC_TEST_EXPECT(fs.write("copyDirectory/subdirectory/testFile.txt", "asdf"));

    // Copy the directory (recursively)
    SC_TEST_EXPECT(fs.copyDirectory("copyDirectory", "COPY_copyDirectory"));

    // Check that file exists in the new copied directory
    SC_TEST_EXPECT(fs.existsAndIsFile("COPY_copyDirectory/testFile.txt"));
    SC_TEST_EXPECT(fs.existsAndIsFile("COPY_copyDirectory/subdirectory/testFile.txt"));

    // Copying again fails (because we're not overwriting)
    SC_TEST_EXPECT(not fs.copyDirectory("copyDirectory", "COPY_copyDirectory"));

    // Try copying again but now we ask to overwrite destination
    SC_TEST_EXPECT(fs.copyDirectory("copyDirectory", "COPY_copyDirectory", FileSystem::CopyFlags().setOverwrite(true)));

    // Remove all files created by the test
    SC_TEST_EXPECT(fs.removeFile("copyDirectory/testFile.txt"));
    SC_TEST_EXPECT(fs.removeFile("copyDirectory/subdirectory/testFile.txt"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("copyDirectory/subdirectory"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("copyDirectory"));
    SC_TEST_EXPECT(fs.removeFile("COPY_copyDirectory/testFile.txt"));
    SC_TEST_EXPECT(fs.removeFile("COPY_copyDirectory/subdirectory/testFile.txt"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("COPY_copyDirectory/subdirectory"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("COPY_copyDirectory"));
    //! [copyDirectoryRecursiveSnippet]
}

void SC::FileSystemTest::removeDirectoryRecursive()
{
    //! [removeDirectoryRecursiveSnippet]
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    // Create a nested directory structure with some files too
    SC_TEST_EXPECT(fs.makeDirectory("removeDirectoryTest"));
    SC_TEST_EXPECT(fs.write("removeDirectoryTest/testFile.txt", "asdf"));
    SC_TEST_EXPECT(fs.makeDirectory("removeDirectoryTest/another"));
    SC_TEST_EXPECT(fs.write("removeDirectoryTest/another/yeah.txt", "asdf"));

    // Remove the entire tree of directories
    SC_TEST_EXPECT(fs.removeDirectoryRecursive("removeDirectoryTest"));

    // Check that all files and directories have been removed
    SC_TEST_EXPECT(not fs.existsAndIsFile("removeDirectoryTest/testFile.txt"));
    SC_TEST_EXPECT(not fs.existsAndIsFile("removeDirectoryTest/another/yeah.txt"));
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("removeDirectoryTest/another"));
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("removeDirectoryTest"));
    //! [removeDirectoryRecursiveSnippet]
}

void SC::FileSystemTest::renameFile()
{
    //! [renameFileSnippet]
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    // Create a file and check that it exists
    SC_TEST_EXPECT(fs.writeString("renameTest.txt", "asdf"));
    SC_TEST_EXPECT(fs.existsAndIsFile("renameTest.txt"));

    // Rename the file
    SC_TEST_EXPECT(fs.rename("renameTest.txt", "renameTest2.txt"));

    // Check that the file has been renamed
    SC_TEST_EXPECT(fs.existsAndIsFile("renameTest2.txt"));
    SC_TEST_EXPECT(not fs.existsAndIsFile("renameTest.txt"));

    // Rename the file again
    SC_TEST_EXPECT(fs.rename("renameTest2.txt", "renameTest.txt"));

    // Check that the file has been renamed
    SC_TEST_EXPECT(fs.existsAndIsFile("renameTest.txt"));
    SC_TEST_EXPECT(not fs.existsAndIsFile("renameTest2.txt"));

    // Remove all files created by the test
    SC_TEST_EXPECT(fs.removeFile("renameTest.txt"));
    //! [renameFileSnippet]
}

void SC::FileSystemTest::renameDirectory()
{
    //! [renameDirectorySnippet]
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    // Create a directory and check that it exists
    SC_TEST_EXPECT(fs.makeDirectory("renameDirectoryTest"));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("renameDirectoryTest"));
    // Create a file in the directory
    SC_TEST_EXPECT(fs.writeString("renameDirectoryTest/testFile.txt", "asdf"));
    SC_TEST_EXPECT(fs.existsAndIsFile("renameDirectoryTest/testFile.txt"));
    // Create a subdirectory in the directory
    SC_TEST_EXPECT(fs.makeDirectory("renameDirectoryTest/subdirectory"));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("renameDirectoryTest/subdirectory"));

    // Create a file in the subdirectory
    SC_TEST_EXPECT(fs.writeString("renameDirectoryTest/subdirectory/testFile.txt", "asdf"));
    SC_TEST_EXPECT(fs.existsAndIsFile("renameDirectoryTest/subdirectory/testFile.txt"));

    // Rename the directory
    SC_TEST_EXPECT(fs.rename("renameDirectoryTest", "renameDirectoryTest2"));

    // Check that the directory has been renamed
    SC_TEST_EXPECT(fs.existsAndIsDirectory("renameDirectoryTest2"));
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("renameDirectoryTest"));

    // Check that the file in the directory has been renamed
    SC_TEST_EXPECT(fs.existsAndIsFile("renameDirectoryTest2/testFile.txt"));
    SC_TEST_EXPECT(not fs.existsAndIsFile("renameDirectoryTest/testFile.txt"));
    // Check that the file in the subdirectory has been renamed
    SC_TEST_EXPECT(fs.existsAndIsFile("renameDirectoryTest2/subdirectory/testFile.txt"));
    SC_TEST_EXPECT(not fs.existsAndIsFile("renameDirectoryTest/subdirectory/testFile.txt"));

    // Remove all directories created by the test
    SC_TEST_EXPECT(fs.removeDirectoryRecursive("renameDirectoryTest2"));
    //! [renameDirectorySnippet]
}

void SC::FileSystemTest::symbolicLink()
{
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    SC_TEST_EXPECT(fs.writeString("symlinkSource.txt", "symlink-content"));
    Result createLinkResult = fs.createSymbolicLink("symlinkSource.txt", "symlinkTarget.txt");
#if SC_PLATFORM_WINDOWS
    if (not createLinkResult)
    {
        report.console.print("Skipping symbolic link assertions because symlink creation is not available\n");
        SC_TEST_EXPECT(fs.removeFile("symlinkSource.txt"));
        return;
    }
#else
    SC_TEST_EXPECT(createLinkResult);
#endif
    SC_TEST_EXPECT(fs.existsAndIsLink("symlinkTarget.txt"));

    StringPath linkTarget;
    SC_TEST_EXPECT(fs.readSymbolicLink("symlinkTarget.txt", linkTarget));

    StringPath expectedTarget;
    SC_TEST_EXPECT(expectedTarget.assign(report.applicationRootDirectory.view()));
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(expectedTarget.append("\\symlinkSource.txt"));
#else
    SC_TEST_EXPECT(expectedTarget.append("/symlinkSource.txt"));
#endif
    SC_TEST_EXPECT(linkTarget.view() == expectedTarget.view());

    String content = StringEncoding::Ascii;
    SC_TEST_EXPECT(fs.read("symlinkTarget.txt", content));
    SC_TEST_EXPECT(content.view() == "symlink-content");

    SC_TEST_EXPECT(fs.removeLinkIfExists("symlinkTarget.txt"));
    SC_TEST_EXPECT(not fs.exists("symlinkTarget.txt"));
    SC_TEST_EXPECT(fs.removeFile("symlinkSource.txt"));

    SC_TEST_EXPECT(fs.makeDirectory("symlinkSourceDirectory"));
    SC_TEST_EXPECT(fs.writeString("symlinkSourceDirectory/nested.txt", "nested-content"));
    Result createDirectoryLinkResult = fs.createSymbolicLink("symlinkSourceDirectory", "symlinkTargetDirectory");
#if SC_PLATFORM_WINDOWS
    if (createDirectoryLinkResult)
#else
    SC_TEST_EXPECT(createDirectoryLinkResult);
    if (createDirectoryLinkResult)
#endif
    {
        SC_TEST_EXPECT(fs.existsAndIsLink("symlinkTargetDirectory"));
        SC_TEST_EXPECT(fs.existsAndIsDirectory("symlinkTargetDirectory"));
        SC_TEST_EXPECT(fs.removeLinkIfExists("symlinkTargetDirectory"));
        SC_TEST_EXPECT(not fs.exists("symlinkTargetDirectory"));
    }
    SC_TEST_EXPECT(fs.existsAndIsDirectory("symlinkSourceDirectory"));
    SC_TEST_EXPECT(fs.existsAndIsFile("symlinkSourceDirectory/nested.txt"));
    SC_TEST_EXPECT(fs.removeDirectoryRecursive("symlinkSourceDirectory"));
}

void SC::FileSystemTest::hardLink()
{
    FileSystem fs;
    StringPath tempDirectory;
#if SC_PLATFORM_WINDOWS
    const DWORD tempLength =
        ::GetTempPathW(static_cast<DWORD>(StringPath::StorageCapacity), tempDirectory.writableSpan().data());
    SC_TEST_EXPECT(tempLength > 0 and tempLength < StringPath::StorageCapacity);
    size_t trimmedLength = static_cast<size_t>(tempLength);
    if (trimmedLength > 0 and tempDirectory.writableSpan().data()[trimmedLength - 1] == L'\\')
    {
        trimmedLength -= 1;
    }
    SC_TEST_EXPECT(tempDirectory.resize(trimmedLength));
#else
    SC_TEST_EXPECT(tempDirectory.assign("/tmp"));
#endif

    StringPath hardLinkDirectory = tempDirectory;
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(hardLinkDirectory.append("\\FileSystemHardLinkTest"));
#else
    SC_TEST_EXPECT(hardLinkDirectory.append("/FileSystemHardLinkTest"));
#endif

    SC_TEST_EXPECT(fs.init(tempDirectory.view()));
    if (fs.existsAndIsDirectory(hardLinkDirectory.view()))
    {
        SC_TEST_EXPECT(fs.removeDirectoryRecursive(hardLinkDirectory.view()));
    }
    SC_TEST_EXPECT(fs.makeDirectory(hardLinkDirectory.view()));
    SC_TEST_EXPECT(fs.changeDirectory(hardLinkDirectory.view()));

    SC_TEST_EXPECT(fs.writeString("hardLinkSource.txt", "first"));
    FileSystem::FileStat sourceStat;
    SC_TEST_EXPECT(fs.stat("hardLinkSource.txt", sourceStat));
    SC_TEST_EXPECT(sourceStat.entryType == FileSystemEntryType::File);
    SC_TEST_EXPECT(sourceStat.hardLinkCount >= 1);
    SC_TEST_EXPECT(fs.createHardLink("hardLinkSource.txt", "hardLinkTarget.txt"));
    SC_TEST_EXPECT(fs.existsAndIsFile("hardLinkTarget.txt"));

    FileSystem::FileStat linkedStat;
    SC_TEST_EXPECT(fs.stat("hardLinkTarget.txt", linkedStat));
    SC_TEST_EXPECT(linkedStat.entryType == FileSystemEntryType::File);
    SC_TEST_EXPECT(linkedStat.hardLinkCount >= 2);

    SC_TEST_EXPECT(fs.writeString("hardLinkSource.txt", "second"));

    String content = StringEncoding::Ascii;
    SC_TEST_EXPECT(fs.read("hardLinkTarget.txt", content));
    SC_TEST_EXPECT(content.view() == "second");

    SC_TEST_EXPECT(fs.removeFiles({"hardLinkSource.txt", "hardLinkTarget.txt"}));
    SC_TEST_EXPECT(fs.changeDirectory(tempDirectory.view()));
    SC_TEST_EXPECT(fs.removeDirectoryRecursive(hardLinkDirectory.view()));
}

void SC::FileSystemTest::access()
{
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    SC_TEST_EXPECT(fs.writeString("accessTest.txt", "asdf"));

    SC_TEST_EXPECT(fs.canAccess("accessTest.txt"));
    SC_TEST_EXPECT(fs.canAccess("accessTest.txt", FileSystem::AccessMode::Read));
    SC_TEST_EXPECT(fs.canAccess("accessTest.txt", FileSystem::AccessMode::Write));
    SC_TEST_EXPECT(not fs.canAccess("accessMissing.txt"));
    SC_TEST_EXPECT(not fs.canAccess("accessMissing.txt", FileSystem::AccessMode::Read));

    SC_TEST_EXPECT(fs.removeFile("accessTest.txt"));
}

void SC::FileSystemTest::moveDirectory()
{
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    SC_TEST_EXPECT(fs.makeDirectory("moveDirectorySource"));
    SC_TEST_EXPECT(fs.writeString("moveDirectorySource/file.txt", "asdf"));
    SC_TEST_EXPECT(fs.makeDirectory("moveDirectorySource/subdir"));
    SC_TEST_EXPECT(fs.writeString("moveDirectorySource/subdir/child.txt", "qwer"));

    SC_TEST_EXPECT(fs.moveDirectory("moveDirectorySource", "moveDirectoryDestination"));

    SC_TEST_EXPECT(not fs.existsAndIsDirectory("moveDirectorySource"));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("moveDirectoryDestination"));
    SC_TEST_EXPECT(fs.existsAndIsFile("moveDirectoryDestination/file.txt"));
    SC_TEST_EXPECT(fs.existsAndIsFile("moveDirectoryDestination/subdir/child.txt"));

    SC_TEST_EXPECT(fs.removeDirectoryRecursive("moveDirectoryDestination"));
}

void SC::FileSystemTest::statAndLStat()
{
    FileSystem fs;
    StringPath tempDirectory;
#if SC_PLATFORM_WINDOWS
    const DWORD tempLength =
        ::GetTempPathW(static_cast<DWORD>(StringPath::StorageCapacity), tempDirectory.writableSpan().data());
    SC_TEST_EXPECT(tempLength > 0 and tempLength < StringPath::StorageCapacity);
    size_t trimmedLength = static_cast<size_t>(tempLength);
    if (trimmedLength > 0 and tempDirectory.writableSpan().data()[trimmedLength - 1] == L'\\')
    {
        trimmedLength -= 1;
    }
    SC_TEST_EXPECT(tempDirectory.resize(trimmedLength));
#else
    SC_TEST_EXPECT(tempDirectory.assign("/tmp"));
#endif

    StringPath statDirectoryRoot = tempDirectory;
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(statDirectoryRoot.append("\\FileSystemStatTest"));
#else
    SC_TEST_EXPECT(statDirectoryRoot.append("/FileSystemStatTest"));
#endif

    SC_TEST_EXPECT(fs.init(tempDirectory.view()));
    if (fs.existsAndIsDirectory(statDirectoryRoot.view()))
    {
        SC_TEST_EXPECT(fs.removeDirectoryRecursive(statDirectoryRoot.view()));
    }
    SC_TEST_EXPECT(fs.makeDirectory(statDirectoryRoot.view()));
    SC_TEST_EXPECT(fs.changeDirectory(statDirectoryRoot.view()));

    SC_TEST_EXPECT(fs.writeString("statFile.txt", "stat-content"));
    SC_TEST_EXPECT(fs.makeDirectory("statDirectory"));

    FileSystem::FileStat statInfo;
    SC_TEST_EXPECT(fs.stat("statFile.txt", statInfo));
    SC_TEST_EXPECT(statInfo.entryType == FileSystemEntryType::File);
    SC_TEST_EXPECT(statInfo.fileSize == StringView("stat-content").sizeInBytes());
    SC_TEST_EXPECT(statInfo.modifiedTime.milliseconds > 0);
    SC_TEST_EXPECT(statInfo.accessedTime.milliseconds > 0);
    SC_TEST_EXPECT(statInfo.hardLinkCount >= 1);
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(statInfo.windows.attributes != 0);
    SC_TEST_EXPECT(statInfo.creationTime.milliseconds > 0);
#else
    StringPath statFilePath = statDirectoryRoot;
    SC_TEST_EXPECT(statFilePath.append("/statFile.txt"));
    struct stat nativeStat;
    SC_TEST_EXPECT(::stat(statFilePath.view().getNullTerminatedNative(), &nativeStat) == 0);
    SC_TEST_EXPECT(statInfo.posix.mode != 0);
    SC_TEST_EXPECT(statInfo.posix.uid == static_cast<SC::uint32_t>(nativeStat.st_uid));
    SC_TEST_EXPECT(statInfo.posix.gid == static_cast<SC::uint32_t>(nativeStat.st_gid));
    SC_TEST_EXPECT(statInfo.posix.inode == static_cast<SC::uint64_t>(nativeStat.st_ino));
#endif

    FileSystem::FileStat compatibilityInfo;
    SC_TEST_EXPECT(fs.getFileStat("statFile.txt", compatibilityInfo));
    SC_TEST_EXPECT(compatibilityInfo.fileSize == statInfo.fileSize);
    SC_TEST_EXPECT(compatibilityInfo.modifiedTime.milliseconds == statInfo.modifiedTime.milliseconds);

    FileSystem::FileStat directoryStat;
    SC_TEST_EXPECT(fs.stat("statDirectory", directoryStat));
    SC_TEST_EXPECT(directoryStat.entryType == FileSystemEntryType::Directory);

    Result createLinkResult = fs.createSymbolicLink("statFile.txt", "statLink.txt");
#if SC_PLATFORM_WINDOWS
    if (createLinkResult)
    {
#else
    SC_TEST_EXPECT(createLinkResult);
    {
#endif
        FileSystem::FileStat followedStat;
        FileSystem::FileStat linkStat;
        SC_TEST_EXPECT(fs.stat("statLink.txt", followedStat));
        SC_TEST_EXPECT(fs.lstat("statLink.txt", linkStat));
        SC_TEST_EXPECT(followedStat.entryType == FileSystemEntryType::File);
        SC_TEST_EXPECT(linkStat.entryType == FileSystemEntryType::SymbolicLink);
#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(linkStat.windows.attributes != 0);
#else
        SC_TEST_EXPECT(linkStat.posix.mode != 0);
        SC_TEST_EXPECT(linkStat.posix.inode != 0);
#endif
        SC_TEST_EXPECT(fs.removeLinkIfExists("statLink.txt"));
    }

    SC_TEST_EXPECT(fs.removeFile("statFile.txt"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("statDirectory"));
    SC_TEST_EXPECT(fs.changeDirectory(tempDirectory.view()));
    SC_TEST_EXPECT(fs.removeDirectoryRecursive(statDirectoryRoot.view()));
}

void SC::FileSystemTest::permissions()
{
    FileSystem fs;
    StringPath tempDirectory;
#if SC_PLATFORM_WINDOWS
    const DWORD tempLength =
        ::GetTempPathW(static_cast<DWORD>(StringPath::StorageCapacity), tempDirectory.writableSpan().data());
    SC_TEST_EXPECT(tempLength > 0 and tempLength < StringPath::StorageCapacity);
    size_t trimmedLength = static_cast<size_t>(tempLength);
    if (trimmedLength > 0 and tempDirectory.writableSpan().data()[trimmedLength - 1] == L'\\')
    {
        trimmedLength -= 1;
    }
    SC_TEST_EXPECT(tempDirectory.resize(trimmedLength));
#else
    SC_TEST_EXPECT(tempDirectory.assign("/tmp"));
#endif

    StringPath permissionsDirectoryRoot = tempDirectory;
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(permissionsDirectoryRoot.append("\\FileSystemPermissionsTest"));
#else
    SC_TEST_EXPECT(permissionsDirectoryRoot.append("/FileSystemPermissionsTest"));
#endif

    SC_TEST_EXPECT(fs.init(tempDirectory.view()));
    if (fs.existsAndIsDirectory(permissionsDirectoryRoot.view()))
    {
        SC_TEST_EXPECT(fs.removeDirectoryRecursive(permissionsDirectoryRoot.view()));
    }
    SC_TEST_EXPECT(fs.makeDirectory(permissionsDirectoryRoot.view()));
    SC_TEST_EXPECT(fs.changeDirectory(permissionsDirectoryRoot.view()));

    SC_TEST_EXPECT(fs.writeString("permissionsFile.txt", "permissions"));

    FileSystem::FileStat fileStatBefore;
    SC_TEST_EXPECT(fs.stat("permissionsFile.txt", fileStatBefore));

#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(fs.chmod("permissionsFile.txt", 0));
    FileSystem::FileStat readOnlyStat;
    SC_TEST_EXPECT(fs.stat("permissionsFile.txt", readOnlyStat));
    SC_TEST_EXPECT((readOnlyStat.windows.attributes & FILE_ATTRIBUTE_READONLY) != 0);

    SC_TEST_EXPECT(fs.chmod("permissionsFile.txt", 0200u));
    FileSystem::FileStat writableStat;
    SC_TEST_EXPECT(fs.stat("permissionsFile.txt", writableStat));
    SC_TEST_EXPECT((writableStat.windows.attributes & FILE_ATTRIBUTE_READONLY) == 0);

    SC_TEST_EXPECT(fs.chown("permissionsFile.txt", 123, 456));
    FileSystem::FileStat afterChownStat;
    SC_TEST_EXPECT(fs.stat("permissionsFile.txt", afterChownStat));
    SC_TEST_EXPECT(afterChownStat.windows.attributes == writableStat.windows.attributes);

    Result createLinkResult = fs.createSymbolicLink("permissionsFile.txt", "permissionsLink.txt");
    if (createLinkResult)
    {
        SC_TEST_EXPECT(fs.lchown("permissionsLink.txt", 123, 456));
        FileSystem::FileStat linkStat;
        SC_TEST_EXPECT(fs.lstat("permissionsLink.txt", linkStat));
        SC_TEST_EXPECT(linkStat.entryType == FileSystemEntryType::SymbolicLink);
        SC_TEST_EXPECT(not fs.lchmod("permissionsLink.txt", 0200u));
        SC_TEST_EXPECT(fs.removeLinkIfExists("permissionsLink.txt"));
    }
    else
    {
        report.console.print(
            "Skipping Windows lchown/lchmod symlink assertions because symlink creation is not available\n");
    }
#else
    SC_TEST_EXPECT(fs.chmod("permissionsFile.txt", 0640u));
    FileSystem::FileStat chmodStat;
    SC_TEST_EXPECT(fs.stat("permissionsFile.txt", chmodStat));
    SC_TEST_EXPECT((chmodStat.posix.mode & 0777u) == 0640u);

    SC_TEST_EXPECT(fs.chown("permissionsFile.txt", chmodStat.posix.uid, chmodStat.posix.gid));
    FileSystem::FileStat afterChownStat;
    SC_TEST_EXPECT(fs.stat("permissionsFile.txt", afterChownStat));
    SC_TEST_EXPECT(afterChownStat.posix.uid == chmodStat.posix.uid);
    SC_TEST_EXPECT(afterChownStat.posix.gid == chmodStat.posix.gid);

    SC_TEST_EXPECT(fs.createSymbolicLink("permissionsFile.txt", "permissionsLink.txt"));
    FileSystem::FileStat linkStatBefore;
    SC_TEST_EXPECT(fs.lstat("permissionsLink.txt", linkStatBefore));
    SC_TEST_EXPECT(fs.lchown("permissionsLink.txt", linkStatBefore.posix.uid, linkStatBefore.posix.gid));
    FileSystem::FileStat linkStatAfter;
    SC_TEST_EXPECT(fs.lstat("permissionsLink.txt", linkStatAfter));
    SC_TEST_EXPECT(linkStatAfter.entryType == FileSystemEntryType::SymbolicLink);
    SC_TEST_EXPECT(linkStatAfter.posix.uid == linkStatBefore.posix.uid);
    SC_TEST_EXPECT(linkStatAfter.posix.gid == linkStatBefore.posix.gid);

#if SC_PLATFORM_APPLE
    const uint32_t oldLinkMode = linkStatAfter.posix.mode & 0777u;
    const uint32_t newLinkMode = oldLinkMode == 0700u ? 0755u : 0700u;
    SC_TEST_EXPECT(fs.lchmod("permissionsLink.txt", newLinkMode));
    FileSystem::FileStat linkStatAfterLChmod;
    SC_TEST_EXPECT(fs.lstat("permissionsLink.txt", linkStatAfterLChmod));
    SC_TEST_EXPECT(linkStatAfterLChmod.entryType == FileSystemEntryType::SymbolicLink);
    SC_TEST_EXPECT((linkStatAfterLChmod.posix.mode & 0777u) == newLinkMode);
#else
    SC_TEST_EXPECT(not fs.lchmod("permissionsLink.txt", 0700u));
#endif
    SC_TEST_EXPECT(fs.removeLinkIfExists("permissionsLink.txt"));
#endif

    SC_TEST_EXPECT(fs.removeFile("permissionsFile.txt"));
    SC_TEST_EXPECT(fs.changeDirectory(tempDirectory.view()));
    SC_TEST_EXPECT(fs.removeDirectoryRecursive(permissionsDirectoryRoot.view()));
}

#if SC_PLATFORM_WINDOWS
void SC::FileSystemTest::prefixedPathInput()
{
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

    StringPath prefixedFilePath;
    SC_TEST_EXPECT(prefixedFilePath.assign("\\\\?\\"_a8));
    SC_TEST_EXPECT(prefixedFilePath.append(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(prefixedFilePath.append("\\prefixed-path-input.txt"_a8));

    SC_TEST_EXPECT(fs.writeString(prefixedFilePath.view(), "prefixed"));
    SC_TEST_EXPECT(fs.exists(prefixedFilePath.view()));

    String content = StringEncoding::Ascii;
    SC_TEST_EXPECT(fs.read(prefixedFilePath.view(), content));
    SC_TEST_EXPECT(content.view() == "prefixed");
    SC_TEST_EXPECT(fs.removeFile(prefixedFilePath.view()));
}

void SC::FileSystemTest::windowsLongPathHelper()
{
    using Helper = FileSystemTestWindowsDetail::WindowsPath;

    StringPath logicalPath;
    SC_TEST_EXPECT(Helper::makeLogicalPath("\\\\?\\C:\\alpha\\beta"_a8, logicalPath));
    SC_TEST_EXPECT(logicalPath.view() == "C:\\alpha\\beta"_a8);

    SC_TEST_EXPECT(Helper::makeLogicalPath("\\\\?\\UNC\\server\\share\\folder"_a8, logicalPath));
    SC_TEST_EXPECT(logicalPath.view() == "\\\\server\\share\\folder"_a8);

    SC_TEST_EXPECT(not Helper::makeLogicalPath("\\\\?\\relative\\folder"_a8, logicalPath));
    SC_TEST_EXPECT(not Helper::makeLogicalPath("\\\\?\\C:folder"_a8, logicalPath));

    SC_TEST_EXPECT(Helper::makeAbsoluteLogicalPath("child\\leaf.txt"_a8, "C:\\base\\dir"_a8, logicalPath));
    SC_TEST_EXPECT(logicalPath.view() == "C:\\base\\dir\\child\\leaf.txt"_a8);

    SC_TEST_EXPECT(Helper::makeAbsoluteLogicalPath("C:\\base\\dir\\..\\leaf.txt"_a8, {}, logicalPath));
    SC_TEST_EXPECT(logicalPath.view() == "C:\\base\\dir\\..\\leaf.txt"_a8);

    Helper::TransportString transportPath;
    SC_TEST_EXPECT(Helper::makeTransportPath("\\\\?\\C:\\base\\dir\\file.txt"_a8, {}, logicalPath, transportPath));
    SC_TEST_EXPECT(logicalPath.view() == "C:\\base\\dir\\file.txt"_a8);
    SC_TEST_EXPECT(transportPath.view() == "C:\\base\\dir\\file.txt"_a8);

    String driveLogicalAtCap = StringEncoding::Utf8;
    SC_TEST_EXPECT(driveLogicalAtCap.assign("C:\\"_a8));
    auto driveBuilder = StringBuilder::createForAppendingTo(driveLogicalAtCap);
    for (size_t idx = 0; idx < 1021; ++idx)
    {
        SC_TEST_EXPECT(driveBuilder.append("a"_a8));
    }
    driveBuilder.finalize();

    String prefixedDriveAtCap = StringEncoding::Utf8;
    SC_TEST_EXPECT(prefixedDriveAtCap.assign("\\\\?\\"_a8));
    auto prefixedDriveBuilder = StringBuilder::createForAppendingTo(prefixedDriveAtCap);
    SC_TEST_EXPECT(prefixedDriveBuilder.append(driveLogicalAtCap.view()));
    prefixedDriveBuilder.finalize();
    SC_TEST_EXPECT(Helper::makeLogicalPath(prefixedDriveAtCap.view(), logicalPath));
    SC_TEST_EXPECT(logicalPath.view() == driveLogicalAtCap.view());
    SC_TEST_EXPECT(Helper::makeTransportPath(driveLogicalAtCap.view(), {}, logicalPath, transportPath));
    SC_TEST_EXPECT(StringView(transportPath.view()).startsWith("\\\\?\\"_a8));

    String uncLogicalAtCap = StringEncoding::Utf8;
    SC_TEST_EXPECT(uncLogicalAtCap.assign("\\\\server\\share\\"_a8));
    auto uncBuilder = StringBuilder::createForAppendingTo(uncLogicalAtCap);
    for (size_t idx = 0; idx < 1009; ++idx)
    {
        SC_TEST_EXPECT(uncBuilder.append("a"_a8));
    }
    uncBuilder.finalize();

    String prefixedUNCAtCap = StringEncoding::Utf8;
    SC_TEST_EXPECT(prefixedUNCAtCap.assign("\\\\?\\UNC\\server\\share\\"_a8));
    auto prefixedUNCBuilder = StringBuilder::createForAppendingTo(prefixedUNCAtCap);
    for (size_t idx = 0; idx < 1009; ++idx)
    {
        SC_TEST_EXPECT(prefixedUNCBuilder.append("a"_a8));
    }
    prefixedUNCBuilder.finalize();
    SC_TEST_EXPECT(Helper::makeLogicalPath(prefixedUNCAtCap.view(), logicalPath));
    SC_TEST_EXPECT(logicalPath.view() == uncLogicalAtCap.view());

    String overCap = StringEncoding::Utf8;
    SC_TEST_EXPECT(overCap.assign("C:\\"_a8));
    auto builder = StringBuilder::createForAppendingTo(overCap);
    for (size_t idx = 0; idx < 260; ++idx)
    {
        SC_TEST_EXPECT(builder.append("abcd\\"_a8));
    }
    SC_TEST_EXPECT(not Helper::makeLogicalPath(overCap.view(), logicalPath));
}
#endif

void SC::FileSystemTest::snippet()
{
    // clang-format off
    //! [FileSystemQuickSheetSnippet]
FileSystem fs;
// Make all operations relative to applicationRootDirectory
SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

// Create a nested directory structure with some files too
SC_TEST_EXPECT(fs.makeDirectoryRecursive("copyDirectory/subdirectory"));
SC_TEST_EXPECT(fs.write("copyDirectory/testFile.txt", "asdf"));
SC_TEST_EXPECT(fs.existsAndIsFile("copyDirectory/testFile.txt"));
SC_TEST_EXPECT(fs.write("copyDirectory/subdirectory/testFile.txt", "asdf"));

// Copy the directory (recursively)
SC_TEST_EXPECT(fs.copyDirectory("copyDirectory", "COPY_copyDirectory"));

// Check that file exists in the new copied directory
SC_TEST_EXPECT(fs.existsAndIsFile("COPY_copyDirectory/testFile.txt"));
SC_TEST_EXPECT(fs.existsAndIsFile("COPY_copyDirectory/subdirectory/testFile.txt"));

// Copying again fails (because we're not overwriting)
SC_TEST_EXPECT(not fs.copyDirectory("copyDirectory", "COPY_copyDirectory"));

// Try copying again but now we ask to overwrite destination
SC_TEST_EXPECT(fs.copyDirectory("copyDirectory", "COPY_copyDirectory",
                FileSystem::CopyFlags().setOverwrite(true)));

// Rename the directory (fs.rename works also for files)
SC_TEST_EXPECT(fs.rename("copyDirectory", "COPY_copyDirectory2"));

// Check that the directory has been renamed
SC_TEST_EXPECT(fs.existsAndIsDirectory("COPY_copyDirectory2"));
SC_TEST_EXPECT(not fs.existsAndIsDirectory("copyDirectory"));

// Rename the directory back to the original name
SC_TEST_EXPECT(fs.rename("COPY_copyDirectory2", "copyDirectory"));

// Remove all files created
SC_TEST_EXPECT(fs.removeFile("copyDirectory/testFile.txt"));
SC_TEST_EXPECT(fs.removeFile("copyDirectory/subdirectory/testFile.txt"));
SC_TEST_EXPECT(fs.removeEmptyDirectory("copyDirectory/subdirectory"));
SC_TEST_EXPECT(fs.removeEmptyDirectory("copyDirectory"));
// Remove the entire tree of directories for the copy
SC_TEST_EXPECT(fs.removeDirectoryRecursive("COPY_copyDirectory"));
    //! [FileSystemQuickSheetSnippet]
    // clang-format on
}

namespace SC
{
void runFileSystemTest(SC::TestReport& report) { FileSystemTest test(report); }
} // namespace SC
