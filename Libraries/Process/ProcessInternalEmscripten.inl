// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Process.h"

struct SC::Process::Internal
{
};
SC::ReturnCode SC::ProcessDescriptorTraits::releaseHandle(int&) { return ReturnCode(true); }
SC::ReturnCode SC::Process::launch(ProcessOptions options) { return ReturnCode(true); }
SC::ReturnCode SC::Process::waitForExitSync() { return ReturnCode(true); }
