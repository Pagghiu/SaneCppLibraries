// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Path.h"
#include "../../Containers/Vector.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"

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
            testIsAbsolute(report);
        }
        if (test_section("Path::dirname"))
        {
            testDirname(report);
        }
        if (test_section("Path::basename"))
        {
            testBasename(report);
        }
        if (test_section("Path::ParsedView::parsePosix"))
        {
            testParsePosix(report);
        }

        if (test_section("Path::ParsedView::parseWindows"))
        {
            testParseWindows(report);
        }

        if (test_section("Path::parseNameExtension"))
        {
            testParseNameExtension(report);
        }

        if (test_section("Path::parse"))
        {
            testParse(report);
        }

        if (test_section("Path::normalize"))
        {
            testNormalize(report);
        }

        if (test_section("Path::relativeFromTo"))
        {
            testRelativeFromTo(report);
        }
    }

    inline void testIsAbsolute(TestReport& report);
    inline void testDirname(TestReport& report);
    inline void testBasename(TestReport& report);
    inline void testParsePosix(TestReport& report);
    inline void testParseWindows(TestReport& report);
    inline void testParseNameExtension(TestReport& report);
    inline void testParse(TestReport& report);
    inline void testNormalize(TestReport& report);
    inline void testRelativeFromTo(TestReport& report);
};

void SC::PathTest::testIsAbsolute(TestReport&)
{
    //! [isAbsoluteSnippet]
    SC_TEST_EXPECT(Path::isAbsolute("/dirname/basename", Path::AsPosix));           // Posix Absolute
    SC_TEST_EXPECT(not Path::isAbsolute("./dirname/basename", Path::AsPosix));      // Posix Relative
    SC_TEST_EXPECT(Path::isAbsolute("C:\\dirname\\basename", Path::AsWindows));     // Windows with Drive
    SC_TEST_EXPECT(Path::isAbsolute("\\\\server\\dir", Path::AsWindows));           // Windows with Network
    SC_TEST_EXPECT(Path::isAbsolute("\\\\?\\C:\\server\\dir", Path::AsWindows));    // Windows with Long
    SC_TEST_EXPECT(not Path::isAbsolute("..\\dirname\\basename", Path::AsWindows)); // Windows relative
    //! [isAbsoluteSnippet]
}

void SC::PathTest::testDirname(TestReport&)
{
    //! [dirnameSnippet]
    SC_TEST_EXPECT(Path::dirname("/dirname/basename", Path::AsPosix) == "/dirname");
    SC_TEST_EXPECT(Path::dirname("/dirname/basename//", Path::AsPosix) == "/dirname");
    SC_TEST_EXPECT(Path::dirname("C:\\dirname\\basename", Path::AsWindows) == "C:\\dirname");
    SC_TEST_EXPECT(Path::dirname("\\dirname\\basename\\\\", Path::AsWindows) == "\\dirname");
    //! [dirnameSnippet]
}

void SC::PathTest::testBasename(TestReport&)
{
    //! [basenameSnippet]
    SC_TEST_EXPECT(Path::basename("/a/basename", Path::AsPosix) == "basename");
    SC_TEST_EXPECT(Path::basename("/a/basename//", Path::AsPosix) == "basename");
    //! [basenameSnippet]
    //! [basenameExtSnippet]
    SC_TEST_EXPECT(Path::basename("/a/basename.html", ".html") == "basename");
    //! [basenameExtSnippet]
}

void SC::PathTest::testParsePosix(TestReport&)
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

void SC::PathTest::testParseWindows(TestReport&)
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
    SC_TEST_EXPECT(path.directory == "C:\\");
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

    SC_TEST_EXPECT(path.parseWindows("//?/ASD/bbb/name.ext"));
    SC_TEST_EXPECT(path.root == "//?/");
    SC_TEST_EXPECT(path.directory == "//?/ASD/bbb");
    SC_TEST_EXPECT(path.base == "name.ext");
    SC_TEST_EXPECT(path.name == "name");
    SC_TEST_EXPECT(path.ext == "ext");
    SC_TEST_EXPECT(path.endsWithSeparator == false);
}

void SC::PathTest::testParseNameExtension(TestReport&)
{
    StringView name("NAME"), ext("EXT");
    //! [parseNameExtensionSnippet]
    SC_TEST_EXPECT(Path::parseNameExtension("name.ext", name, ext));
    SC_TEST_EXPECT(name == "name");
    SC_TEST_EXPECT(ext == "ext");

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
    //! [parseNameExtensionSnippet]
}

void SC::PathTest::testParse(TestReport&)
{
    Path::ParsedView view;
    SC_TEST_EXPECT(Path::parse("C:\\dir\\base.ext", view, Path::AsWindows));
    SC_TEST_EXPECT(view.directory == "C:\\dir");
    SC_TEST_EXPECT(Path::parse("/usr/dir/base.ext", view, Path::AsPosix));
    SC_TEST_EXPECT(view.directory == "/usr/dir");
}

void SC::PathTest::testNormalize(TestReport&)
{
    String                       path;
    SmallVector<StringView, 256> cmp;
    //! [normalizeSnippet]
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
    SC_TEST_EXPECT(Path::normalize("..\\../../../Libraries/Plugin/PluginTest.h", cmp, &path, Path::AsPosix) and
                   path == "../../../../Libraries/Plugin/PluginTest.h");
    SC_TEST_EXPECT(Path::normalize("\\\\Mac\\Some\\Dir", cmp, &path, Path::AsPosix) and path == "\\\\Mac/Some/Dir");
    //! [normalizeSnippet]
}

void SC::PathTest::testRelativeFromTo(TestReport&)
{
    String path;
    SC_TEST_EXPECT(not Path::relativeFromTo("/a", "", path, Path::AsPosix));
    SC_TEST_EXPECT(not Path::relativeFromTo("", "/a", path, Path::AsPosix));
    SC_TEST_EXPECT(not Path::relativeFromTo("", "", path, Path::AsPosix));
    SC_TEST_EXPECT(Path::relativeFromTo("/", "/a/b/c//", path, Path::AsPosix) and path == "a/b/c");
    SC_TEST_EXPECT(Path::relativeFromTo("/a/b/_1/2/3", "/a/b/_d/e", path, Path::AsPosix, Path::AsPosix) and
                   path == "../../../_d/e");
    SC_TEST_EXPECT(Path::relativeFromTo("C:\\a\\b", "C:\\a\\c", path, Path::AsWindows, Path::AsWindows) and
                   path == "..\\c");
    SC_TEST_EXPECT(not Path::relativeFromTo("/a", "b/c", path, Path::AsPosix));
    SC_TEST_EXPECT(not Path::relativeFromTo("a", "/b/c", path, Path::AsPosix));
    SC_TEST_EXPECT(Path::relativeFromTo("/a/b", "/a/b", path, Path::AsPosix) and path == ".");
    SC_TEST_EXPECT(Path::relativeFromTo("/a/b/c/d/e/f/g/h", "/a/b/c/d/e", path, Path::AsPosix, Path::AsPosix) and
                   path == "../../..");
}

namespace SC
{
void runPathTest(SC::TestReport& report) { PathTest test(report); }
} // namespace SC
