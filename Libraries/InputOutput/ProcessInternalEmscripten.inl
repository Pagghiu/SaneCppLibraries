// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Process.h"

SC::ReturnCode SC::ProcessEntry::run(const ProcessOptions& options) { return true; }
SC::ReturnCode SC::ProcessEntry::waitProcessExit() { return true; }
SC::ReturnCode SC::ProcessNativeHandleClosePosix(const ProcessNativeHandle& handle) { return true; }
