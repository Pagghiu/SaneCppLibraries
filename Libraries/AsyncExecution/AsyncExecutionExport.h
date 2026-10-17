// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/CompilerMacrosExport.h"

#ifndef SC_EXPORT_LIBRARY_ASYNCEXECUTION
#define SC_EXPORT_LIBRARY_ASYNCEXECUTION 0
#endif
#define SC_ASYNC_EXECUTION_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_ASYNCEXECUTION)
