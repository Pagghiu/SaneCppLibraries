// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "FileSystemWatcherKqueueInventory.h"

#include <string.h>

namespace SC
{
/// @brief Reconciles a BSD vnode inventory after a directory-change hint.
/// @details A successful scan provides one relative path when exactly one distinct path changed. Multiple, transient,
/// or unrepresentable changes produce a conservative root hint. Unambiguous moves may preserve existing native
/// registrations; ambiguous moves are rediscovered. A native or capacity failure stops the inventory so the caller
/// cannot silently continue with incomplete coverage. All supplied buffers and inventory storage must be disjoint,
/// and the caller must serialize this operation with native event dispatch and inventory lifecycle changes.
struct FileSystemWatcherKqueueReconcile
{
    using Inventory = FileSystemWatcherKqueueInventory;
    using State     = Inventory::State;
    using Reader    = Inventory::Reader;
    using Result    = Inventory::Result;

    struct Change
    {
        bool   hasPath        = false;
        bool   passiveAdded   = false;
        bool   rescanRequired = false;
        size_t pathLength     = 0;
    };

    /// @brief Reconciles the watched tree and returns a path or conservative root hint in caller-owned storage.
    /// @note Marks must contain at least one byte per inventory record. A failed precondition leaves the inventory
    /// active; a scan, native or capacity failure stops it and reports any additional cleanup error.
    static Result run(Inventory& inventory, Span<char> scan, Span<uint8_t> marks, Span<char> path, Change& change)
    {
        change = {};
        if (not inventory.inUse)
            return {Inventory::Error::NotStarted};
        if (scan.data() == nullptr or scan.sizeInBytes() < Reader::MinimumStorageBytes or marks.data() == nullptr or
            marks.sizeInElements() < inventory.storage.sizeInElements() or
            (path.data() == nullptr and not path.empty()))
            return {Inventory::Error::InvalidStorage};
        struct Region
        {
            const void* data;
            size_t      size;
        };
        const Region regions[] = {{inventory.storage.data(), inventory.storage.sizeInBytes()},
                                  {inventory.nameStorage.data(), inventory.nameStorage.sizeInBytes()},
                                  {scan.data(), scan.sizeInBytes()},
                                  {marks.data(), marks.sizeInBytes()},
                                  {path.data(), path.sizeInBytes()}};
        for (size_t first = 0; first < sizeof(regions) / sizeof(regions[0]); ++first)
            for (size_t second = first + 1; second < sizeof(regions) / sizeof(regions[0]); ++second)
                if (Inventory::overlaps(regions[first].data, regions[first].size, regions[second].data,
                                        regions[second].size))
                    return {Inventory::Error::InvalidStorage};

        Accumulator accumulator{path, change};
        Result      result = reconcile(inventory, scan, marks, accumulator);
        if (result.error == Inventory::Error::EntryChanged and result.cleanupError == Inventory::Error::None)
        {
            // Keep the parent registrations active and rebuild from the partially updated inventory.
            // One retry lets a settled replacement recover while bounding work under continuous churn.
            change.rescanRequired = true;
            accumulator.broad     = true;
            result                = reconcile(inventory, scan, marks, accumulator);
        }
        if (not result)
        {
            change = {};
            return inventory.fail(result);
        }
        accumulator.finish();
        return {};
    }

  private:
    struct Accumulator;

    static Result reconcile(Inventory& inventory, Span<char> scan, Span<uint8_t> marks, Accumulator& accumulator)
    {
        ::memset(marks.data(), 0, inventory.storage.sizeInElements());
        marks[inventory.root] = Seen | Pending;
        Result result         = walk(inventory, scan, marks, accumulator, false);
        if (result)
            result = retireMissing(inventory, marks, accumulator);
        if (result)
        {
            for (size_t index = 0; index < inventory.storage.sizeInElements(); ++index)
                marks[index] &= Seen;
            marks[inventory.root] |= Pending | Verified;
            result = walk(inventory, scan, marks, accumulator, true);
        }
        if (result)
        {
            // A child can disappear after the first pass. The second pass already verified the
            // surviving tree, so retire the absent child and its registrations now.
            for (size_t index = 0; index < inventory.storage.sizeInElements(); ++index)
                marks[index] = (marks[index] & Verified) != 0 ? Seen : 0;
            result = retireMissing(inventory, marks, accumulator);
        }
        return result;
    }

