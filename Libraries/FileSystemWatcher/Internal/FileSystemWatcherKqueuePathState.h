// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Common/Span.h"

#include <stdint.h>
#include <string.h>

namespace SC
{
/// @brief Issues nonzero, non-repeating tokens for registrations across all trees on one native event queue.
/// @note Keep this source alive across queue restarts while previously read events may still be dispatched.
struct FileSystemWatcherKqueueRegistrationTokens
{
    explicit FileSystemWatcherKqueueRegistrationTokens(uintptr_t lastIssued = 0) : lastIssued(lastIssued) {}

    FileSystemWatcherKqueueRegistrationTokens(const FileSystemWatcherKqueueRegistrationTokens&) = delete;

    FileSystemWatcherKqueueRegistrationTokens& operator=(const FileSystemWatcherKqueueRegistrationTokens&) = delete;

    bool next(uintptr_t& token)
    {
        token = 0;
        if (lastIssued == UINTPTR_MAX)
            return false;
        token = ++lastIssued;
        return true;
    }

  private:
    uintptr_t lastIssued;
};

/// @brief Bounded path and native-registration bookkeeping for a BSD watched tree.
/// @details Storage and descriptors belong to the caller. The table neither opens nor closes descriptors.
/// Mutations invalidate borrowed names. Callers must serialize access and retire a record before closing its
/// descriptor.
struct FileSystemWatcherKqueuePathState
{
    static constexpr size_t InvalidIndex = static_cast<size_t>(-1);

    FileSystemWatcherKqueuePathState() = default;

    FileSystemWatcherKqueuePathState(const FileSystemWatcherKqueuePathState&) = delete;

    FileSystemWatcherKqueuePathState& operator=(const FileSystemWatcherKqueuePathState&) = delete;

    struct NativeInfo
    {
        // Keep the flag alongside the descriptor to reduce per-entry padding.
        int  descriptor = -1;
        bool directory  = false;

        uintptr_t token  = 0;
        uint64_t  device = 0;
        uint64_t  inode  = 0;

        NativeInfo() = default;

        NativeInfo(int descriptor, uintptr_t token, uint64_t device, uint64_t inode, bool directory)
            : descriptor(descriptor), directory(directory), token(token), device(device), inode(inode)
        {}
    };

    struct Entry
    {
        NativeInfo native;

        size_t parent     = InvalidIndex;
        size_t nameOffset = 0;
        size_t nameLength = 0;

        bool active = false;
    };

    enum class Error
    {
        None,
        NotInitialized,
        NotEmpty,
        InvalidStorage,
        InvalidParent,
        InvalidName,
        DuplicateName,
        DuplicateToken,
        RecordCapacity,
        NameCapacity,
        HasChildren,
        NotFound,
        OutputTooSmall,
    };

    /// @brief Initializes storage, or replaces it after every entry has been retired.
    Error reset(Span<Entry> entries, Span<char> names)
    {
        if (entryCount != 0)
            return Error::NotEmpty;
        if (entries.data() == nullptr or entries.empty() or (names.data() == nullptr and not names.empty()))
            return Error::InvalidStorage;
        records     = entries;
        nameStorage = names;
        namesUsed   = 0;
        root        = InvalidIndex;
        for (size_t index = 0; index < records.sizeInElements(); ++index)
            records[index] = {};
        return Error::None;
    }

    /// @brief Releases borrowed storage after every entry has been retired.
    Error release()
    {
        if (entryCount != 0)
            return Error::NotEmpty;
        records     = {};
        nameStorage = {};
        namesUsed   = 0;
        root        = InvalidIndex;
        return Error::None;
    }

    /// @brief Adds the root or a child. On failure, storage and output index remain unchanged.
    Error add(size_t parent, Span<const char> name, NativeInfo native, size_t& outputIndex)
    {
        if (records.data() == nullptr)
            return Error::NotInitialized;
        const bool isRoot = parent == InvalidIndex;
        if (isRoot)
        {
            if (root != InvalidIndex or not native.directory or not name.empty())
                return Error::InvalidParent;
        }
        else
        {
            if (get(parent) == nullptr or not records[parent].native.directory)
                return Error::InvalidParent;
            if (not isValidName(name))
                return Error::InvalidName;
            if (findChild(parent, name) != InvalidIndex)
                return Error::DuplicateName;
        }
        if (native.token != 0 and findToken(native.token) != InvalidIndex)
            return Error::DuplicateToken;
        if (name.sizeInBytes() > nameStorage.sizeInBytes() - namesUsed)
            return Error::NameCapacity;

        size_t freeIndex = InvalidIndex;
        for (size_t index = 0; index < records.sizeInElements(); ++index)
        {
            if (not records[index].active)
            {
                freeIndex = index;
                break;
            }
        }
        if (freeIndex == InvalidIndex)
            return Error::RecordCapacity;

        if (not name.empty())
            ::memmove(nameStorage.data() + namesUsed, name.data(), name.sizeInBytes());
        Entry& entry     = records[freeIndex];
        entry.native     = native;
        entry.parent     = parent;
        entry.nameOffset = namesUsed;
        entry.nameLength = name.sizeInBytes();
        entry.active     = true;
        namesUsed += name.sizeInBytes();
        ++entryCount;
        if (isRoot)
            root = freeIndex;
        outputIndex = freeIndex;
        return Error::None;
    }

