// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemWatcher.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemWatcherWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/FileSystemWatcherEmscripten.inl"
#elif SC_PLATFORM_APPLE
#include "Internal/FileSystemWatcherApple.inl"
#endif

SC::Result SC::FileSystemWatcher::init(EventLoopRunner& runner) { return internal.get().init(*this, runner); }

SC::Result SC::FileSystemWatcher::init(ThreadRunner& runner) { return internal.get().init(*this, runner); }

SC::Result SC::FileSystemWatcher::close() { return internal.get().close(); }
SC::Result SC::FileSystemWatcher::watch(FolderWatcher& watcher, String& path,
                                        Function<void(const Notification&)>&& notifyCallback)
{
    SC_TRY_MSG(watcher.parent == nullptr, "Watcher belongs to other FileSystemWatcher");
    watcher.parent         = this;
    watcher.path           = &path;
    watcher.notifyCallback = move(notifyCallback);
    watchers.queueBack(watcher);
    return internal.get().startWatching(&watcher);
}

SC::Result SC::FileSystemWatcher::FolderWatcher::unwatch()
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
