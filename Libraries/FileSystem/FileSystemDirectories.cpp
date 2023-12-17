// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemDirectories.h"
#include "../Containers/SmallVector.h"
#include "../Foundation/Result.h"

#if SC_PLATFORM_WINDOWS
#include "../FileSystem/Path.h"
#include "../Strings/StringBuilder.h"

#include <Windows.h>

bool SC::FileSystemDirectories::init()
{
    // TODO: OsPaths::init() for Windows is messy. Tune the API to improve writing software like this.
    // Reason is because it's handy counting in wchars but we can't do it with StringNative.
    // Additionally we must convert to utf8 at the end otherwise path::dirname will not work
    SmallVector<wchar_t, MAX_PATH> buffer;

    size_t numChars;
    int    tries = 0;
    do
    {
        SC_TRY(buffer.resizeWithoutInitializing(buffer.size() + MAX_PATH));
        // Is returned null terminated
        numChars = GetModuleFileNameW(0L, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (tries++ >= 10)
        {
            return false;
        }
    } while (numChars == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    SC_TRY(buffer.resizeWithoutInitializing(numChars + 1));
    SC_TRY(buffer[numChars] == 0);

    StringView utf16executable = StringView(Span<const wchar_t>(buffer.data(), (buffer.size() - 1)), true);

    // TODO: FileSystemDirectories::init - We must also convert to utf8 because dirname will not work on non utf8 or
    // ascii text assigning directly the SmallString inside StringNative will copy as is instad of converting utf16 to
    // utf8
    executableFile = ""_u8;
    StringBuilder builder(executableFile);
    SC_TRY(builder.append(utf16executable));
    applicationRootDirectory = Path::dirname(executableFile.view(), Path::AsWindows);
    return Result(true);
}
#elif SC_PLATFORM_APPLE

#include <mach-o/dyld.h>
#if 1
#include <CoreFoundation/CoreFoundation.h>
#else
#if __clang__
#include <objc/objc-runtime.h>
#include <objc/objc.h>
#else
// XCode SDK obj-c runtime headers use obj-c syntax and GCC refuses it in C++ mode
#include <objc/objc.h>
OBJC_EXPORT void objc_msgSend(void /* id self, SEL op, ... */) OBJC_AVAILABLE(10.0, 2.0, 9.0, 1.0, 2.0);
OBJC_EXPORT      Class _Nullable objc_lookUpClass(const char* _Nonnull name) OBJC_AVAILABLE(10.0, 2.0, 9.0, 1.0, 2.0);
#endif
#endif
#if SC_XCTEST
#include "../FileSystem/Path.h"
#include <dlfcn.h>
#endif

bool SC::FileSystemDirectories::init()
{
#if SC_XCTEST
    Dl_info dlinfo;
    SC_TRY(dladdr((void*)Memory::allocate, &dlinfo));
    const StringView path = StringView::fromNullTerminated(dlinfo.dli_fname, StringEncoding::Utf8);
    SC_TRY(executableFile.assign(path));
    SC_TRY(applicationRootDirectory.assign(Path::dirname(executableFile.view(), Path::Type::AsPosix, 3)));
    return true;
#else
    SmallVector<char, StaticPathSize> data;
    uint32_t                          executable_length = 0;
    _NSGetExecutablePath(NULL, &executable_length);
    executableFile = String(StringEncoding::Utf8);
    if (executable_length > 1)
    {
        SC_TRY(data.resizeWithoutInitializing(executable_length));
        // Writes also the null terminator, but assert just in case
        _NSGetExecutablePath(data.data(), &executable_length);
        SC_TRY(data[executable_length - 1] == 0);
        executableFile = SmallString<StaticPathSize>(move(data), StringEncoding::Utf8);
    }

#if 1
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle != nullptr)
    {
        CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle);
        if (bundleURL != nullptr)
        {
            constexpr int MaxPathLength = 2048;
            char          urlToFs[MaxPathLength];
            if (CFURLGetFileSystemRepresentation(bundleURL, true, reinterpret_cast<uint8_t*>(urlToFs), MaxPathLength))
            {
                CFRelease(bundleURL);
                return applicationRootDirectory.assign(StringView::fromNullTerminated(urlToFs, StringEncoding::Utf8));
            }
            CFRelease(bundleURL);
        }
    }
    return false;
#else
    // NSURL*      appFolder = [[NSBundle mainBundle] bundleURL];
    // const char* theString = [appFolder.path UTF8String];
    id pool = ((id(*)(id, SEL))objc_msgSend)((id)objc_lookUpClass("NSAutoreleasePool"), sel_getUid("alloc"));
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_getUid("init"));

    id  nsBundleClass = (id)objc_getClass("NSBundle");
    SEL mainBundleSel = sel_registerName("mainBundle");
    SEL bundleURLSel  = sel_registerName("bundleURL");
    SEL pathSel       = sel_registerName("path");
    SEL UTF8StringSel = sel_registerName("UTF8String");

    id mainBundle = ((id(*)(id, SEL))objc_msgSend)(nsBundleClass, mainBundleSel);
    id appFolder  = ((id(*)(id, SEL))objc_msgSend)(mainBundle, bundleURLSel);
    id path       = ((id(*)(id, SEL))objc_msgSend)(appFolder, pathSel);

    const char* theString    = ((const char* (*)(id, SEL))objc_msgSend)(path, UTF8StringSel);
    applicationRootDirectory = StringView::fromNullTerminated(theString, StringEncoding::Utf8);
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_getUid("release"));
    return true;
#endif
#endif
}

#else
bool SC::FileSystemDirectories::init() { return true; }

#endif
