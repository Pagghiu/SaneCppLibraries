// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/String.h"
#include "../Testing/Test.h"
#include "Path.h"

namespace SC
{
struct PathTest;
}

struct SC::PathTest : public SC::TestCase
{
    PathTest(SC::TestReport& report) : TestCase(report, "PathTest")
    {
        if (test_section("Path::isAbsolute"))
        {
            SC_TEST_EXPECT(Path::Posix::isAbsolute("/dirname/basename"));           // Posix Absolute
            SC_TEST_EXPECT(not Path::Posix::isAbsolute("./dirname/basename"));      // Posix Relative
            SC_TEST_EXPECT(Path::Windows::isAbsolute("C:\\dirname\\basename"));     // Windows with Drive
            SC_TEST_EXPECT(Path::Windows::isAbsolute("\\\\server\\dir"));           // Windows with Network
            SC_TEST_EXPECT(Path::Windows::isAbsolute("\\\\?\\C:\\server\\dir"));    // Windows with Long
            SC_TEST_EXPECT(not Path::Windows::isAbsolute("..\\dirname\\basename")); // Widnwos relative
        }
        if (test_section("Path::dirname"))
        {
            SC_TEST_EXPECT(Path::Posix::dirname("/dirname/basename") == "/dirname");
            SC_TEST_EXPECT(Path::Posix::dirname("/dirname/basename//") == "/dirname");
            SC_TEST_EXPECT(Path::Windows::dirname("C:\\dirname\\basename") == "C:\\dirname");
            SC_TEST_EXPECT(Path::Windows::dirname("\\dirname\\basename\\\\") == "\\dirname");
        }
        if (test_section("Path::basename"))
        {
            SC_TEST_EXPECT(Path::Posix::basename("/a/basename") == "basename");
            SC_TEST_EXPECT(Path::Posix::basename("/a/basename//") == "basename");
            SC_TEST_EXPECT(Path::Posix::basename("/a/basename.html", ".html") == "basename");
        }
        if (test_section("Path::ParsedView::parsePosix"))
        {
            Path::ParsedView path;
            SC_TEST_EXPECT(path.parsePosix("/123/456"));
            SC_TEST_EXPECT(path.root == "/");
            SC_TEST_EXPECT(path.directory == "/123");
            SC_TEST_EXPECT(path.base == "456");
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parsePosix("/123/"));
            SC_TEST_EXPECT(path.root == "/");
            SC_TEST_EXPECT(path.directory == "/123");
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parsePosix("/"));
            SC_TEST_EXPECT(path.root == "/");
            SC_TEST_EXPECT(path.directory == "/");
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parsePosix("//"));
            SC_TEST_EXPECT(path.root == "/");
            SC_TEST_EXPECT(path.directory == "//");
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);
        }

        if (test_section("Path::ParsedView::parseWindows"))
        {
            Path::ParsedView path;
            SC_TEST_EXPECT(!path.parseWindows("\\"));
            SC_TEST_EXPECT(!path.parseWindows(""));
            SC_TEST_EXPECT(!path.parseWindows(":"));
            SC_TEST_EXPECT(!path.parseWindows("C:"));

            SC_TEST_EXPECT(!path.parseWindows("C"));
            SC_TEST_EXPECT(path.root.isEmpty());
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("C:\\"));
            SC_TEST_EXPECT(path.root == "C:\\");
            SC_TEST_EXPECT(path.directory == "C:\\");
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\\\"));
            SC_TEST_EXPECT(path.root == "C:\\");
            SC_TEST_EXPECT(path.directory == "C:\\\\");
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD"));
            SC_TEST_EXPECT(path.root == "C:\\");
            SC_TEST_EXPECT(path.directory == "C:\\");
            SC_TEST_EXPECT(path.base == "ASD");
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\"));
            SC_TEST_EXPECT(path.root == "C:\\");
            SC_TEST_EXPECT(path.directory == "C:\\ASD");
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\\\"));
            SC_TEST_EXPECT(path.root == "C:\\");
            SC_TEST_EXPECT(path.directory == "C:\\ASD\\");
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\bbb"));
            SC_TEST_EXPECT(path.root == "C:\\");
            SC_TEST_EXPECT(path.directory == "C:\\ASD");
            SC_TEST_EXPECT(path.base == "bbb");
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\bbb\\name.ext"));
            SC_TEST_EXPECT(path.root == "C:\\");
            SC_TEST_EXPECT(path.directory == "C:\\ASD\\bbb");
            SC_TEST_EXPECT(path.base == "name.ext");
            SC_TEST_EXPECT(path.name == "name");
            SC_TEST_EXPECT(path.ext == "ext");
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("\\\\ASD\\bbb\\name.ext"));
            SC_TEST_EXPECT(path.root == "\\\\");
            SC_TEST_EXPECT(path.directory == "\\\\ASD\\bbb");
            SC_TEST_EXPECT(path.base == "name.ext");
            SC_TEST_EXPECT(path.name == "name");
            SC_TEST_EXPECT(path.ext == "ext");
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("\\\\?\\ASD\\bbb\\name.ext"));
            SC_TEST_EXPECT(path.root == "\\\\?\\");
            SC_TEST_EXPECT(path.directory == "\\\\?\\ASD\\bbb");
            SC_TEST_EXPECT(path.base == "name.ext");
            SC_TEST_EXPECT(path.name == "name");
            SC_TEST_EXPECT(path.ext == "ext");
            SC_TEST_EXPECT(path.endsWithSeparator == false);
        }

        if (test_section("Path::parseNameExtension"))
        {
            StringView name("ASD"), ext("DSA");
            SC_TEST_EXPECT(!Path::parseNameExtension("", name, ext));
            SC_TEST_EXPECT(name.isEmpty());
            SC_TEST_EXPECT(ext.isEmpty());

            SC_TEST_EXPECT(!Path::parseNameExtension(".", name, ext));
            SC_TEST_EXPECT(name.isEmpty());
            SC_TEST_EXPECT(ext.isEmpty());

            SC_TEST_EXPECT(Path::parseNameExtension(".ext", name, ext));
            SC_TEST_EXPECT(name.isEmpty());
            SC_TEST_EXPECT(ext == "ext");

            SC_TEST_EXPECT(Path::parseNameExtension("name.", name, ext));
            SC_TEST_EXPECT(name == "name");
            SC_TEST_EXPECT(ext.isEmpty());

            SC_TEST_EXPECT(Path::parseNameExtension("name.name.ext", name, ext));
            SC_TEST_EXPECT(name == "name.name");
            SC_TEST_EXPECT(ext == "ext");

            SC_TEST_EXPECT(Path::parseNameExtension("name..", name, ext));
            SC_TEST_EXPECT(name == "name.");
            SC_TEST_EXPECT(ext.isEmpty());
        }

        if (test_section("Path::parse"))
        {
            Path::ParsedView view;
            SC_TEST_EXPECT(Path::parse("C:\\dir\\base.ext", view, Path::AsWindows));
            SC_TEST_EXPECT(view.directory == "C:\\dir");
            SC_TEST_EXPECT(Path::parse("/usr/dir/base.ext", view, Path::AsPosix));
            SC_TEST_EXPECT(view.directory == "/usr/dir");
        }

        if (test_section("Path::normalize"))
        {
            String                       path;
            SmallVector<StringView, 256> cmp;
            SC_TEST_EXPECT(Path::normalize("///", cmp, &path, Path::AsPosix) and path == "/");
            SC_TEST_EXPECT(Path::normalize("\\\\", cmp, &path, Path::AsWindows) and path == "\\\\");
            SC_TEST_EXPECT(Path::normalize("/a/b/c/../d/e//", cmp, &path, Path::AsPosix) and path == "/a/b/d/e");
            SC_TEST_EXPECT(Path::normalize("a\\b\\..\\c\\d\\..\\e", cmp, &path, Path::AsPosix) and path == "a/c/e");
            SC_TEST_EXPECT(Path::normalize("..\\a\\b\\c", cmp, &path, Path::AsWindows) and path == "..\\a\\b\\c");
            SC_TEST_EXPECT(Path::normalize("C:\\Users\\SC\\..\\Documents\\", cmp, &path, Path::AsWindows) and
                           path == "C:\\Users\\Documents");
            SC_TEST_EXPECT(Path::normalize("\\\\Users\\SC\\..\\Documents", cmp, &path, Path::AsWindows) and
                           path == "\\\\Users\\Documents");
            SC_TEST_EXPECT(Path::normalize("/a/b/../c/./d/", cmp, &path, Path::AsPosix) and path == "/a/c/d");
        }

        if (test_section("Path::relativeFromTo"))
        {
            String path;
            SC_TEST_EXPECT(not Path::relativeFromTo("/a", "", path, Path::AsPosix));
            SC_TEST_EXPECT(not Path::relativeFromTo("", "/a", path, Path::AsPosix));
            SC_TEST_EXPECT(not Path::relativeFromTo("", "", path, Path::AsPosix));
            SC_TEST_EXPECT(Path::relativeFromTo("/", "/a/b/c//", path, Path::AsPosix) and path == "a/b/c");
            SC_TEST_EXPECT(Path::relativeFromTo("/a/b/1/2/3", "/a/b/d/e", path, Path::AsPosix) and
                           path == "../../../d/e");
            SC_TEST_EXPECT(Path::relativeFromTo("C:\\a\\b", "C:\\a\\c", path, Path::AsWindows) and path == "..\\c");
            SC_TEST_EXPECT(not Path::relativeFromTo("/a", "b/c", path, Path::AsPosix));
            SC_TEST_EXPECT(not Path::relativeFromTo("a", "/b/c", path, Path::AsPosix));
            SC_TEST_EXPECT(Path::relativeFromTo("/a/b", "/a/b", path, Path::AsPosix) and path == ".");
        }
    }
};
