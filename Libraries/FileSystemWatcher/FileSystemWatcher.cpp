// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystemWatcher/FileSystemWatcher.h"
#include "../Foundation/Assert.h"
#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemWatcherWindows.inl"
#elif SC_PLATFORM_APPLE
#include "Internal/FileSystemWatcherApple.inl"
#else
#include "Internal/FileSystemWatcherLinux.inl"
#endif

SC::FileSystemWatcher::FolderWatcher::FolderWatcher(Span<char> buffer)
{
#if SC_PLATFORM_LINUX
    subFolderRelativePathsBuffer = buffer;
#else
    (void)buffer;
#endif
}

SC::Result SC::FileSystemWatcher::init(EventLoopRunner& runner) { return internal.get().init(*this, runner); }

SC::Result SC::FileSystemWatcher::init(ThreadRunner& runner) { return internal.get().init(*this, runner); }

SC::Result SC::FileSystemWatcher::close() { return internal.get().close(); }

SC::Result SC::FileSystemWatcher::watch(FolderWatcher& watcher, StringSpan path)
{
    SC_TRY_MSG(watcher.parent == nullptr, "Watcher belongs to other FileSystemWatcher");
    watcher.parent = this;
    SC_TRY_MSG(watcher.path.assign(path), "FileSystemWatcher::watch - Error assigning path");
    watchers.queueBack(watcher);
    return internal.get().startWatching(&watcher);
}

SC::Result SC::FileSystemWatcher::FolderWatcher::stopWatching()
{
    SC_TRY_MSG(parent != nullptr, "FolderWatcher already unwatched");
    return parent->internal.get().stopWatching(*this);
}

void SC::FileSystemWatcher::FolderWatcher::setDebugName(const char* debugName) { (void)debugName; }

//! [OpaqueDefinition2Snippet]
template <>
void SC::FileSystemWatcher::InternalOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::FileSystemWatcher::InternalOpaque::destruct(Object& obj)
{
    obj.~Object();
}
//! [OpaqueDefinition2Snippet]
template <>
void SC::FileSystemWatcher::ThreadRunner::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::FileSystemWatcher::ThreadRunner::destruct(Object& obj)
{
    obj.~Object();
}

template <>
void SC::OpaqueObject<SC::FileSystemWatcher::FolderWatcherSizes>::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::OpaqueObject<SC::FileSystemWatcher::FolderWatcherSizes>::destruct(Object& obj)
{
    obj.~Object();
}

void SC::FileSystemWatcher::EventLoopRunner::internalInit(FileSystemWatcher& pself, int handle)
{
    fileSystemWatcher = &pself;
    (void)handle;
#if SC_PLATFORM_LINUX
    notifyFd = handle;
#endif
}

void SC::FileSystemWatcher::WatcherLinkedList::queueBack(FolderWatcher& item)
{
    SC_ASSERT_DEBUG(item.next == nullptr and item.prev == nullptr);
    if (back)
    {
        back->next = &item;
        item.prev  = back;
    }
    else
    {
        SC_ASSERT_DEBUG(front == nullptr);
        front = &item;
    }
    back = &item;
    SC_ASSERT_DEBUG(back->next == nullptr);
    SC_ASSERT_DEBUG(front->prev == nullptr);
}

void SC::FileSystemWatcher::WatcherLinkedList::remove(FolderWatcher& item)
{
    using T = FolderWatcher;
#if SC_CONFIGURATION_DEBUG
    bool found = false;
    auto it    = front;
    while (it)
    {
        if (it == &item)
        {
            found = true;
            break;
        }
        it = static_cast<T*>(it->next);
    }
    SC_ASSERT_DEBUG(found);
#endif
    if (&item == front)
    {
        front = static_cast<T*>(front->next);
    }
    if (&item == back)
    {
        back = static_cast<T*>(back->prev);
    }

    T* next = static_cast<T*>(item.next);
    T* prev = static_cast<T*>(item.prev);

    if (prev)
    {
        prev->next = next;
    }

    if (next)
    {
        next->prev = prev;
    }

    item.next = nullptr;
    item.prev = nullptr;
}
