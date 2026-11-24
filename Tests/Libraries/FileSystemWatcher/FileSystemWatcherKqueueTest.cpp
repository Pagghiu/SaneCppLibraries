// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Common/PlatformMacrosType.h"

#if SC_PLATFORM_NETBSD || SC_PLATFORM_OPENBSD

#include "Libraries/Common/Deferred.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Testing/Testing.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{
enum class DirectoryReadFault
{
    None,
    Interrupted,
    Failed,
    TruncatedHeader,
    InvalidRecord,
    InvalidName,
};

struct WatcherFaults
{
    DirectoryReadFault readFault               = DirectoryReadFault::None;
    int                directory               = -1;
    int                openedChild             = -1;
    int                scanCount               = 0;
    int                movedStatCount          = 0;
    bool               removeDuringScan        = false;
    bool               replaceDuringMove       = false;
    bool               rejectChildRegistration = false;
    bool               reportChildCloseError   = false;
    bool               injected                = false;
};

WatcherFaults faults;

bool createFile(int parent, const char* name)
{
    const int fd = ::openat(parent, name, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    return fd != -1 and ::close(fd) == 0;
}

ssize_t watchedGetdents(int descriptor, char* storage, size_t capacity)
{
    if (faults.removeDuringScan and descriptor == faults.directory and ++faults.scanCount == 3)
    {
        if (::unlinkat(descriptor, "old", 0) != 0)
            return -1;
        faults.injected = true;
    }

    const auto fault = faults.readFault;
    if (fault == DirectoryReadFault::None)
        return ::getdents(descriptor, storage, capacity);

    faults.injected = true;
    if (fault == DirectoryReadFault::Interrupted)
    {
        faults.readFault = DirectoryReadFault::None;
        errno            = EINTR;
        return -1;
    }
    if (fault == DirectoryReadFault::Failed)
    {
        errno = EIO;
        return -1;
    }

    constexpr size_t headerBytes = offsetof(struct dirent, d_name);
    if (fault == DirectoryReadFault::TruncatedHeader)
        return headerBytes;
    struct dirent record = {};
    record.d_fileno      = 1;
    record.d_namlen      = 1;
    record.d_reclen      = fault == DirectoryReadFault::InvalidRecord ? 0 : headerBytes + 2;
    ::memcpy(storage, &record, headerBytes);
    storage[headerBytes]     = 'x';
    storage[headerBytes + 1] = 'x'; // Missing NUL terminator
    return headerBytes + 2;
}

int watchedStatAt(int parent, const char* name, struct stat* value, int flags)
{
    const int result = ::fstatat(parent, name, value, flags);
    if (result == 0 and faults.replaceDuringMove and ::strcmp(name, "moved") == 0 and ++faults.movedStatCount == 2)
    {
        if (::unlinkat(parent, name, 0) != 0 or not createFile(parent, name))
            return -1;
        faults.injected = true;
    }
    return result;
}

int watchedKevent(int queue, const struct kevent* changes, int changeCount, struct kevent* events, int eventCount,
                  const struct timespec* timeout)
{
    if (faults.rejectChildRegistration and changeCount == 1 and (changes->flags & EV_ADD) != 0 and
        changes->ident != static_cast<decltype(changes->ident)>(faults.directory))
    {
        faults.openedChild = static_cast<int>(changes->ident);
        faults.injected    = true;
        errno              = ENOSPC;
        return -1;
    }
    return ::kevent(queue, changes, changeCount, events, eventCount, timeout);
}

int watchedClose(int descriptor)
{
    const int result = ::close(descriptor);
    if (result == 0 and faults.reportChildCloseError and descriptor == faults.openedChild)
    {
        errno = EIO;
        return -1;
    }
    return result;
}
} // namespace

