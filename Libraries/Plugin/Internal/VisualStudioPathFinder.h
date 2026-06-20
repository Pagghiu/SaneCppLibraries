// Copyright (c) Gabe Csendes
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/Result.h"
#include "../../Process/Process.h"
#include "PluginFileSystem.h"
#include "PluginString.h"

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
        const StringSpan wsp = L"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
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

        return Result(vsPath.assign(PluginString::trimEndNewLines(output.view())));
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
        PluginString::Tokenizer tokenizer(StringSpan::fromNullTerminated(outputStorage, StringEncoding::Utf8));
        while (tokenizer.next('\n'))
        {
            StringPath path;
            if (path.assign(PluginString::trimEnd(tokenizer.component, '\r')))
            {
                SC_TRY(vsPaths.push_back(path));
            }
        }

        return Result(true);
    }

  private:
    StringSpan vsWherePath;
};

//! @}

} // namespace SC
