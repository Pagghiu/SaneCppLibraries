// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Process.h"
#include "EnvironmentTable.h"

struct SC::Process::Internal
{
};
SC::Result SC::detail::ProcessDescriptorDefinition::releaseHandle(int&) { return Result(true); }
SC::Result SC::Process::launchImplementation() { return Result(true); }
SC::Result SC::Process::waitForExitSync() { return Result(true); }
SC::Result SC::Process::formatArguments(Span<const StringView>) { return Result(true); }
SC::ProcessEnvironment::ProcessEnvironment() {}
SC::ProcessEnvironment::~ProcessEnvironment() {}
bool SC::ProcessEnvironment::get(size_t, StringView&, StringView&) const { return true; }
