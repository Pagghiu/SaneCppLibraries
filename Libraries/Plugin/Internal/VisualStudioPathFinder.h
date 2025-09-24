// Copyright (c) Gabe Csendes
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Containers/Vector.h"
#include "../../Foundation/Result.h"
#include "../../Process/Process.h"
#include "../../Strings/StringView.h"

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
        const StringView wsp = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
        if (FileSystem().exists(wsp))
            vswherePath = wsp;
    }

    /// @brief Finds newest version of the installed Visual Studio instance(s)
    /// @param[out] vsPath Path where Visual Studio is installed
    /// @return Valid Result if a Visual Studio path has been found successfully
    Result findLatest(String& vsPath)
    {
        if (vswherePath.isEmpty())
            return Result::Error("Visual Studio Locator not found.");

        String output = StringEncoding::Ascii;
        if (not Process().exec({vswherePath, "-prerelease", "-latest", "-property", "installationPath"}, output))
            return Result::Error("Visual Studio Locator cannot be executed.");

        vsPath = StringView(output.view()).trimEndAnyOf({'\r', '\n'});
        return Result(true);
    }

    /// @brief Collects every Visual Studio version that installed on the current system
    /// @param[out] vsPaths List of installed Visual Studio path(s)
    /// @return Valid Result if VS paths has been collected successfully
    Result findAll(Vector<String>& vsPaths)
    {
        if (vswherePath.isEmpty())
            return Result::Error("Visual Studio Locator not found.");

        String output = StringEncoding::Ascii;
        if (not Process().exec({vswherePath, "-prerelease", "-property", "installationPath"}, output))
            return Result::Error("Visual Studio Locator cannot be executed.");

        StringViewTokenizer tokenizer(output.view());
        while (tokenizer.tokenizeNext('\n', StringViewTokenizer::SkipEmpty))
            (void)vsPaths.push_back(tokenizer.component.trimEndAnyOf({'\r'}));

        return Result(true);
    }

  private:
    StringView vswherePath;
};

//! @}

} // namespace SC
