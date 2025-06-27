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

SC::Result SC::FileSystemWatcher::watch(FolderWatcher& watcher, StringViewData path)
{
    SC_TRY_MSG(watcher.parent == nullptr, "Watcher belongs to other FileSystemWatcher");
    watcher.parent = this;
    // If on windows convert path to UTF16
    // anywhere else return error on the path being UTF16
    SC_TRY_MSG(path.sizeInBytes() / sizeof(native_char_t) < StringViewData::MaxPath, "Path too long");
#if SC_PLATFORM_WINDOWS
    if (path.getEncoding() != StringEncoding::Utf16)
    {
        const int stringLen =
            ::MultiByteToWideChar(CP_UTF8, 0, path.bytesWithoutTerminator(), static_cast<int>(path.sizeInBytes()),
                                  watcher.pathBuffer, StringViewData::MaxPath / sizeof(wchar_t));
        SC_TRY_MSG(stringLen > 0, "Failed to convert path to UTF16");
        watcher.pathBuffer[stringLen] = L'\0'; // Ensure null termination
        watcher.path                  = StringViewData({watcher.pathBuffer, static_cast<size_t>(stringLen)}, true);
    }
    else
#endif
    {
        SC_TRY_MSG(path.getEncoding() != StringEncoding::Utf16, "Path cannot be UTF16 on this platform");
        ::memcpy(watcher.pathBuffer, path.bytesWithoutTerminator(), path.sizeInBytes());
        watcher.pathBuffer[path.sizeInBytes() / sizeof(native_char_t)] = '\0'; // Ensure null termination
        watcher.path = StringViewData({reinterpret_cast<const char*>(watcher.pathBuffer), path.sizeInBytes()}, true,
                                      path.getEncoding());
    }

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
