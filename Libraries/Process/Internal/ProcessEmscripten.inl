// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Process.h"

struct SC::Process::Internal
{
};
SC::Result SC::ProcessDescriptorDefinition::releaseHandle(int&) { return Result(true); }
SC::Result SC::Process::launch(Options options) { return Result(true); }
SC::Result SC::Process::waitForExitSync() { return Result(true); }