    /// @brief Moves or renames a live subtree without changing any descriptor or token.
    /// @note The new name must not overlap name storage. Failures do not change the table.
    Error updatePath(size_t index, size_t newParent, Span<const char> name)
    {
        const Entry* oldEntry = get(index);
        if (oldEntry == nullptr)
            return Error::NotFound;
        if (index == root or get(newParent) == nullptr or not records[newParent].native.directory)
            return Error::InvalidParent;
        for (size_t ancestor = newParent; ancestor != InvalidIndex; ancestor = records[ancestor].parent)
            if (ancestor == index)
                return Error::InvalidParent;
        if (not isValidName(name) or overlapsNameStorage(name))
            return Error::InvalidName;
        const size_t existing = findChild(newParent, name);
        if (existing == index)
            return Error::None;
        if (existing != InvalidIndex)
            return Error::DuplicateName;

        const size_t oldOffset = oldEntry->nameOffset;
        const size_t oldLength = oldEntry->nameLength;
        const size_t newLength = name.sizeInBytes();
        if (newLength > oldLength and newLength - oldLength > nameStorage.sizeInBytes() - namesUsed)
            return Error::NameCapacity;

        resizeName(index, newLength);
        ::memcpy(nameStorage.data() + oldOffset, name.data(), newLength);
        records[index].nameLength = newLength;
        records[index].parent     = newParent;
        return Error::None;
    }

    /// @brief Retires one leaf and returns its native data so the caller can remove its watch and close its descriptor.
    /// @note The output must not alias the supplied record storage.
    Error removeLeaf(size_t index, NativeInfo& retired)
    {
        const Entry* entry = get(index);
        if (entry == nullptr)
            return Error::NotFound;
        for (size_t child = 0; child < records.sizeInElements(); ++child)
            if (records[child].active and records[child].parent == index)
                return Error::HasChildren;

        retired = entry->native;

        resizeName(index, 0);
        records[index] = {};
        --entryCount;
        if (index == root)
            root = InvalidIndex;
        return Error::None;
    }

    const Entry* get(size_t index) const
    {
        return index < records.sizeInElements() and records[index].active ? &records[index] : nullptr;
    }

    size_t findToken(uintptr_t token) const
    {
        if (token != 0)
            for (size_t index = 0; index < records.sizeInElements(); ++index)
                if (records[index].active and records[index].native.token == token)
                    return index;
        return InvalidIndex;
    }

    size_t findChild(size_t parent, Span<const char> name) const
    {
        if (name.data() == nullptr or name.empty())
            return InvalidIndex;
        for (size_t index = 0; index < records.sizeInElements(); ++index)
        {
            const Entry& entry = records[index];
            if (entry.active and entry.parent == parent and entry.nameLength == name.sizeInBytes() and
                ::memcmp(nameStorage.data() + entry.nameOffset, name.data(), entry.nameLength) == 0)
                return index;
        }
        return InvalidIndex;
    }

    /// @brief Copies the relative path and a terminator, leaving output untouched on failure.
    /// @note Output must not overlap the supplied name storage.
    Error copyRelativePath(size_t index, Span<char> output) const
    {
        if (get(index) == nullptr)
            return Error::NotFound;
        if (output.data() == nullptr or output.empty())
            return Error::OutputTooSmall;
        size_t required = 1;
        for (size_t current = index; current != root; current = records[current].parent)
        {
            const Entry& entry = records[current];
            if (entry.nameLength > output.sizeInBytes() - required)
                return Error::OutputTooSmall;
            required += entry.nameLength;
            if (entry.parent != root)
            {
                if (required == output.sizeInBytes())
                    return Error::OutputTooSmall;
                ++required;
            }
        }
        size_t position  = required - 1;
        output[position] = '\0';
        for (size_t current = index; current != root; current = records[current].parent)
        {
            const Entry& entry = records[current];
            position -= entry.nameLength;
            ::memcpy(output.data() + position, nameStorage.data() + entry.nameOffset, entry.nameLength);
            if (entry.parent != root)
                output[--position] = '/';
        }
        return Error::None;
    }

    size_t size() const { return entryCount; }

  private:
    // Open or close a gap in the packed names and adjust only the records in its tail.
    void resizeName(size_t index, size_t newLength)
    {
        const Entry& entry = records[index];
        if (entry.nameLength == newLength)
            return;
        const size_t oldEnd = entry.nameOffset + entry.nameLength;
        const size_t newEnd = entry.nameOffset + newLength;
        if (oldEnd < namesUsed)
            ::memmove(nameStorage.data() + newEnd, nameStorage.data() + oldEnd, namesUsed - oldEnd);
        for (size_t other = 0; other < records.sizeInElements(); ++other)
        {
            if (other != index and records[other].active and records[other].nameOffset >= oldEnd)
                records[other].nameOffset = records[other].nameOffset - oldEnd + newEnd;
        }
        namesUsed = namesUsed - oldEnd + newEnd;
    }

    static bool isValidName(Span<const char> name)
    {
        if (name.data() == nullptr or name.empty())
            return false;
        if ((name.sizeInBytes() == 1 and name[0] == '.') or
            (name.sizeInBytes() == 2 and name[0] == '.' and name[1] == '.'))
            return false;
        for (size_t index = 0; index < name.sizeInBytes(); ++index)
            if (name[index] == '\0' or name[index] == '/')
                return false;
        return true;
    }

    bool overlapsNameStorage(Span<const char> name) const
    {
        if (nameStorage.empty())
            return false;
        const uintptr_t firstName    = reinterpret_cast<uintptr_t>(name.data());
        const uintptr_t firstStorage = reinterpret_cast<uintptr_t>(nameStorage.data());
        return firstName >= firstStorage ? firstName - firstStorage < nameStorage.sizeInBytes()
                                         : firstStorage - firstName < name.sizeInBytes();
    }

    Span<Entry> records;
    Span<char>  nameStorage;

    size_t entryCount = 0;
    size_t namesUsed  = 0;
    size_t root       = InvalidIndex;
};
} // namespace SC