    static constexpr uint8_t Seen     = 1;
    static constexpr uint8_t Pending  = 2;
    static constexpr uint8_t Scanned  = 4;
    static constexpr uint8_t Verified = 8;

    struct Accumulator
    {
        Span<char> path;
        Change&    change;

        bool recorded = false;
        bool broad    = false;

        void record(const State& state, size_t index)
        {
            if (broad)
                return;
            // StringPath on the tested BSDs stores at most 4096 path bytes.
            char candidate[4097];
            if (state.copyRelativePath(index, candidate) != State::Error::None)
            {
                broad = true;
                return;
            }
            const size_t length = ::strlen(candidate);
            if (not recorded)
            {
                recorded = true;
                if (path.data() == nullptr or path.sizeInBytes() <= length)
                {
                    broad = true;
                    return;
                }
                ::memcpy(path.data(), candidate, length + 1);
                change.hasPath    = true;
                change.pathLength = length;
            }
            else if (not change.hasPath or change.pathLength != length or ::memcmp(path.data(), candidate, length) != 0)
                broad = true;
        }

        void finish()
        {
            if (not recorded or broad)
            {
                change.hasPath    = false;
                change.pathLength = 0;
                if (path.data() != nullptr and not path.empty())
                    path[0] = '\0';
            }
        }
    };

    enum class Match
    {
        Missing,
        Same,
        Replaced,
    };

    static Result matchEntry(int parent, Reader::Entry name, const State::NativeInfo& old, Match& match)
    {
        struct stat current;

        int status;
        do
            status = ::fstatat(parent, name.name, &current, AT_SYMLINK_NOFOLLOW);
        while (status == -1 and errno == EINTR);
        if (status == -1)
        {
            if (errno == ENOENT)
            {
                match = Match::Missing;
                return {};
            }
            return {Inventory::Error::EntryStat, Inventory::Error::None, errno};
        }
        const bool directory = S_ISDIR(current.st_mode);
        const bool watchable = directory or S_ISREG(current.st_mode);

        const bool same = static_cast<uint64_t>(current.st_dev) == old.device and
                          static_cast<uint64_t>(current.st_ino) == old.inode and directory == old.directory and
                          watchable == (old.descriptor != -1);
        match = same ? Match::Same : Match::Replaced;
        return {};
    }

    static bool isDescendant(const Inventory& inventory, size_t index, size_t ancestor)
    {
        for (size_t depth = 0; depth < inventory.storage.sizeInElements(); ++depth)
        {
            if (index == ancestor)
                return true;
            const State::Entry* entry = inventory.state.get(index);
            if (entry == nullptr or entry->parent == State::InvalidIndex)
                return false;
            index = entry->parent;
        }
        return false;
    }

    static Result removeSubtree(Inventory& inventory, size_t subtree, Span<uint8_t> marks)
    {
        Result cleanup;
        while (inventory.state.get(subtree) != nullptr)
        {
            bool removed = false;
            for (size_t index = 0; index < inventory.storage.sizeInElements(); ++index)
            {
                if (inventory.state.get(index) == nullptr or not isDescendant(inventory, index, subtree))
                    continue;
                State::NativeInfo  retired;
                const State::Error error = inventory.state.removeLeaf(index, retired);
                if (error == State::Error::HasChildren)
                    continue;
                if (error != State::Error::None)
                {
                    cleanup.error     = Inventory::Error::Cleanup;
                    cleanup.pathError = error;
                    return cleanup;
                }
                removed      = true;
                marks[index] = 0;
                if (retired.descriptor == -1)
                {
                    if (inventory.passiveCount == 0)
                    {
                        cleanup.error = Inventory::Error::Cleanup;
                        return cleanup;
                    }
                    --inventory.passiveCount;
                }
                else if (::close(retired.descriptor) == -1 and cleanup)
                {
                    cleanup.error       = Inventory::Error::Cleanup;
                    cleanup.nativeError = errno;
                }
            }
            if (not removed)
            {
                cleanup.error = Inventory::Error::Cleanup;
                return cleanup;
            }
        }
        return cleanup;
    }

