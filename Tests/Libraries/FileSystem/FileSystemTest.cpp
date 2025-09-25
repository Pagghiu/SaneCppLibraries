// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

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
        // TODO: Add tests for createSymbolicLink, existsAndIsLink, removeLinkIfExists and moveDirectory
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
