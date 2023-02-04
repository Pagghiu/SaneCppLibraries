// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "OS.h"

SC::OSPaths globalPaths;

bool SC::OSPaths::close()
{
    globalPaths = OSPaths();
    return true;
}

const SC::OSPaths& SC::OSPaths::get()
{
    // Probably forgot to call init
    SC_RELEASE_ASSERT(!globalPaths.executableFile.view().isEmpty());
    return globalPaths;
}

bool SC::printBacktrace() { return OS::printBacktrace(); }