    static Result walk(Inventory& inventory, Span<char> scan, Span<uint8_t> marks, Accumulator& changes, bool discover)
    {
        for (;;)
        {
            size_t directory = State::InvalidIndex;
            for (size_t index = 0; index < inventory.storage.sizeInElements(); ++index)
            {
                if ((marks[index] & Pending) != 0 and inventory.state.get(index) != nullptr)
                {
                    directory = index;
                    break;
                }
            }
            if (directory == State::InvalidIndex)
                return {};
            marks[directory]    = static_cast<uint8_t>((marks[directory] | Scanned) & ~Pending);
            const Result result = discover ? discoverDirectory(inventory, directory, scan, marks, changes)
                                           : markDirectory(inventory, directory, scan, marks, changes);
            if (not result)
                return result;
        }
    }

    static Result findMovedEntry(Inventory& inventory, int parentFd, Reader::Entry name, Span<uint8_t> marks,
                                 size_t& moved)
    {
        moved = State::InvalidIndex;
        struct stat current;
        int         status;
        do
            status = ::fstatat(parentFd, name.name, &current, AT_SYMLINK_NOFOLLOW);
        while (status == -1 and errno == EINTR);
        if (status == -1)
        {
            if (errno == ENOENT)
                return {};
            return {Inventory::Error::EntryStat, Inventory::Error::None, errno};
        }

        const bool directory = S_ISDIR(current.st_mode);
        // Passive entries have no descriptor keeping their inode alive. Multiple hard links make a file move ambiguous.
        if (not directory and (not S_ISREG(current.st_mode) or current.st_nlink != 1))
            return {};

        size_t candidate = State::InvalidIndex;
        for (size_t index = 0; index < inventory.storage.sizeInElements(); ++index)
        {
            const State::Entry* entry = inventory.state.get(index);
            if (entry == nullptr or entry->native.descriptor == -1 or entry->native.directory != directory or
                entry->native.device != static_cast<uint64_t>(current.st_dev) or
                entry->native.inode != static_cast<uint64_t>(current.st_ino))
                continue;
            if (candidate != State::InvalidIndex)
                return {}; // Do not choose between multiple old names for one native object.
            candidate = index;
        }
        if (candidate == State::InvalidIndex or candidate == inventory.root or (marks[candidate] & Seen) != 0)
            return {};

        const State::Entry* oldEntry  = inventory.state.get(candidate);
        const State::Entry* oldParent = inventory.state.get(oldEntry->parent);
        if (oldParent == nullptr or not oldParent->native.directory or oldParent->native.descriptor == -1)
        {
            Result result{Inventory::Error::PathState};
            result.pathError = State::Error::InvalidParent;
            return result;
        }
        char oldPath[4097];
        if (inventory.state.copyRelativePath(candidate, oldPath) != State::Error::None)
            return {}; // Fall back to retiring and rediscovering an unrepresentable old path.
        const char* oldName = ::strrchr(oldPath, '/');
        oldName             = oldName == nullptr ? oldPath : oldName + 1;
        struct stat oldLocation;
        do
            status = ::fstatat(oldParent->native.descriptor, oldName, &oldLocation, AT_SYMLINK_NOFOLLOW);
        while (status == -1 and errno == EINTR);
        if (status == -1 and errno != ENOENT)
            return {Inventory::Error::EntryStat, Inventory::Error::None, errno};
        if (status == 0 and static_cast<uint64_t>(oldLocation.st_dev) == oldEntry->native.device and
            static_cast<uint64_t>(oldLocation.st_ino) == oldEntry->native.inode)
            return {}; // The old name still identifies this object, so this is an additional name.

        moved = candidate;
        return {};
    }

