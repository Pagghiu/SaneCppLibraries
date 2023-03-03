// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Process.h"

#include "../Foundation/MovableHandle.h"

struct SC::ProcessEntry::Internal
{
    static ReturnCode ProcessHandleClose(int& handle) { return true; }
};
struct SC::ProcessEntry::ProcessHandle : public MovableHandle<int, 0, ReturnCode, &Internal::ProcessHandleClose>
{
};
SC::ReturnCode SC::ProcessEntry::run(const ProcessOptions& options) { return true; }
SC::ReturnCode SC::ProcessEntry::waitProcessExit() { return true; }
