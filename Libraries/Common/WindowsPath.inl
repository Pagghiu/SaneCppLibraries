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

struct WindowsPath
{
    static constexpr size_t LogicalCapacity   = StringPath::MaxPath;
    static constexpr size_t TransportCapacity = StringPath::MaxPath + 6;

    using TransportString = StringNativeBuffer<TransportCapacity + 1>;

    static Result makeLogicalPath(StringSpan input, StringPath& logicalPath)
    {
        TransportString inputPath;
        SC_TRY_MSG(inputPath.assign(input), "Path exceeds SC::StringPath limit (1024)");
        canonicalizeSeparators(inputPath);
        return copyLogicalPathWithoutTransportPrefix(inputPath.view(), logicalPath);
    }

    static Result makeAbsoluteLogicalPath(StringSpan input, StringSpan baseDirectory, StringPath& logicalPath)
    {
        SC_TRY(makeLogicalPath(input, logicalPath));
        if (isAbsolute(logicalPath.view()))
        {
            return Result(true);
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

        SC_TRY_MSG(isAbsolute(basePath.view()), "Windows long-path base directory must be absolute");

        StringPath joinedPath;
        SC_TRY_MSG(joinedPath.assign(basePath.view()), "Path exceeds SC::StringPath limit (1024)");

        const wchar_t* logicalData = logicalPath.view().getNullTerminatedNative();
        if (isRootedRelative(logicalPath.view()))
        {
            SC_TRY(copyRootOnly(basePath.view(), joinedPath));
            SC_TRY_MSG(joinedPath.append(logicalPath.view()), "Path exceeds SC::StringPath limit (1024)");
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

    static Result makeTransportPath(StringSpan input, StringSpan baseDirectory, StringPath& logicalPath,
                                    TransportString& transportPath)
    {
        SC_TRY(makeAbsoluteLogicalPath(input, baseDirectory, logicalPath));
        if (not needsTransportPrefix(logicalPath.view()))
        {
            transportPath.clear();
            SC_TRY_MSG(transportPath.assign(logicalPath.view()), "Path exceeds SC::StringPath limit (1024)");
            return Result(true);
        }
        return appendTransportPrefix(logicalPath.view(), transportPath);
    }

    static Result appendTransportPrefix(StringSpan logicalPath, TransportString& transportPath)
    {
        transportPath.clear();
        if (isUNC(logicalPath))
        {
            SC_TRY_MSG(transportPath.append(L"\\\\?\\UNC\\"), "Path exceeds SC::StringPath limit (1024)");
            const wchar_t* pathData = logicalPath.getNullTerminatedNative();
            SC_TRY_MSG(transportPath.append(StringSpan({pathData + 2, logicalPath.sizeInBytes() / sizeof(wchar_t) - 2},
                                                       true, StringEncoding::Utf16)),
                       "Path exceeds SC::StringPath limit (1024)");
        }
        else
        {
            SC_TRY_MSG(transportPath.append(L"\\\\?\\"), "Path exceeds SC::StringPath limit (1024)");
            SC_TRY_MSG(transportPath.append(logicalPath), "Path exceeds SC::StringPath limit (1024)");
        }
        return Result(true);
    }

    static bool needsTransportPrefix(StringSpan logicalPath)
    {
        // Directory creation needs extended spelling before the nominal MAX_PATH boundary.
        return logicalPath.sizeInBytes() / sizeof(wchar_t) >= MAX_PATH - 12;
    }

    static Result getExecutablePath(StringPath& executablePath)
    {
        DWORD length = ::GetModuleFileNameW(nullptr, executablePath.writableSpan().data(),
                                            static_cast<DWORD>(StringPath::StorageCapacity));
        if (length == 0 || length >= StringPath::StorageCapacity)
        {
            (void)executablePath.resize(0);
            return Result::Error("Path exceeds SC::StringPath limit (1024)");
        }
        SC_TRY_MSG(executablePath.resize(length), "Path exceeds SC::StringPath limit (1024)");
        canonicalizeSeparators(executablePath);
        return stripTransportPrefix(executablePath);
    }

    static Result getCurrentDirectory(StringPath& currentWorkingDirectory)
    {
        DWORD length = ::GetCurrentDirectoryW(static_cast<DWORD>(StringPath::StorageCapacity),
                                              currentWorkingDirectory.writableSpan().data());
        if (length == 0 || length >= StringPath::StorageCapacity)
        {
            (void)currentWorkingDirectory.resize(0);
            return Result::Error("Path exceeds SC::StringPath limit (1024)");
        }
        SC_TRY_MSG(currentWorkingDirectory.resize(length), "Path exceeds SC::StringPath limit (1024)");
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

    static Result copyDriveRoot(StringSpan path, StringPath& rootPath)
    {
        const wchar_t* pathData = path.getNullTerminatedNative();
        SC_TRY_MSG(rootPath.resize(0), "Path exceeds SC::StringPath limit (1024)");
        SC_TRY_MSG(rootPath.append(StringSpan({pathData, 2}, false, StringEncoding::Utf16)),
                   "Path exceeds SC::StringPath limit (1024)");
        SC_TRY_MSG(rootPath.append(L"\\"), "Path exceeds SC::StringPath limit (1024)");
        return Result(true);
    }

    static Result copyRootOnly(StringSpan basePath, StringPath& rootPath)
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
        SC_TRY_MSG(offset < length, "Malformed Windows UNC path");
        offset += 1;
        while (offset < length and data[offset] != L'\\')
            offset += 1;
        SC_TRY_MSG(offset <= length, "Malformed Windows UNC path");
        if (offset < length)
            offset += 1;
        SC_TRY_MSG(rootPath.resize(0), "Path exceeds SC::StringPath limit (1024)");
        SC_TRY_MSG(rootPath.append(StringSpan({data, offset}, false, StringEncoding::Utf16)),
                   "Path exceeds SC::StringPath limit (1024)");
        return Result(true);
    }

    static Result copyLogicalPathWithoutTransportPrefix(StringSpan inputPath, StringPath& logicalPath)
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
                SC_TRY_MSG(hasUNCServerAndShare(pathData + 8, length - 8), "Malformed Windows long-path prefix");
                SC_TRY_MSG(logicalPath.resize(0), "Path exceeds SC::StringPath limit (1024)");
                SC_TRY_MSG(logicalPath.append(L"\\\\"), "Path exceeds SC::StringPath limit (1024)");
                SC_TRY_MSG(logicalPath.append(StringSpan({pathData + 8, length - 8}, true, StringEncoding::Utf16)),
                           "Path exceeds SC::StringPath limit (1024)");
                return Result(true);
            }
            if (length >= 7 and isDriveLetter(pathData[4]) and pathData[5] == L':' and pathData[6] == L'\\')
            {
                SC_TRY_MSG(logicalPath.assign(StringSpan({pathData + 4, length - 4}, true, StringEncoding::Utf16)),
                           "Path exceeds SC::StringPath limit (1024)");
                return Result(true);
            }
            return Result::Error("Malformed Windows long-path prefix");
        }

        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'\\' and pathData[2] == L'?')
        {
            return Result::Error("Malformed Windows long-path prefix");
        }
        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'?' and pathData[2] == L'?')
        {
            return Result::Error("Malformed Windows long-path prefix");
        }
        SC_TRY_MSG(logicalPath.assign(inputPath), "Path exceeds SC::StringPath limit (1024)");
        return Result(true);
    }

    static Result appendRelativePath(StringPath& output, StringSpan relativePath)
    {
        const wchar_t* relativeData   = relativePath.getNullTerminatedNative();
        const size_t   relativeLength = relativePath.sizeInBytes() / sizeof(wchar_t);
        if (relativeLength == 0)
        {
            return Result(true);
        }

        const wchar_t* outputData   = output.view().getNullTerminatedNative();
        const size_t   outputLength = output.view().sizeInBytes() / sizeof(wchar_t);
        if (outputLength > 0 and outputData[outputLength - 1] != L'\\' and relativeData[0] != L'\\')
        {
            SC_TRY_MSG(output.append(L"\\"), "Path exceeds SC::StringPath limit (1024)");
        }
        SC_TRY_MSG(output.append(relativePath), "Path exceeds SC::StringPath limit (1024)");
        return Result(true);
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

    static Result appendPathSlice(StringPath& output, const wchar_t* data, size_t start, size_t end)
    {
        if (start >= end)
        {
            return Result(true);
        }
        return Result(output.append(StringSpan({data + start, end - start}, false, StringEncoding::Utf16)));
    }

    static Result appendNormalizedSegment(StringPath& output, const wchar_t* data, size_t start, size_t end,
                                          size_t* segmentStarts, size_t& segmentCount)
    {
        if (start >= end)
        {
            return Result(true);
        }
        if (end == start + 1 and data[start] == L'.')
        {
            return Result(true);
        }
        if (end == start + 2 and data[start] == L'.' and data[start + 1] == L'.')
        {
            if (segmentCount > 0)
            {
                SC_TRY(output.resize(segmentStarts[--segmentCount]));
            }
            return Result(true);
        }

        const wchar_t* outputData       = output.view().getNullTerminatedNative();
        size_t         outputLength     = output.view().sizeInBytes() / sizeof(wchar_t);
        const size_t   segmentStartSize = outputLength;
        if (outputLength > 0 and outputData[outputLength - 1] != L'\\')
        {
            SC_TRY_MSG(output.append(L"\\"), "Path exceeds SC::StringPath limit (1024)");
        }
        SC_TRY_MSG(appendPathSlice(output, data, start, end), "Path exceeds SC::StringPath limit (1024)");
        segmentStarts[segmentCount++] = segmentStartSize;
        return Result(true);
    }

    static Result normalizeAbsolutePath(StringSpan inputPath, StringPath& normalizedPath)
    {
        const wchar_t* data       = inputPath.getNullTerminatedNative();
        const size_t   length     = inputPath.sizeInBytes() / sizeof(wchar_t);
        const size_t   rootLength = absoluteRootLength(inputPath);
        SC_TRY_MSG(rootLength > 0, "Windows long-path base directory must be absolute");

        SC_TRY_MSG(normalizedPath.resize(0), "Path exceeds SC::StringPath limit (1024)");
        SC_TRY_MSG(appendPathSlice(normalizedPath, data, 0, rootLength), "Path exceeds SC::StringPath limit (1024)");

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
        return Result(true);
    }

    static Result stripTransportPrefix(StringPath& logicalPath)
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
                SC_TRY_MSG(hasUNCServerAndShare(pathData + 8, length - 8), "Malformed Windows long-path prefix");
                ::memmove(pathData + 2, pathData + 8, (length - 8 + 1) * sizeof(wchar_t));
                pathData[0] = L'\\';
                pathData[1] = L'\\';
                return Result(logicalPath.resize(length - 6));
            }
            if (length >= 7 and isDriveLetter(pathData[4]) and pathData[5] == L':' and pathData[6] == L'\\')
            {
                ::memmove(pathData, pathData + 4, (length - 4 + 1) * sizeof(wchar_t));
                return Result(logicalPath.resize(length - 4));
            }
            return Result::Error("Malformed Windows long-path prefix");
        }

        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'\\' and pathData[2] == L'?')
        {
            return Result::Error("Malformed Windows long-path prefix");
        }
        if (length >= 3 and pathData[0] == L'\\' and pathData[1] == L'?' and pathData[2] == L'?')
        {
            return Result::Error("Malformed Windows long-path prefix");
        }
        return Result(true);
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
