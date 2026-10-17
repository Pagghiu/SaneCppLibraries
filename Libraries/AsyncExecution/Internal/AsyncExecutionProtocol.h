// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/CompilerMacrosStdCpp.h"

// stdexec is an opt-in interoperability test backend, never a required SC dependency.
#ifndef SC_ASYNC_EXECUTION_USE_STDEXEC
#define SC_ASYNC_EXECUTION_USE_STDEXEC 0
#endif

#if SC_INCLUDE_STD_CPP
#if SC_ASYNC_EXECUTION_USE_STDEXEC
#include <stdexec/execution.hpp>
#define SC_ASYNC_EXECUTION_AVAILABLE 1
namespace SC
{
namespace AsyncExecutionProtocol = stdexec;
}
#else
#include <execution>
#if defined(__cpp_lib_senders) && __cpp_lib_senders >= 202506L
#define SC_ASYNC_EXECUTION_AVAILABLE 1
namespace SC
{
namespace AsyncExecutionProtocol = std::execution;
}
#endif
#endif
#endif

#ifndef SC_ASYNC_EXECUTION_AVAILABLE
#define SC_ASYNC_EXECUTION_AVAILABLE 0
#endif
