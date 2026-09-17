// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//
// Intentionally no #pragma once / include guard.
// This file is source material for private implementation namespaces.
// Each including library must get its own copy, especially in single-file amalgamations.
//
// Include from inside a unique private namespace.
// Required includes before this file: Windows.h, string.h, wchar.h, wctype.h.
// Required SC types: Result, StringPath, StringSpan, StringNativeBuffer.

enum class WindowsPathError : uint8_t
{
    None = 0,
    CapacityExceeded,
    BasePathNotAbsolute,
    MalformedPath,
    NativeCallFailed,
};

struct WindowsPathResult
{
    WindowsPathError error       = WindowsPathError::None;
    uint32_t         nativeError = 0;

    constexpr WindowsPathResult() = default;
    constexpr WindowsPathResult(WindowsPathError error, uint32_t nativeError = 0)
        : error(error), nativeError(nativeError)
    {}

    explicit constexpr operator bool() const { return error == WindowsPathError::None; }
    constexpr          operator Result() const { return Result(error == WindowsPathError::None); }
    constexpr Result   toResult() const { return Result(error == WindowsPathError::None); }
};

struct WindowsPath
{
    static constexpr size_t LogicalCapacity   = StringPath::MaxPath;
    static constexpr size_t TransportCapacity = StringPath::MaxPath + 6;

    using TransportString = StringNativeBuffer<TransportCapacity + 1>;

    static WindowsPathResult makeLogicalPath(StringSpan input, StringPath& logicalPath)
    {
        TransportString inputPath;
        if (not inputPath.assign(input))
            return WindowsPathError::CapacityExceeded;
        canonicalizeSeparators(inputPath);
        return copyLogicalPathWithoutTransportPrefix(inputPath.view(), logicalPath);
    }

    static WindowsPathResult makeAbsoluteLogicalPath(StringSpan input, StringSpan baseDirectory,
                                                     StringPath& logicalPath)
    {
        SC_TRY(makeLogicalPath(input, logicalPath));
        if (isAbsolute(logicalPath.view()))
        {
            return {};
        }

        StringPath basePath;
        if (baseDirectory.isEmpty())
        {
            SC_TRY(getCurrentDirectory(basePath));
        }
        else
        {
            SC_TRY(makeLogicalPath(baseDirectory, basePath));
        }

        if (not isAbsolute(basePath.view()))
            return WindowsPathError::BasePathNotAbsolute;

        StringPath joinedPath;
        if (not joinedPath.assign(basePath.view()))
            return WindowsPathError::CapacityExceeded;

        const wchar_t* logicalData = logicalPath.view().getNullTerminatedNative();
        if (isRootedRelative(logicalPath.view()))
        {
            SC_TRY(copyRootOnly(basePath.view(), joinedPath));
            if (not joinedPath.append(logicalPath.view()))
                return WindowsPathError::CapacityExceeded;
        }
        else if (isDriveRelative(logicalPath.view()))
        {
            if (sameDrive(basePath.view(), logicalPath.view()))
            {
                SC_TRY(appendRelativePath(
                    joinedPath, StringSpan({logicalData + 2, logicalPath.view().sizeInBytes() / sizeof(wchar_t) - 2},
                                           true, StringEncoding::Utf16)));
            }
            else
            {
                SC_TRY(copyDriveRoot(logicalPath.view(), joinedPath));
                SC_TRY(appendRelativePath(
                    joinedPath, StringSpan({logicalData + 2, logicalPath.view().sizeInBytes() / sizeof(wchar_t) - 2},
                                           true, StringEncoding::Utf16)));
            }
        }
        else
        {
            SC_TRY(appendRelativePath(joinedPath, logicalPath.view()));
        }

        return normalizeAbsolutePath(joinedPath.view(), logicalPath);
    }

    static WindowsPathResult makeTransportPath(StringSpan input, StringSpan baseDirectory, StringPath& logicalPath,
                                               TransportString& transportPath)
    {
        SC_TRY(makeAbsoluteLogicalPath(input, baseDirectory, logicalPath));
        if (not needsTransportPrefix(logicalPath.view()))
        {
            transportPath.clear();
            if (not transportPath.assign(logicalPath.view()))
                return WindowsPathError::CapacityExceeded;
            return {};
        }
        return appendTransportPrefix(logicalPath.view(), transportPath);
    }

