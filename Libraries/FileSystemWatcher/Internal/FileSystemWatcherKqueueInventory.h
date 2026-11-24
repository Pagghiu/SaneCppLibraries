// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "FileSystemWatcherKqueueDirectoryReader.h"
#include "FileSystemWatcherKqueuePathState.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace SC
{
/// @brief Builds an initial bounded BSD vnode inventory for one directory tree.
/// @details The queue, root descriptor, token source and storage are borrowed; descendant descriptors are owned here.
/// The caller must serialize setup, event dispatch and cleanup, and keep the supplied storage unchanged while active.
/// The root descriptor must have no other vnode registration on this queue and must remain open until stop() completes.
/// Enumeration rewinds and consumes the borrowed root descriptor's directory position.
/// A successful start requires an explicit stop().
struct FileSystemWatcherKqueueInventory
{
    using State  = FileSystemWatcherKqueuePathState;
    using Reader = FileSystemWatcherKqueueDirectoryReader;

    enum class Error
    {
        None,
        AlreadyStarted,
        NotStarted,
        InvalidRoot,
        InvalidStorage,
        PathState,
        TokenExhausted,
        DirectoryRead,
        EntryStat,
        EntryOpen,
        EntryChanged,
        Registration,
        Cleanup,
    };

    struct Result
    {
        Error error        = Error::None;
        Error cleanupError = Error::None;

        int nativeError        = 0;
        int cleanupNativeError = 0;

        State::Error  pathError = State::Error::None;
        Reader::Error readError = Reader::Error::None;

        explicit operator bool() const { return error == Error::None; }
    };

    FileSystemWatcherKqueueInventory() = default;

    FileSystemWatcherKqueueInventory(const FileSystemWatcherKqueueInventory&) = delete;

    FileSystemWatcherKqueueInventory& operator=(const FileSystemWatcherKqueueInventory&) = delete;

    /// @brief Registers the root and discovers its current descendants, rolling back on failure.
    /// @note Symlinks and other non-regular, non-directory entries are retained as passive entries. They have no direct
    /// vnode registration; the caller must decide how to expose that limitation before declaring a watch active.
    Result start(int queue, int rootDescriptor, FileSystemWatcherKqueueRegistrationTokens& tokens,
                 Span<State::Entry> entries, Span<char> names, Span<char> scanStorage)
    {
        if (inUse)
            return {Error::AlreadyStarted};
        if (entries.sizeInElements() > SIZE_MAX / sizeof(State::Entry) or
            overlaps(entries.data(), entries.sizeInBytes(), names.data(), names.sizeInBytes()) or
            overlaps(entries.data(), entries.sizeInBytes(), scanStorage.data(), scanStorage.sizeInBytes()) or
            overlaps(names.data(), names.sizeInBytes(), scanStorage.data(), scanStorage.sizeInBytes()))
            return {Error::InvalidStorage};
        struct stat rootStat;
        if (statDescriptor(rootDescriptor, rootStat) == -1)
            return {Error::InvalidRoot, Error::None, errno};
        if (not S_ISDIR(rootStat.st_mode))
            return {Error::InvalidRoot, Error::None, ENOTDIR};
        const State::Error resetError = state.reset(entries, names);
        if (resetError != State::Error::None)
        {
            Result result{Error::PathState};
            result.pathError = resetError;
            return result;
        }
        inUse        = true;
        queueFd      = queue;
        rootFd       = rootDescriptor;
        storage      = entries;
        nameStorage  = names;
        tokenSource  = &tokens;
        passiveCount = 0;

        uintptr_t rootToken = 0;
        if (not tokens.next(rootToken))
            return fail({Error::TokenExhausted});
        size_t rootIndex = State::InvalidIndex;

        const State::NativeInfo rootInfo{rootFd, rootToken, static_cast<uint64_t>(rootStat.st_dev),
                                         static_cast<uint64_t>(rootStat.st_ino), true};

        const State::Error rootError = state.add(State::InvalidIndex, {}, rootInfo, rootIndex);
        if (rootError != State::Error::None)
        {
            Result result{Error::PathState};
            result.pathError = rootError;
            return fail(result);
        }
        root = rootIndex;

        rootRegistrationAttempted = true;
        if (registerVnode(rootFd, rootToken) == -1)
            return fail({Error::Registration, Error::None, errno});

        // Initial insertion uses the first vacant slot without removing entries; children therefore follow parents.
        for (size_t index = 0; index < storage.sizeInElements(); ++index)
        {
            const State::Entry* parent = state.get(index);
            if (parent == nullptr or not parent->native.directory)
                continue;
            const Result scanned = scanDirectory(parent->native.descriptor, scanStorage,
                                                 [&](Reader::Entry name) { return addChild(index, name); });
            if (not scanned)
                return fail(scanned);
        }
        return {};
    }

    /// @brief Retires all registrations and closes descendant descriptors, leaving the borrowed root and queue open.
    /// @note If root deregistration fails, the caller must resolve it or close the root or queue after handling the
    /// error.
    Result stop()
    {
        if (not inUse)
            return {Error::NotStarted};
        Result cleanup;
        while (state.size() != 0)
        {
            bool removed = false;
            for (size_t index = 0; index < storage.sizeInElements(); ++index)
            {
                if (state.get(index) == nullptr)
                    continue;
                State::NativeInfo  retired;
                const State::Error result = state.removeLeaf(index, retired);
                if (result == State::Error::HasChildren)
                    continue;
                if (result != State::Error::None)
                {
                    cleanup.error     = Error::Cleanup;
                    cleanup.pathError = result;
                    break;
                }
                removed = true;
                if (index == root)
                {
                    if (rootRegistrationAttempted and unregisterRoot() == -1 and errno != ENOENT and cleanup)
                    {
                        cleanup.error       = Error::Cleanup;
                        cleanup.nativeError = errno;
                    }
                    rootRegistrationAttempted = false;
                }
                else if (retired.descriptor != -1 and ::close(retired.descriptor) == -1 and cleanup)
                {
                    cleanup.error       = Error::Cleanup;
                    cleanup.nativeError = errno;
                }
            }
            if (not removed)
            {
                if (cleanup)
                    cleanup.error = Error::Cleanup;
                return cleanup;
            }
        }
        const State::Error released = state.release();
        if (released != State::Error::None and cleanup)
        {
            cleanup.error     = Error::Cleanup;
            cleanup.pathError = released;
        }
        inUse        = false;
        queueFd      = -1;
        rootFd       = -1;
        root         = State::InvalidIndex;
        storage      = {};
        nameStorage  = {};
        tokenSource  = nullptr;
        passiveCount = 0;
        return cleanup;
    }

    const State& paths() const { return state; }
    size_t       rootIndex() const { return root; }
    size_t       passiveEntries() const { return passiveCount; }

  private:
    friend struct FileSystemWatcherKqueueReconcile;
    static bool overlaps(const void* first, size_t firstSize, const void* second, size_t secondSize)
    {
        if (first == nullptr or second == nullptr or firstSize == 0 or secondSize == 0)
            return false;
        const uintptr_t firstAddress  = reinterpret_cast<uintptr_t>(first);
        const uintptr_t secondAddress = reinterpret_cast<uintptr_t>(second);
        return firstAddress >= secondAddress ? firstAddress - secondAddress < secondSize
                                             : secondAddress - firstAddress < firstSize;
    }

    static int statDescriptor(int descriptor, struct stat& value)
    {
        int result;
        do
            result = ::fstat(descriptor, &value);
        while (result == -1 and errno == EINTR);
        return result;
    }

    int changeRegistration(int descriptor, short flags, uintptr_t token)
    {
        struct kevent change;
        EV_SET(&change, descriptor, EVFILT_VNODE, flags,
               NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_DELETE | NOTE_RENAME | NOTE_REVOKE, 0,
               reinterpret_cast<void*>(token));
        int result;
        do
            result = ::kevent(queueFd, &change, 1, nullptr, 0, nullptr);
        while (result == -1 and errno == EINTR);
        return result;
    }

    int registerVnode(int descriptor, uintptr_t token)
    {
        return changeRegistration(descriptor, EV_ADD | EV_CLEAR, token);
    }
    int unregisterRoot() { return changeRegistration(rootFd, EV_DELETE, 0); }

    // Setup and both reconciliation passes use the same directory-reader error mapping.
    template <typename Visit>
    static Result scanDirectory(int descriptor, Span<char> scanStorage, Visit visit)
    {
        Reader         reader;
        Reader::Result read = reader.reset(descriptor, scanStorage);
        while (read)
        {
            Reader::Entry entry;
            bool          hasEntry = false;
            read                   = reader.next(entry, hasEntry);
            if (not read)
                break;
            if (not hasEntry)
                return {};
            const Result result = visit(entry);
            if (not result)
                return result;
        }
        Result result{Error::DirectoryRead, Error::None, read.nativeError};
        result.readError = read.error;
        return result;
    }

    Result fail(Result result)
    {
        const Result cleanup = stop();
        if (not cleanup and result.cleanupError == Error::None)
        {
            result.cleanupError       = cleanup.error;
            result.cleanupNativeError = cleanup.nativeError;
        }
        return result;
    }

    Result addChild(size_t parentIndex, Reader::Entry name, size_t* addedIndex = nullptr)
    {
        const int parentFd = state.get(parentIndex)->native.descriptor;

        struct stat before;

        int status;
        do
            status = ::fstatat(parentFd, name.name, &before, AT_SYMLINK_NOFOLLOW);
        while (status == -1 and errno == EINTR);
        if (status == -1)
        {
            if (errno == ENOENT)
                return {}; // A queued parent event will cover changes after this scan.
            return {Error::EntryStat, Error::None, errno};
        }

        const bool isDirectory = S_ISDIR(before.st_mode);
        const bool watchVnode  = isDirectory or S_ISREG(before.st_mode);
        int        descriptor  = -1;
        uintptr_t  token       = 0;
        if (watchVnode)
        {
            const int flags = O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW | (isDirectory ? O_DIRECTORY : 0);
            do
                descriptor = ::openat(parentFd, name.name, flags);
            while (descriptor == -1 and errno == EINTR);
            if (descriptor == -1)
            {
                if (errno == ENOENT)
                    return {};
                return {Error::EntryOpen, Error::None, errno};
            }
            struct stat opened;
            if (statDescriptor(descriptor, opened) == -1)
            {
                const int nativeError = errno;
                Result    result{Error::EntryStat, Error::None, nativeError};
                closeUntracked(descriptor, result);
                return result;
            }
            if (before.st_dev != opened.st_dev or before.st_ino != opened.st_ino or
                (before.st_mode & S_IFMT) != (opened.st_mode & S_IFMT))
            {
                Result result{Error::EntryChanged};
                closeUntracked(descriptor, result);
                return result;
            }
            if (not tokenSource->next(token))
            {
                Result result{Error::TokenExhausted};
                closeUntracked(descriptor, result);
                return result;
            }
        }

        size_t index = State::InvalidIndex;

        const State::NativeInfo info{descriptor, token, static_cast<uint64_t>(before.st_dev),
                                     static_cast<uint64_t>(before.st_ino), isDirectory};

        const State::Error added = state.add(parentIndex, {name.name, name.nameLength}, info, index);
        if (added != State::Error::None)
        {
            Result result{Error::PathState};
            result.pathError = added;
            if (descriptor != -1)
                closeUntracked(descriptor, result);
            return result;
        }
        if (watchVnode)
        {
            if (registerVnode(descriptor, token) == -1)
                return {Error::Registration, Error::None, errno};
        }
        else
            ++passiveCount;
        if (addedIndex)
            *addedIndex = index;
        return {};
    }

    static void closeUntracked(int descriptor, Result& result)
    {
        // Both supported BSDs retire the descriptor even when close reports an error; retrying could close a reused fd.
        if (::close(descriptor) == -1)
        {
            result.cleanupError       = Error::Cleanup;
            result.cleanupNativeError = errno;
        }
    }

    State state;

    Span<State::Entry> storage;
    Span<char>         nameStorage;

    FileSystemWatcherKqueueRegistrationTokens* tokenSource = nullptr;

    int queueFd = -1;
    int rootFd  = -1;

    size_t root         = State::InvalidIndex;
    size_t passiveCount = 0;

    bool inUse = false;

    bool rootRegistrationAttempted = false;
};
} // namespace SC
