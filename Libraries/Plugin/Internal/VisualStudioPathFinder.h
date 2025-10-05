// Copyright (c) Gabe Csendes
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Result.h"
#include "../../Process/Process.h"
#include "../../Strings/StringView.h"
#include "PluginFileSystem.h"

namespace SC
{

//! @addtogroup group_plugin
//! @{

/// @brief Finds paths of the installed Visual Studio instances
struct VisualStudioPathFinder
{
    /// @brief Constructs a VisualStudioPathFinder and check if VS locator exists or not
    VisualStudioPathFinder()
    {
        const StringView wsp = L"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
        if (PluginFileSystem::existsAndIsFileAbsolute(wsp))
            vsWherePath = wsp;
    }

    /// @brief Finds newest version of the installed Visual Studio instance(s)
    /// @param[out] vsPath Path where Visual Studio is installed
    /// @return Valid Result if a Visual Studio path has been found successfully
    Result findLatest(StringPath& vsPath)
    {
        if (vsWherePath.isEmpty())
            return Result::Error("Visual Studio Locator not found.");

        StringPath output;
        if (not Process().exec({vsWherePath, "-prerelease", "-latest", "-property", "installationPath"}, output))
            return Result::Error("Visual Studio Locator cannot be executed.");

        return Result(vsPath.assign(StringView(output.view()).trimEndAnyOf({'\r', '\n'})));
    }

    /// @brief Collects every Visual Studio version that installed on the current system
    /// @param[out] vsPaths List of installed Visual Studio path(s)
    /// @return Valid Result if VS paths has been collected successfully
    template <typename Container>
    Result findAll(Container& vsPaths)
    {
        if (vsWherePath.isEmpty())
            return Result::Error("Visual Studio Locator not found.");
        char       outputStorage[MAX_PATH * 2 + 1];
        Span<char> output = {outputStorage};
        if (not Process().exec({vsWherePath, "-prerelease", "-property", "installationPath"}, output))
            return Result::Error("Visual Studio Locator cannot be executed.");

        // TODO: Check if VSWhere output is actually UTF8
        StringViewTokenizer tokenizer(StringView::fromNullTerminated(outputStorage, StringEncoding::Utf8));
        while (tokenizer.tokenizeNext('\n', StringViewTokenizer::SkipEmpty))
        {
            StringPath path;
            if (path.assign(tokenizer.component.trimEndAnyOf({'\r'})))
            {
                SC_TRY(vsPaths.push_back(path));
            }
        }

        return Result(true);
    }

  private:
    StringView vsWherePath;
};

//! @}

} // namespace SC
