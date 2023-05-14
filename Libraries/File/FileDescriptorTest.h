// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../FileSystem/FileSystem.h"
#include "../FileSystem/Path.h"
#include "../Testing/Test.h"
#include "FileDescriptor.h"

namespace SC
{
struct FileDescriptorTest;
}

struct SC::FileDescriptorTest : public SC::TestCase
{
    FileDescriptorTest(SC::TestReport& report) : TestCase(report, "FileDescriptorTest")
    {
        using namespace SC;
        if (test_section("open"))
        {
            StringNative<255> filePath = StringEncoding::Native;
            StringNative<255> dirPath  = StringEncoding::Native;
            const StringView  name     = "FileDescriptorTest";
            const StringView  fileName = "test.txt";
            SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
            SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
            SC_TEST_EXPECT(fs.makeDirectory(name));

            SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));

            FileDescriptor fd;

            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::WriteCreateTruncate));
            SC_TEST_EXPECT(fd.write(StringView("test").toCharSpan()));
            SC_TEST_EXPECT(fd.close());

            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::ReadOnly));

            char       buffer[4] = {0};
            Span<char> spanOut;
            SC_TEST_EXPECT(fd.read({buffer, sizeof(buffer)}, spanOut));
            SC_TEST_EXPECT(fd.close());

            StringView sv(spanOut, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(sv.compareASCII("test") == StringComparison::Equals);

            SC_TEST_EXPECT(fs.removeFile(fileName));
            SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
        }
    }
};
