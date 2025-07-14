// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Internal/IntrusiveDoubleLinkedList.inl" // IWYU pragma: keep

#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemWatcherWindows.inl"
#elif SC_PLATFORM_APPLE
#include "Internal/FileSystemWatcherApple.inl"
#else
#include "Internal/FileSystemWatcherLinux.inl"
#endif

SC::Result SC::FileSystemWatcher::init(EventLoopRunner& runner, AsyncEventLoop& eventLoop)
{
    runner.eventLoop = &eventLoop;
    return internal.get().init(*this, runner);
}

SC::Result SC::FileSystemWatcher::init(ThreadRunner& runner) { return internal.get().init(*this, runner); }

SC::Result SC::FileSystemWatcher::close() { return internal.get().close(); }

SC::Result SC::FileSystemWatcher::watch(FolderWatcher& watcher, StringSpan path)
{
    SC_TRY_MSG(watcher.parent == nullptr, "Watcher belongs to other FileSystemWatcher");
    watcher.parent = this;
    SC_TRY_MSG(watcher.path.path.assign(path), "FileSystemWatcher::watch - Error assigning path");
    watchers.queueBack(watcher);
    return internal.get().startWatching(&watcher);
}

SC::Result SC::FileSystemWatcher::FolderWatcher::stopWatching()
{
    SC_TRY_MSG(parent != nullptr, "FolderWatcher already unwatched");
    return parent->internal.get().stopWatching(*this);
}

void SC::FileSystemWatcher::FolderWatcher::setDebugName(const char* debugName)
{
    (void)debugName;
#if SC_PLATFORM_WINDOWS
    return Internal::setDebugName(*this, debugName);
#endif
}

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
