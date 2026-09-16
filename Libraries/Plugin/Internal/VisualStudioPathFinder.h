// Copyright (c) Gabe Csendes
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Process/Process.h"
#include "../PluginError.h"
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
    ResultPlugin findLatest(StringPath& vsPath)
    {
        if (vsWherePath.isEmpty())
            return ResultPlugin(PluginError::CompilerNotFound, PluginErrorDetail::CompilerFindConfiguration);

        StringPath output;
        SC_TRY(Process().exec({vsWherePath, "-prerelease", "-latest", "-property", "installationPath"}, output));

        if (not vsPath.assign(PluginString::trimEndNewLines(output.view())))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAssign);
        return ResultPlugin(true);
    }

    /// @brief Collects every Visual Studio version that installed on the current system
    /// @param[out] vsPaths List of installed Visual Studio path(s)
    /// @return Valid Result if VS paths has been collected successfully
    template <typename Container>
    ResultPlugin findAll(Container& vsPaths)
    {
        if (vsWherePath.isEmpty())
            return ResultPlugin(PluginError::CompilerNotFound, PluginErrorDetail::CompilerFindConfiguration);
        char       outputStorage[MAX_PATH * 2 + 1];
        Span<char> output = {outputStorage};
        SC_TRY(Process().exec({vsWherePath, "-prerelease", "-property", "installationPath"}, output));

        // TODO: Check if VSWhere output is actually UTF8
        PluginString::Tokenizer tokenizer(StringSpan::fromNullTerminated(outputStorage, StringEncoding::Utf8));
        while (tokenizer.next('\n'))
        {
            StringPath path;
            if (not path.assign(PluginString::trimEnd(tokenizer.component, '\r')))
                return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAssign);
            if (not vsPaths.push_back(path))
                return ResultPlugin::withRequiredElements(PluginError::CompilerConfigurationCapacityExceeded,
                                                          PluginErrorDetail::CompilerFindConfiguration,
                                                          static_cast<uint32_t>(vsPaths.size() + 1));
        }

        return ResultPlugin(true);
    }

  private:
    StringSpan vsWherePath;
};

//! @}

} // namespace SC
