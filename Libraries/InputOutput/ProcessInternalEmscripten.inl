// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Process.h"

struct SC::Process::Internal
{
};
SC::ReturnCode SC::ProcessNativeHandleClose(int& handle) { return true; }
SC::ReturnCode SC::Process::run(const ProcessOptions& options) { return true; }
SC::ReturnCode SC::Process::waitProcessExit() { return true; }