// Compile test-local variants of these header-only internals. The normal watcher in SCTest
// continues using its native calls and a distinct set of C++ types.
#define FileSystemWatcherKqueueDirectoryReader FileSystemWatcherKqueueDirectoryReaderUnderTest
#define FileSystemWatcherKqueueInventory       FileSystemWatcherKqueueInventoryUnderTest
#define FileSystemWatcherKqueueReconcile       FileSystemWatcherKqueueReconcileUnderTest
#define getdents(...)                          watchedGetdents(__VA_ARGS__)
#define fstatat(...)                           watchedStatAt(__VA_ARGS__)
#define kevent(...)                            watchedKevent(__VA_ARGS__)
#define close(...)                             watchedClose(__VA_ARGS__)
#include "Libraries/FileSystemWatcher/Internal/FileSystemWatcherKqueueReconcile.h"
#undef close
#undef kevent
#undef fstatat
#undef getdents
#undef FileSystemWatcherKqueueReconcile
#undef FileSystemWatcherKqueueInventory
#undef FileSystemWatcherKqueueDirectoryReader

namespace SC
{
namespace
{
struct FileSystemWatcherKqueueTest : public TestCase
{
    using Inventory = FileSystemWatcherKqueueInventoryUnderTest;
    using Reader    = Inventory::Reader;
    using Reconcile = FileSystemWatcherKqueueReconcileUnderTest;

    FileSystemWatcherKqueueTest(TestReport& report) : TestCase(report, "FileSystemWatcherKqueueTest")
    {
        if (test_section("directory record validation"))
            withDirectory([this](int directory) { directoryRecords(directory); });
        if (test_section("registration rollback and reuse"))
            withDirectory([this](int directory) { registrationRollback(directory); });
        if (test_section("entry disappears during reconciliation"))
            withDirectory([this](int directory) { reconcileChange(directory, false); });
        if (test_section("entry replaced during rename verification"))
            withDirectory([this](int directory) { reconcileChange(directory, true); });
    }

    template <typename Test>
    void withDirectory(Test test)
    {
        char       path[]  = "/tmp/sc-bsd-watcher.XXXXXXXX";
        const bool created = ::mkdtemp(path) != nullptr;
        SC_TEST_EXPECT(created);
        if (not created)
            return;
        const auto cleanup = MakeDeferred(
            [&]
            {
                FileSystem fs;
                SC_TEST_EXPECT(fs.init("."));
                SC_TEST_EXPECT(fs.removeDirectoryRecursive(StringSpan::fromNullTerminated(path, StringEncoding::Utf8)));
            });
        const int directory = ::open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        SC_TEST_EXPECT(directory != -1);
        if (directory == -1)
            return;
        const auto closeDirectory = MakeDeferred([&] { SC_TEST_EXPECT(::close(directory) == 0); });
        faults                    = {};
        faults.directory          = directory;
        const auto resetFaults    = MakeDeferred([] { faults = {}; });
        test(directory);
    }

    void directoryRecords(int directory)
    {
        SC_TEST_EXPECT(createFile(directory, "entry"));
        Reader                      reader;
        alignas(struct dirent) char bytes[Reader::MinimumStorageBytes + 1];
        const Span<char>            scan{bytes + 1, sizeof(bytes) - 1};
        Reader::Entry               entry;
        bool                        hasEntry = false;

        faults.readFault = DirectoryReadFault::Interrupted;
        SC_TEST_EXPECT(static_cast<bool>(reader.reset(directory, scan)));
        SC_TEST_EXPECT(reader.next(entry, hasEntry) and faults.injected and hasEntry);
        SC_TEST_EXPECT(hasEntry and entry.nameLength == 5 and ::memcmp(entry.name, "entry", 5) == 0);

        const DirectoryReadFault malformed[] = {DirectoryReadFault::TruncatedHeader, DirectoryReadFault::InvalidRecord,
                                                DirectoryReadFault::InvalidName};
        for (const auto fault : malformed)
        {
            SC_TEST_EXPECT(static_cast<bool>(reader.reset(directory, scan)));
            faults.readFault = fault;
            SC_TEST_EXPECT(reader.next(entry, hasEntry).error == Reader::Error::MalformedEntry and not hasEntry);
        }
    }

    struct Watch
    {
        Inventory                                 inventory;
        FileSystemWatcherKqueueRegistrationTokens tokens;
        Inventory::State::Entry                   records[8];
        char                                      names[64];
        char                                      scan[Reader::MinimumStorageBytes];
        uint8_t                                   marks[8];
        char                                      path[64];

