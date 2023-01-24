// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "OS.h"

#if SC_GCC
#error "Please fix GCC build script by allowing building objective C++ files"
#else
#import <Foundation/Foundation.h>
#endif
#include <mach-o/dyld.h>

extern SC::OSPaths globalPaths;

bool SC::OSPaths::init()
{
    uint32_t executable_length = 0;
    _NSGetExecutablePath(NULL, &executable_length);
    if (executable_length > 1)
    {
        SC_TRY_IF(globalPaths.executableFile.data.resizeWithoutInitializing(executable_length));
        // Writes also the null terminator, but assert just in case
        _NSGetExecutablePath(globalPaths.executableFile.data.data(), &executable_length);
        SC_TRY_IF(globalPaths.executableFile.data[executable_length - 1] == 0);
    }
    NSURL*      appFolder                = [[NSBundle mainBundle] bundleURL];
    const char* theString                = [appFolder.path UTF8String];
    globalPaths.applicationRootDirectory = StringView(theString, strlen(theString), true, StringEncoding::Utf8);
    return true;
}
