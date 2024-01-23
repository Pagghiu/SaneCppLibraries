// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "FileSystemWatcher.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemWatcherWindows.inl"
#elif SC_PLATFORM_APPLE
#include "Internal/FileSystemWatcherApple.inl"
#elif SC_PLATFORM_LINUX
#include "Internal/FileSystemWatcherLinux.inl"
#else
#include "Internal/FileSystemWatcherEmscripten.inl"
#endif

SC::Result SC::FileSystemWatcher::init(EventLoopRunner& runner) { return internal.get().init(*this, runner); }

SC::Result SC::FileSystemWatcher::init(ThreadRunner& runner) { return internal.get().init(*this, runner); }

SC::Result SC::FileSystemWatcher::close() { return internal.get().close(); }
SC::Result SC::FileSystemWatcher::watch(FolderWatcher& watcher, StringView path,
                                        Function<void(const Notification&)>&& notifyCallback)
{
    SC_TRY_MSG(watcher.parent == nullptr, "Watcher belongs to other FileSystemWatcher");
    watcher.parent = this;
    SC_TRY(watcher.path.assign(path));
    watcher.notifyCallback = move(notifyCallback);
    watchers.queueBack(watcher);
    return internal.get().startWatching(&watcher);
}

SC::Result SC::FileSystemWatcher::FolderWatcher::stopWatching()
{
    SC_TRY_MSG(parent != nullptr, "FolderWatcher already unwatched");
    return parent->internal.get().stopWatching(*this);
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
