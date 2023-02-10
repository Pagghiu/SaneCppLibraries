// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#if __APPLE__
#include <mach-o/dyld.h>
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

#include "Console.h"
#include "Limits.h"

#include <execinfo.h> // backtrace
#include <memory.h>   // memcpy
#include <stdlib.h>   // free

bool SC::OSPaths::init()
{
#if __APPLE__

    uint32_t executable_length = 0;
    _NSGetExecutablePath(NULL, &executable_length);
    executableFile = ""_u8;
    if (executable_length > 1)
    {
        SC_TRY_IF(executableFile.data.resizeWithoutInitializing(executable_length));
        // Writes also the null terminator, but assert just in case
        _NSGetExecutablePath(executableFile.data.data(), &executable_length);
        SC_TRY_IF(executableFile.data[executable_length - 1] == 0);
    }

    // NSURL*      appFolder = [[NSBundle mainBundle] bundleURL];
    // const char* theString = [appFolder.path UTF8String];

    id pool = ((id(*)(id, SEL))objc_msgSend)((id)objc_lookUpClass("NSAutoreleasePool"), sel_getUid("alloc"));
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_getUid("init"));

    id  nsBundleClass = (id)objc_lookUpClass("NSBundle");
    SEL mainBundleSel = sel_registerName("mainBundle");
    SEL bundleURLSel  = sel_registerName("bundleURL");
    SEL pathSel       = sel_registerName("path");
    SEL UTF8StringSel = sel_registerName("UTF8String");

    id mainBundle = ((id(*)(id, SEL))objc_msgSend)(nsBundleClass, mainBundleSel);
    id appFolder  = ((id(*)(id, SEL))objc_msgSend)(mainBundle, bundleURLSel);
    id path       = ((id(*)(id, SEL))objc_msgSend)(appFolder, pathSel);

    const char* theString = ((const char* (*)(id, SEL))objc_msgSend)(path, UTF8StringSel);

    StringView bundlePath(theString, strlen(theString), true, StringEncoding::Utf8);
    applicationRootDirectory = bundlePath;
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_getUid("release"));
    return true;
#else
    return false;
#endif
}
bool SC::OS::printBacktrace()
{
    void* backtraceBuffer[100];
    return printBacktrace(backtraceBuffer, sizeof(backtraceBuffer));
}

bool SC::OS::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    const size_t numFrames = captureBacktrace(2, backtraceBuffer, backtraceBufferSizeInBytes, nullptr);
    if (numFrames == 0)
    {
        return false;
    }
    char** strs = backtrace_symbols(backtraceBuffer, static_cast<int>(numFrames));
    for (size_t i = 0; i < numFrames; ++i)
    {
        StringView line(strs[i], strlen(strs[i]), true, StringEncoding::Ascii);
        Console::printNullTerminatedASCII(line);
        Console::printNullTerminatedASCII("\n"_a8);
    }
    // TODO: Fix Backtrace line numbers
    // https://stackoverflow.com/questions/8278691/how-to-fix-backtrace-line-number-error-in-c
    free(strs);
    return true;
}

SC::size_t SC::OS::captureBacktrace(size_t framesToSkip, void** backtraceBuffer, size_t backtraceBufferSizeInBytes,
                                    uint32_t* hash)
{
    const size_t   framesToCapture = backtraceBufferSizeInBytes / sizeof(void*);
    constexpr auto maxVal          = static_cast<size_t>(static_cast<int>(MaxValue()));
    if (framesToCapture > maxVal || (backtraceBuffer == nullptr))
    {
        return 0;
    }
    // This signature maps 1 to 1 with windows CaptureStackBackTrace, at some
    // point we will allow framesToSkip > 0 and compute has
    int numFrames = backtrace(backtraceBuffer, static_cast<int>(framesToCapture));
    numFrames -= framesToSkip;
    if (framesToSkip > 0)
    {
        for (int i = 0; i < numFrames; ++i)
        {
            backtraceBuffer[i] = backtraceBuffer[i + framesToSkip];
        }
    }
    if (hash)
    {
        uint32_t computedHash = 0;
        // TODO: Compute a proper hash
        for (int i = 0; i < numFrames; ++i)
        {
            uint32_t value;
            memcpy(&value, backtraceBuffer[i], sizeof(uint32_t));
            computedHash ^= value;
        }
        *hash = computedHash;
    }
    return numFrames;
}
