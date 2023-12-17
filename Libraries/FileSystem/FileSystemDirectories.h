// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Strings/SmallString.h"

namespace SC
{
struct FileSystemDirectories;
} // namespace SC

//! @addtogroup group_file_system
//! @{

/// @brief Reports location of system directories (executable / application root)
struct SC::FileSystemDirectories
{
    /// @brief Absolute executable path with extension (UTF16 on Windows, UTF8 elsewhere)
    StringView getExecutablePath() const { return executableFile.view(); }

    /// @brief Absolute Application path with extension (UTF16 on Windows, UTF8 elsewhere)
    /// @note on macOS this is different from FileSystemDirectories::getExecutablePath
    StringView getApplicationPath() const { return applicationRootDirectory.view(); }

    /// @brief Initializes the paths
    /// @return `true` if paths have been initialized correctly
    [[nodiscard]] bool init();

  private:
    static const int StaticPathSize = 1024 * sizeof(native_char_t);

    SmallString<StaticPathSize> executableFile;
    SmallString<StaticPathSize> applicationRootDirectory;
};

//! @}
