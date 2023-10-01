// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Foundation/Containers/SmallVector.h"
#include "System.h"

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

bool SC::SystemDirectories::init()
{
#if SC_XCTEST
    Dl_info dlinfo;
    SC_TRY(dladdr((void*)Memory::allocate, &dlinfo));
    const StringView path(dlinfo.dli_fname, ::strlen(dlinfo.dli_fname), true, StringEncoding::Utf8);
    SC_TRY(executableFile.assign(path));
    SC_TRY(applicationRootDirectory.assign(Path::dirname(executableFile.view(), 3)));
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
#endif

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
                StringView bundlePath(urlToFs, strlen(urlToFs), true, StringEncoding::Utf8);
                CFRelease(bundleURL);
                return applicationRootDirectory.assign(bundlePath);
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

    const char* theString = ((const char* (*)(id, SEL))objc_msgSend)(path, UTF8StringSel);
    StringView  bundlePath(theString, strlen(theString), true, StringEncoding::Utf8);
    applicationRootDirectory = bundlePath;
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_getUid("release"));
    return true;
#endif
}
