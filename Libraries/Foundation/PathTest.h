// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Path.h"

namespace SC
{
struct PathTest;
}

struct SC::PathTest : public SC::TestCase
{
    PathTest(SC::TestReport& report) : TestCase(report, "PathTest")
    {
        // TODO: PathView::directory and base are not defined consistently
        if (test_section("PathView::parsePosix"))
        {
            PathView path;
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

        if (test_section("PathView::parseWindows"))
        {
            PathView path;
            SC_TEST_EXPECT(!path.parseWindows("\\"));
            SC_TEST_EXPECT(!path.parseWindows(""));
            SC_TEST_EXPECT(!path.parseWindows(":"));
            SC_TEST_EXPECT(!path.parseWindows("C:"));

            SC_TEST_EXPECT(!path.parseWindows("C"));
            SC_TEST_EXPECT(path.root.isEmpty());
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("C:\\"));
            SC_TEST_EXPECT(path.root == "C:\\"_sv);
            SC_TEST_EXPECT(path.directory == "C:\\"_sv);
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\\\"));
            SC_TEST_EXPECT(path.root == "C:\\"_sv);
            SC_TEST_EXPECT(path.directory == "C:\\\\"_sv);
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD"));
            SC_TEST_EXPECT(path.root == "C:\\"_sv);
            SC_TEST_EXPECT(path.directory == "C:\\"_sv);
            SC_TEST_EXPECT(path.base == "ASD"_sv);
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\"));
            SC_TEST_EXPECT(path.root == "C:\\"_sv);
            SC_TEST_EXPECT(path.directory == "C:\\ASD"_sv);
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\\\"));
            SC_TEST_EXPECT(path.root == "C:\\"_sv);
            SC_TEST_EXPECT(path.directory == "C:\\ASD\\"_sv);
            SC_TEST_EXPECT(path.base.isEmpty());
            SC_TEST_EXPECT(path.endsWithSeparator == true);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\bbb"));
            SC_TEST_EXPECT(path.root == "C:\\"_sv);
            SC_TEST_EXPECT(path.directory == "C:\\ASD"_sv);
            SC_TEST_EXPECT(path.base == "bbb"_sv);
            SC_TEST_EXPECT(path.endsWithSeparator == false);

            SC_TEST_EXPECT(path.parseWindows("C:\\ASD\\bbb\\name.ext"));
            SC_TEST_EXPECT(path.root == "C:\\"_sv);
            SC_TEST_EXPECT(path.directory == "C:\\ASD\\bbb"_sv);
            SC_TEST_EXPECT(path.base == "name.ext"_sv);
            SC_TEST_EXPECT(path.name == "name"_sv);
            SC_TEST_EXPECT(path.ext == "ext"_sv);
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
#if SC_PLATFORM_WINDOWS
            PathView view;
            SC_TEST_EXPECT(Path::parse("C:\\dir\\base.ext", view));
            SC_TEST_EXPECT(view.directory == "C:\\dir");
#else
            PathView view;
            SC_TEST_EXPECT(Path::parse("/usr/dir/base.ext", view));
            SC_TEST_EXPECT(view.directory == "/usr/dir");
#endif
        }
    }
};
