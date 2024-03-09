// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Testing/Testing.h"

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
        if (test_section("makeDirectoryRecursive / removeEmptyDirectoryRecursive"))
        {
            makeRemoveDirectoryRecursive();
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
        // TODO: Add tests for createSymbolicLink, existsAndIsLink, removeLinkIfExists and moveDirectory
    }

    inline void formatError();
    inline void makeRemoveIsDirectory();
    inline void makeRemoveDirectoryRecursive();
    inline void writeReadRemoveFile();
    inline void copyExistsFile();
    inline void copyDirectoryRecursive();
    inline void removeDirectoryRecursive();
};

void SC::FileSystemTest::formatError()
{
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
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
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));

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

void SC::FileSystemTest::makeRemoveDirectoryRecursive()
{
    //! [makeRemoveDirectoryRecursive]
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));

    // Create a directory with 2 levels of nesting
    SC_TEST_EXPECT(fs.makeDirectoryRecursive("Test3/Subdir"));

    // Check that both levels have been created
    SC_TEST_EXPECT(fs.existsAndIsDirectory("Test3"));
    SC_TEST_EXPECT(fs.existsAndIsDirectory("Test3/Subdir"));

    // Remove both levels of directory
    SC_TEST_EXPECT(fs.removeEmptyDirectoryRecursive("Test3/Subdir"));

    // Check that directory has been removed
    SC_TEST_EXPECT(not fs.existsAndIsDirectory("Test3"));
    //! [makeRemoveDirectoryRecursive]
}

void SC::FileSystemTest::writeReadRemoveFile()
{
    //! [writeReadRemoveFileSnippet]
    FileSystem fs;
    // Make all operations relative to applicationRootDirectory
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    StringView content = "ASDF content";

    // Check that file doesn't exists before write-ing it and then check that it exist
    SC_TEST_EXPECT(not fs.exists("file.txt"));
    SC_TEST_EXPECT(fs.write("file.txt", content));
    SC_TEST_EXPECT(fs.existsAndIsFile("file.txt"));

    // Read the file and check its content
    String newString;
    SC_TEST_EXPECT(fs.read("file.txt", newString, StringEncoding::Ascii));
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
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));

    // Create a File names 'sourceFile.txt'
    StringView contentSource = "this is some content";
    SC_TEST_EXPECT(not fs.exists("sourceFile.txt"));
    SC_TEST_EXPECT(fs.write("sourceFile.txt", contentSource));

    // Check that 'sourceFile.txt' exist, but not 'destinationFile.txt'
    SC_TEST_EXPECT(fs.existsAndIsFile("sourceFile.txt"));
    SC_TEST_EXPECT(not fs.exists("destinationFile.txt"));

    // Ask to copy sourceFile.txt to destinationFile.txt (eventually overwriting, but without cloning)
    SC_TEST_EXPECT(fs.copyFile("sourceFile.txt", "destinationFile.txt",
                               FileSystem::CopyFlags().setOverwrite(true).setUseCloneIfSupported(false)));

    // Now read the destinationFile.txt content and check if it's the same as source
    String content;
    SC_TEST_EXPECT(fs.read("destinationFile.txt", content, StringEncoding::Ascii));
    SC_TEST_EXPECT(content.view() == contentSource);

    // Copy again sourceFile.txt to destinationFile.txt but using clone this time
    SC_TEST_EXPECT(fs.copyFile("sourceFile.txt", "destinationFile.txt",
                               FileSystem::CopyFlags().setOverwrite(true).setUseCloneIfSupported(true)));

    // Check again if file exists and its content
    SC_TEST_EXPECT(fs.existsAndIsFile("destinationFile.txt"));
    SC_TEST_EXPECT(fs.read("destinationFile.txt", content, StringEncoding::Ascii));
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
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));

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
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));

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

namespace SC
{
void runFileSystemTest(SC::TestReport& report) { FileSystemTest test(report); }
} // namespace SC
