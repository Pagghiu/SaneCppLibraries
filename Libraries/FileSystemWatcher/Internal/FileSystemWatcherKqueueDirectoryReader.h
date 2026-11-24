// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Common/Span.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

namespace SC
{
/// @brief Reads BSD directory entries into caller-owned storage without owning the directory descriptor.
/// @details Names refer to the supplied buffer and remain valid until the next call to next() or reset().
struct FileSystemWatcherKqueueDirectoryReader
{
    static constexpr size_t MinimumStorageBytes = 4096;

    enum class Error
    {
        None,
        NotInitialized,
        BufferTooSmall,
        RewindFailed,
        ReadFailed,
        MalformedEntry,
    };

    struct Result
    {
        Error error       = Error::None;
        int   nativeError = 0;

        explicit operator bool() const { return error == Error::None; }
    };

    struct Entry
    {
        const char* name       = nullptr;
        size_t      nameLength = 0;
    };

    /// @brief Starts a scan at the beginning of an already-open directory descriptor.
    /// @note A failed reset invalidates any previous scan. The caller retains descriptor and buffer ownership.
    /// Some filesystems require a buffer larger than MinimumStorageBytes; a native read failure is returned in that
    /// case.
    Result reset(int directoryDescriptor, Span<char> storage)
    {
        descriptor = -1;
        buffer     = {};
        position   = 0;
        bytes      = 0;
        exhausted  = false;
        if (storage.data() == nullptr or storage.sizeInBytes() < MinimumStorageBytes)
            return {Error::BufferTooSmall};
        if (::lseek(directoryDescriptor, 0, SEEK_SET) == -1)
            return {Error::RewindFailed, errno};
        descriptor = directoryDescriptor;
        buffer     = storage;
        return {};
    }

    /// @brief Returns one entry or sets hasEntry to false at the end of the scan.
    /// @note Dot entries and unused native records are omitted. Errors do not close the descriptor.
    Result next(Entry& entry, bool& hasEntry)
    {
        entry    = {};
        hasEntry = false;
        if (descriptor == -1)
            return {Error::NotInitialized};
        if (exhausted)
            return {};

        for (;;)
        {
            if (position == bytes)
            {
                const size_t capacity = buffer.sizeInBytes() > INT_MAX ? INT_MAX : buffer.sizeInBytes();
                decltype(::getdents(descriptor, buffer.data(), capacity)) count;
                do
                {
                    count = ::getdents(descriptor, buffer.data(), capacity);
                } while (count < 0 and errno == EINTR);
                if (count < 0)
                    return {Error::ReadFailed, errno};
                if (count == 0)
                {
                    exhausted = true;
                    return {};
                }
                if (static_cast<size_t>(count) > capacity)
                    return {Error::MalformedEntry};
                position = 0;
                bytes    = static_cast<size_t>(count);
            }

            constexpr size_t headerBytes = offsetof(struct dirent, d_name);
            const size_t     remaining   = bytes - position;
            if (remaining < headerBytes + 1)
                return {Error::MalformedEntry};

            // A caller-owned buffer need not be aligned for dirent. Copy the fixed header before inspecting it.
            struct dirent nativeEntry;
            ::memcpy(&nativeEntry, buffer.data() + position, headerBytes);
            const size_t recordBytes = nativeEntry.d_reclen;
            const size_t nameBytes   = nativeEntry.d_namlen;
            if (recordBytes < headerBytes + 1 or recordBytes > remaining)
                return {Error::MalformedEntry};
            if (nativeEntry.d_fileno == 0)
            {
                position += recordBytes;
                continue;
            }
            if (nameBytes == 0 or nameBytes >= recordBytes - headerBytes)
                return {Error::MalformedEntry};
            const char* name = buffer.data() + position + headerBytes;
            if (name[nameBytes] != '\0' or ::memchr(name, '\0', nameBytes) != nullptr)
                return {Error::MalformedEntry};
            position += recordBytes;
            if ((nameBytes == 1 and name[0] == '.') or (nameBytes == 2 and name[0] == '.' and name[1] == '.'))
                continue;
            entry    = {name, nameBytes};
            hasEntry = true;
            return {};
        }
    }

  private:
    int descriptor = -1;

    Span<char> buffer;

    size_t position = 0;
    size_t bytes    = 0;

    bool exhausted = false;
};
} // namespace SC
