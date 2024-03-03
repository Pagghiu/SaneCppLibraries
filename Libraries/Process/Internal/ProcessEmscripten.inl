// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Process.h"

struct SC::Process::Internal
{
};
SC::Result SC::detail::ProcessDescriptorDefinition::releaseHandle(int&) { return Result(true); }
SC::Result SC::Process::launchImplementation() { return Result(true); }
SC::Result SC::Process::waitForExitSync() { return Result(true); }
