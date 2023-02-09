// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/StringNative.h"

namespace SC
{
struct FileSystem;
} // namespace SC

struct SC::FileSystem
{
    StringNative<512> currentDirectory;

    bool localizedErrorMessages = true;
    bool preciseErrorMessages   = true;

    ReturnCode init(StringView currentWorkingDirectory);
    ReturnCode changeDirectory(StringView currentWorkingDirectory);
    void       close();

    struct CopyFlags
    {
        CopyFlags()
        {
            overwrite           = false;
            useCloneIfSupported = true;
        }

        bool       overwrite;
        CopyFlags& setOverwrite(bool value)
        {
            overwrite = value;
            return *this;
        }

        bool       useCloneIfSupported;
        CopyFlags& setUseCloneIfSupported(bool value)
        {
            useCloneIfSupported = value;
            return *this;
        }
    };

    struct CopyOperation
    {
        StringView source;
        StringView destination;
        CopyFlags  copyFlags;
    };
    [[nodiscard]] ReturnCode copyFile(Span<const CopyOperation> sourceDestination);
    [[nodiscard]] ReturnCode copyFile(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyFile(CopyOperation{source, destination, copyFlags});
    }
    [[nodiscard]] ReturnCode copyDirectory(Span<const CopyOperation> sourceDestination);
    [[nodiscard]] ReturnCode copyDirectory(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyDirectory(CopyOperation{source, destination, copyFlags});
    }
    [[nodiscard]] ReturnCode removeFile(Span<const StringView> files);
    [[nodiscard]] ReturnCode removeDirectoryRecursive(Span<const StringView> directories);
    [[nodiscard]] ReturnCode removeEmptyDirectory(Span<const StringView> directories);
    [[nodiscard]] ReturnCode makeDirectory(Span<const StringView> directories);
    [[nodiscard]] bool       exists(StringView fileOrDirectory);
    [[nodiscard]] bool       existsAndIsDirectory(StringView directory);
    [[nodiscard]] bool       existsAndIsFile(StringView file);
    [[nodiscard]] ReturnCode write(StringView file, SpanVoid<const void> data);
    [[nodiscard]] ReturnCode read(StringView file, Vector<char>& data);
    [[nodiscard]] ReturnCode write(StringView file, StringView text);
    [[nodiscard]] ReturnCode read(StringView file, String& data, StringEncoding encoding);

  private:
    [[nodiscard]] bool pushFile1(StringView file, StringView& encodedPath);
    [[nodiscard]] bool pushFile2(StringView file, StringView& encodedPath);

    StringNative<128> fileFormatBuffer1;
    StringNative<128> fileFormatBuffer2;
    StringNative<128> errorMessageBuffer;

    ReturnCode formatError(int errorNumber, StringView item, bool isWindowsNativeError);
    struct Internal;
};