        Inventory::Result start(int queue, int directory)
        {
            return inventory.start(queue, directory, tokens, records, names, scan);
        }

        Inventory::Result reconcile(Reconcile::Change& change)
        {
            return Reconcile::run(inventory, scan, marks, path, change);
        }

        ~Watch() { (void)inventory.stop(); }
    };

    void registrationRollback(int directory)
    {
        SC_TEST_EXPECT(createFile(directory, "child"));
        const int queue = ::kqueue();
        SC_TEST_EXPECT(queue != -1);
        if (queue == -1)
            return;
        const auto closeQueue = MakeDeferred([&] { SC_TEST_EXPECT(::close(queue) == 0); });
        Watch      watch;
        faults.rejectChildRegistration = true;
        faults.reportChildCloseError   = true;

        const auto failed = watch.start(queue, directory);
        SC_TEST_EXPECT(faults.injected and faults.openedChild != -1);
        SC_TEST_EXPECT(failed.error == Inventory::Error::Registration and failed.nativeError == ENOSPC);
        SC_TEST_EXPECT(failed.cleanupError == Inventory::Error::Cleanup and failed.cleanupNativeError == EIO);
        SC_TEST_EXPECT(faults.openedChild != -1 and ::fcntl(faults.openedChild, F_GETFD) == -1 and errno == EBADF);
        SC_TEST_EXPECT(watch.inventory.paths().size() == 0 and ::fcntl(directory, F_GETFD) != -1);

        faults.rejectChildRegistration = false;
        faults.reportChildCloseError   = false;
        const auto started             = watch.start(queue, directory);
        SC_TEST_EXPECT(static_cast<bool>(started));
        if (started)
        {
            faults.readFault = DirectoryReadFault::Failed;
            Reconcile::Change change;
            const auto        stopped = watch.reconcile(change);
            SC_TEST_EXPECT(stopped.error == Inventory::Error::DirectoryRead and stopped.nativeError == EIO);
            SC_TEST_EXPECT(watch.inventory.paths().size() == 0 and not change.hasPath);
            faults.readFault = DirectoryReadFault::None;
            SC_TEST_EXPECT(static_cast<bool>(watch.start(queue, directory)));
            SC_TEST_EXPECT(static_cast<bool>(watch.inventory.stop()));
        }
    }

    void reconcileChange(int directory, bool replace)
    {
        SC_TEST_EXPECT(createFile(directory, "old"));
        const int queue = ::kqueue();
        SC_TEST_EXPECT(queue != -1);
        if (queue == -1)
            return;
        const auto closeQueue = MakeDeferred([&] { SC_TEST_EXPECT(::close(queue) == 0); });
        Watch      watch;
        const auto started = watch.start(queue, directory);
        SC_TEST_EXPECT(static_cast<bool>(started));
        if (not started)
            return;
        Reconcile::Change change;
        if (replace)
        {
            SC_TEST_EXPECT(::renameat(directory, "old", directory, "moved") == 0);
            faults.replaceDuringMove = true;
            SC_TEST_EXPECT(watch.reconcile(change) and faults.injected and change.rescanRequired and
                           not change.hasPath);
            const auto& state = watch.inventory.paths();
            const auto* child = state.get(state.findChild(watch.inventory.rootIndex(), {"moved", 5}));
            struct stat current;
            SC_TEST_EXPECT(child != nullptr and ::fstatat(directory, "moved", &current, AT_SYMLINK_NOFOLLOW) == 0 and
                           child->native.inode == static_cast<uint64_t>(current.st_ino));
        }
        else
        {
            faults.removeDuringScan = true;
            SC_TEST_EXPECT(watch.reconcile(change) and faults.injected and change.hasPath and
                           ::strcmp(watch.path, "old") == 0);
            SC_TEST_EXPECT(watch.inventory.paths().size() == 1);
        }
        SC_TEST_EXPECT(static_cast<bool>(watch.inventory.stop()));
    }
};
} // namespace

void runFileSystemWatcherKqueueTest(TestReport& report) { FileSystemWatcherKqueueTest test(report); }
} // namespace SC

#endif