    static WindowsPathResult appendTransportPrefix(StringSpan logicalPath, TransportString& transportPath)
    {
        transportPath.clear();
        if (isUNC(logicalPath))
        {
            if (not transportPath.append(L"\\\\?\\UNC\\"))
                return WindowsPathError::CapacityExceeded;
            const wchar_t* pathData = logicalPath.getNullTerminatedNative();
            if (not transportPath.append(StringSpan({pathData + 2, logicalPath.sizeInBytes() / sizeof(wchar_t) - 2},
                                                    true, StringEncoding::Utf16)))
                return WindowsPathError::CapacityExceeded;
        }
        else
        {
            if (not transportPath.append(L"\\\\?\\"))
                return WindowsPathError::CapacityExceeded;
            if (not transportPath.append(logicalPath))
                return WindowsPathError::CapacityExceeded;
        }
        return {};
    }

    static bool needsTransportPrefix(StringSpan logicalPath)
    {
        // Directory creation needs extended spelling before the nominal MAX_PATH boundary.
        return logicalPath.sizeInBytes() / sizeof(wchar_t) >= MAX_PATH - 12;
    }

    static WindowsPathResult getExecutablePath(StringPath& executablePath)
    {
        DWORD length = ::GetModuleFileNameW(nullptr, executablePath.writableSpan().data(),
                                            static_cast<DWORD>(StringPath::StorageCapacity));
        if (length == 0)
        {
            (void)executablePath.resize(0);
            return {WindowsPathError::NativeCallFailed, ::GetLastError()};
        }
        if (length >= StringPath::StorageCapacity)
        {
            (void)executablePath.resize(0);
            return WindowsPathError::CapacityExceeded;
        }
        if (not executablePath.resize(length))
            return WindowsPathError::CapacityExceeded;
        canonicalizeSeparators(executablePath);
        return stripTransportPrefix(executablePath);
    }

    static WindowsPathResult getCurrentDirectory(StringPath& currentWorkingDirectory)
    {
        DWORD length = ::GetCurrentDirectoryW(static_cast<DWORD>(StringPath::StorageCapacity),
                                              currentWorkingDirectory.writableSpan().data());
        if (length == 0)
        {
            (void)currentWorkingDirectory.resize(0);
            return {WindowsPathError::NativeCallFailed, ::GetLastError()};
        }
        if (length >= StringPath::StorageCapacity)
        {
            (void)currentWorkingDirectory.resize(0);
            return WindowsPathError::CapacityExceeded;
        }
        if (not currentWorkingDirectory.resize(length))
            return WindowsPathError::CapacityExceeded;
        canonicalizeSeparators(currentWorkingDirectory);
        return stripTransportPrefix(currentWorkingDirectory);
    }

    static bool looksLikeFilesystemPath(StringSpan input)
    {
        StringPath logicalPath;
        if (not makeLogicalPath(input, logicalPath))
        {
            return false;
        }
        const StringSpan path = logicalPath.view();
        if (isAbsolute(path) or isDriveRelative(path) or isRootedRelative(path))
        {
            return true;
        }
        const wchar_t* data   = path.getNullTerminatedNative();
        const size_t   length = path.sizeInBytes() / sizeof(wchar_t);
        for (size_t idx = 0; idx < length; ++idx)
        {
            if (data[idx] == L'\\' or data[idx] == L'/')
            {
                return true;
            }
        }
        return length > 0 and data[0] == L'.';
    }

    static bool isAbsolute(StringSpan path) { return isDriveAbsolute(path) or isUNC(path); }

  private:
    static bool isDriveAbsolute(StringSpan path)
    {
        const wchar_t* data   = path.getNullTerminatedNative();
        const size_t   length = path.sizeInBytes() / sizeof(wchar_t);
        return length >= 3 and isDriveLetter(data[0]) and data[1] == L':' and data[2] == L'\\';
    }