    static Result markDirectory(Inventory& inventory, size_t directory, Span<char> scan, Span<uint8_t> marks,
                                Accumulator& changes)
    {
        const int parentFd = inventory.state.get(directory)->native.descriptor;

        auto visitEntry = [&](Reader::Entry name) -> Result
        {
            size_t index = inventory.state.findChild(directory, {name.name, name.nameLength});
            if (index == State::InvalidIndex)
            {
                const Result found = findMovedEntry(inventory, parentFd, name, marks, index);
                if (not found)
                    return found;
                if (index == State::InvalidIndex)
                    return {};
                changes.record(inventory.state, index);
                const State::Error moved = inventory.state.updatePath(index, directory, {name.name, name.nameLength});
                if (moved == State::Error::NameCapacity or moved == State::Error::InvalidParent)
                    return {}; // The ordinary retirement pass can reclaim storage or resolve a stale parent.
                if (moved != State::Error::None)
                {
                    Result result{Inventory::Error::PathState};
                    result.pathError = moved;
                    return result;
                }
                changes.record(inventory.state, index);
            }
            const State::Entry* entry = inventory.state.get(index);
            Match               match = Match::Missing;

            const Result matched = matchEntry(parentFd, name, entry->native, match);
            if (not matched)
                return matched;
            if (match == Match::Same)
            {
                marks[index] |= Seen;
                if (entry->native.directory and (marks[index] & Scanned) == 0)
                    marks[index] |= Pending;
            }
            return {};
        };
        return Inventory::scanDirectory(parentFd, scan, visitEntry);
    }

    static Result retireMissing(Inventory& inventory, Span<uint8_t> marks, Accumulator& changes)
    {
        for (size_t index = 0; index < inventory.storage.sizeInElements(); ++index)
        {
            const State::Entry* entry = inventory.state.get(index);
            if (entry == nullptr or index == inventory.root or (marks[index] & Seen) != 0)
                continue;
            if (entry->parent == State::InvalidIndex or entry->parent >= inventory.storage.sizeInElements() or
                inventory.state.get(entry->parent) == nullptr)
            {
                Result result{Inventory::Error::PathState};
                result.pathError = State::Error::InvalidParent;
                return result;
            }
            if ((marks[entry->parent] & Seen) == 0)
                continue;
            changes.record(inventory.state, index);
            const Result removed = removeSubtree(inventory, index, marks);
            if (not removed)
                return removed;
        }
        for (size_t index = 0; index < inventory.storage.sizeInElements(); ++index)
        {
            if (inventory.state.get(index) != nullptr and (marks[index] & Seen) == 0)
            {
                Result result{Inventory::Error::PathState};
                result.pathError = State::Error::InvalidParent;
                return result;
            }
        }
        return {};
    }

    static Result discoverDirectory(Inventory& inventory, size_t directory, Span<char> scan, Span<uint8_t> marks,
                                    Accumulator& changes)
    {
        const int parentFd = inventory.state.get(directory)->native.descriptor;

        auto visitEntry = [&](Reader::Entry name) -> Result
        {
            size_t index = inventory.state.findChild(directory, {name.name, name.nameLength});

            const State::Entry* entry = inventory.state.get(index);
            if (entry != nullptr)
            {
                Match match = Match::Missing;

                const Result matched = matchEntry(parentFd, name, entry->native, match);
                if (not matched)
                    return matched;
                if (match == Match::Missing)
                    return {};
                if (match == Match::Replaced)
                    return {Inventory::Error::EntryChanged};
                marks[index] |= Verified;
                if (entry->native.directory and (marks[index] & Scanned) == 0)
                    marks[index] |= Pending;
                return {};
            }
            const Result added = inventory.addChild(directory, name, &index);
            if (not added)
                return added;
            entry = inventory.state.get(index);
            if (entry == nullptr)
            {
                changes.broad = true; // A transient name has no current path to report.
                return {};
            }
            marks[index] = Seen | Verified | (entry->native.directory ? Pending : 0);
            changes.change.passiveAdded |= entry->native.descriptor == -1;
            changes.record(inventory.state, index);
            return {};
        };
        return Inventory::scanDirectory(parentFd, scan, visitEntry);
    }
};
} // namespace SC
