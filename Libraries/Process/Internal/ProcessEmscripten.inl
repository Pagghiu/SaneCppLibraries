// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Process.h"
#include "EnvironmentTable.h"

struct SC::Process::Internal
{
};
SC::Result SC::detail::ProcessDescriptorDefinition::releaseHandle(int&) { return Result(true); }
SC::Result SC::Process::launchImplementation(Options options) { return Result(true); }
SC::Result SC::Process::waitForExitSync() { return Result(true); }
SC::Result SC::Process::formatArguments(Span<const StringView>) { return Result(true); }
SC::size_t SC::Process::getNumberOfProcessors() { return 1; }
bool       SC::Process::isWindowsConsoleSubsystem() { return false; }
SC::ProcessEnvironment::ProcessEnvironment() {}
SC::ProcessEnvironment::~ProcessEnvironment() {}
bool SC::ProcessEnvironment::get(size_t, StringView&, StringView&) const { return true; }
SC::ProcessFork::ProcessFork() {}
SC::ProcessFork::~ProcessFork() {}
SC::Result SC::ProcessFork::waitForChild() { return Result(true); }
SC::Result SC::ProcessFork::resumeChildFork() { return Result(true); }
SC::Result SC::ProcessFork::fork(State) { return Result(true); }