    static bool isUNC(StringSpan path)
    {
        const wchar_t* data   = path.getNullTerminatedNative();
        const size_t   length = path.sizeInBytes() / sizeof(wchar_t);
        if (length < 5 or data[0] != L'\\' or data[1] != L'\\')
        {
            return false;
        }
        return hasUNCServerAndShare(data + 2, length - 2);
    }

    static bool isRootedRelative(StringSpan path)
    {
        const wchar_t* data   = path.getNullTerminatedNative();
        const size_t   length = path.sizeInBytes() / sizeof(wchar_t);
        return length > 0 and data[0] == L'\\' and (length == 1 or data[1] != L'\\');
    }

    static bool isDriveRelative(StringSpan path)
    {
        const wchar_t* data   = path.getNullTerminatedNative();
        const size_t   length = path.sizeInBytes() / sizeof(wchar_t);
        return length >= 2 and isDriveLetter(data[0]) and data[1] == L':' and (length == 2 or data[2] != L'\\');
    }

    static bool sameDrive(StringSpan basePath, StringSpan relativePath)
    {
        const wchar_t* baseData = basePath.getNullTerminatedNative();
        const wchar_t* relData  = relativePath.getNullTerminatedNative();
        return isDriveLetter(baseData[0]) and towupper(baseData[0]) == towupper(relData[0]);
    }

    static WindowsPathResult copyDriveRoot(StringSpan path, StringPath& rootPath)
    {
        const wchar_t* pathData = path.getNullTerminatedNative();
        if (not rootPath.resize(0))
            return WindowsPathError::CapacityExceeded;
        if (not rootPath.append(StringSpan({pathData, 2}, false, StringEncoding::Utf16)))
            return WindowsPathError::CapacityExceeded;
        if (not rootPath.append(L"\\"))
            return WindowsPathError::CapacityExceeded;
        return {};
    }

    static WindowsPathResult copyRootOnly(StringSpan basePath, StringPath& rootPath)
    {
        if (isDriveAbsolute(basePath))
        {
            return copyDriveRoot(basePath, rootPath);
        }

        const wchar_t* data   = basePath.getNullTerminatedNative();
        const size_t   length = basePath.sizeInBytes() / sizeof(wchar_t);
        size_t         offset = 2;
        while (offset < length and data[offset] != L'\\')
            offset += 1;
        if (offset >= length)
            return WindowsPathError::MalformedPath;
        offset += 1;
        while (offset < length and data[offset] != L'\\')
            offset += 1;
        if (offset > length)
            return WindowsPathError::MalformedPath;
        if (offset < length)
            offset += 1;
        if (not rootPath.resize(0))
            return WindowsPathError::CapacityExceeded;
        if (not rootPath.append(StringSpan({data, offset}, false, StringEncoding::Utf16)))
            return WindowsPathError::CapacityExceeded;
        return {};
    }

    static WindowsPathResult copyLogicalPathWithoutTransportPrefix(StringSpan inputPath, StringPath& logicalPath)
    {
        const wchar_t* pathData       = inputPath.getNullTerminatedNative();
        const size_t   length         = inputPath.sizeInBytes() / sizeof(wchar_t);
        const bool     hasWin32Prefix = length >= 4 and pathData[0] == L'\\' and pathData[1] == L'\\' and
                                    pathData[2] == L'?' and pathData[3] == L'\\';
        const bool hasNtPrefix = length >= 4 and pathData[0] == L'\\' and pathData[1] == L'?' and
                                 pathData[2] == L'?' and pathData[3] == L'\\';
        if (hasWin32Prefix or hasNtPrefix)
        {
            if (length >= 8 and (pathData[4] == L'U' or pathData[4] == L'u') and
                (pathData[5] == L'N' or pathData[5] == L'n') and (pathData[6] == L'C' or pathData[6] == L'c') and
                pathData[7] == L'\\')
            {
                if (not hasUNCServerAndShare(pathData + 8, length - 8))
                    return WindowsPathError::MalformedPath;
                if (not logicalPath.resize(0))
                    return WindowsPathError::CapacityExceeded;
                if (not logicalPath.append(L"\\\\"))
                    return WindowsPathError::CapacityExceeded;
                if (not logicalPath.append(StringSpan({pathData + 8, length - 8}, true, StringEncoding::Utf16)))
                    return WindowsPathError::CapacityExceeded;
                return {};
            }
            if (length >= 7 and isDriveLetter(pathData[4]) and pathData[5] == L':' and pathData[6] == L'\\')
            {
                if (not logicalPath.assign(StringSpan({pathData + 4, length - 4}, true, StringEncoding::Utf16)))
                    return WindowsPathError::CapacityExceeded;
                return {};
            }
            return WindowsPathError::MalformedPath;
        }

        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'\\' and pathData[2] == L'?')
        {
            return WindowsPathError::MalformedPath;
        }
        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'?' and pathData[2] == L'?')
        {
            return WindowsPathError::MalformedPath;
        }
        if (not logicalPath.assign(inputPath))
            return WindowsPathError::CapacityExceeded;
        return {};
    }

    static WindowsPathResult appendRelativePath(StringPath& output, StringSpan relativePath)
    {
        const wchar_t* relativeData   = relativePath.getNullTerminatedNative();
        const size_t   relativeLength = relativePath.sizeInBytes() / sizeof(wchar_t);
        if (relativeLength == 0)
        {
            return {};
        }

        const wchar_t* outputData   = output.view().getNullTerminatedNative();
        const size_t   outputLength = output.view().sizeInBytes() / sizeof(wchar_t);
        if (outputLength > 0 and outputData[outputLength - 1] != L'\\' and relativeData[0] != L'\\')
        {
            if (not output.append(L"\\"))
                return WindowsPathError::CapacityExceeded;
        }
        if (not output.append(relativePath))
            return WindowsPathError::CapacityExceeded;
        return {};
    }

    static size_t absoluteRootLength(StringSpan path)
    {
        const wchar_t* data   = path.getNullTerminatedNative();
        const size_t   length = path.sizeInBytes() / sizeof(wchar_t);
        if (isDriveAbsolute(path))
        {
            return 3;
        }
        if (length < 5 or data[0] != L'\\' or data[1] != L'\\')
        {
            return 0;
        }

        size_t firstSeparator = 2;
        while (firstSeparator < length and data[firstSeparator] != L'\\')
            firstSeparator += 1;
        if (firstSeparator == 2 or firstSeparator >= length - 1)
        {
            return 0;
        }
        size_t secondSeparator = firstSeparator + 1;
        while (secondSeparator < length and data[secondSeparator] != L'\\')
            secondSeparator += 1;
        if (secondSeparator <= firstSeparator + 1)
        {
            return 0;
        }
        return secondSeparator < length ? secondSeparator + 1 : secondSeparator;
    }

    static WindowsPathResult appendPathSlice(StringPath& output, const wchar_t* data, size_t start, size_t end)
    {
        if (start >= end)
        {
            return {};
        }
        if (not output.append(StringSpan({data + start, end - start}, false, StringEncoding::Utf16)))
            return WindowsPathError::CapacityExceeded;
        return {};
    }

    static WindowsPathResult appendNormalizedSegment(StringPath& output, const wchar_t* data, size_t start, size_t end,
                                                     size_t* segmentStarts, size_t& segmentCount)
    {
        if (start >= end)
        {
            return {};
        }
        if (end == start + 1 and data[start] == L'.')
        {
            return {};
        }
        if (end == start + 2 and data[start] == L'.' and data[start + 1] == L'.')
        {
            if (segmentCount > 0)
            {
                if (not output.resize(segmentStarts[--segmentCount]))
                    return WindowsPathError::CapacityExceeded;
            }
            return {};
        }

        const wchar_t* outputData       = output.view().getNullTerminatedNative();
        size_t         outputLength     = output.view().sizeInBytes() / sizeof(wchar_t);
        const size_t   segmentStartSize = outputLength;
        if (outputLength > 0 and outputData[outputLength - 1] != L'\\')
        {
            if (not output.append(L"\\"))
                return WindowsPathError::CapacityExceeded;
        }
        SC_TRY(appendPathSlice(output, data, start, end));
        segmentStarts[segmentCount++] = segmentStartSize;
        return {};
    }

    static WindowsPathResult normalizeAbsolutePath(StringSpan inputPath, StringPath& normalizedPath)
    {
        const wchar_t* data       = inputPath.getNullTerminatedNative();
        const size_t   length     = inputPath.sizeInBytes() / sizeof(wchar_t);
        const size_t   rootLength = absoluteRootLength(inputPath);
        if (rootLength == 0)
            return WindowsPathError::BasePathNotAbsolute;

        if (not normalizedPath.resize(0))
            return WindowsPathError::CapacityExceeded;
        SC_TRY(appendPathSlice(normalizedPath, data, 0, rootLength));

        size_t segmentStarts[LogicalCapacity] = {};
        size_t segmentCount                   = 0;
        size_t segmentStart                   = rootLength;
        while (segmentStart < length)
        {
            while (segmentStart < length and data[segmentStart] == L'\\')
                segmentStart += 1;
            size_t segmentEnd = segmentStart;
            while (segmentEnd < length and data[segmentEnd] != L'\\')
                segmentEnd += 1;
            SC_TRY(
                appendNormalizedSegment(normalizedPath, data, segmentStart, segmentEnd, segmentStarts, segmentCount));
            segmentStart = segmentEnd;
        }
        return {};
    }

    static WindowsPathResult stripTransportPrefix(StringPath& logicalPath)
    {
        wchar_t*   pathData       = logicalPath.writableSpan().data();
        size_t     length         = logicalPath.view().sizeInBytes() / sizeof(wchar_t);
        const bool hasWin32Prefix = length >= 4 and pathData[0] == L'\\' and pathData[1] == L'\\' and
                                    pathData[2] == L'?' and pathData[3] == L'\\';
        const bool hasNtPrefix = length >= 4 and pathData[0] == L'\\' and pathData[1] == L'?' and
                                 pathData[2] == L'?' and pathData[3] == L'\\';
        if (hasWin32Prefix or hasNtPrefix)
        {
            if (length >= 8 and (pathData[4] == L'U' or pathData[4] == L'u') and
                (pathData[5] == L'N' or pathData[5] == L'n') and (pathData[6] == L'C' or pathData[6] == L'c') and
                pathData[7] == L'\\')
            {
                if (not hasUNCServerAndShare(pathData + 8, length - 8))
                    return WindowsPathError::MalformedPath;
                ::memmove(pathData + 2, pathData + 8, (length - 8 + 1) * sizeof(wchar_t));
                pathData[0] = L'\\';
                pathData[1] = L'\\';
                if (not logicalPath.resize(length - 6))
                    return WindowsPathError::CapacityExceeded;
                return {};
            }
            if (length >= 7 and isDriveLetter(pathData[4]) and pathData[5] == L':' and pathData[6] == L'\\')
            {
                ::memmove(pathData, pathData + 4, (length - 4 + 1) * sizeof(wchar_t));
                if (not logicalPath.resize(length - 4))
                    return WindowsPathError::CapacityExceeded;
                return {};
            }
            return WindowsPathError::MalformedPath;
        }

        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'\\' and pathData[2] == L'?')
        {
            return WindowsPathError::MalformedPath;
        }
        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'?' and pathData[2] == L'?')
        {
            return WindowsPathError::MalformedPath;
        }
        return {};
    }

    static bool hasUNCServerAndShare(const wchar_t* data, size_t length)
    {
        size_t firstSeparator = 0;
        while (firstSeparator < length and data[firstSeparator] != L'\\')
            firstSeparator += 1;
        if (firstSeparator == 0 or firstSeparator >= length - 1)
        {
            return false;
        }
        size_t secondSeparator = firstSeparator + 1;
        while (secondSeparator < length and data[secondSeparator] != L'\\')
            secondSeparator += 1;
        return secondSeparator > firstSeparator + 1;
    }

    static bool isDriveLetter(wchar_t character)
    {
        return (character >= L'a' and character <= L'z') or (character >= L'A' and character <= L'Z');
    }

    template <typename PathBuffer>
    static void canonicalizeSeparators(PathBuffer& path)
    {
        wchar_t*     pathData = path.writableSpan().data();
        const size_t length   = path.view().sizeInBytes() / sizeof(wchar_t);
        for (size_t idx = 0; idx < length; ++idx)
        {
            if (pathData[idx] == L'/')
            {
                pathData[idx] = L'\\';
            }
        }
    }
};
