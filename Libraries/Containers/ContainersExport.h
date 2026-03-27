// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Compiler.h"

#ifndef SC_EXPORT_LIBRARY_CONTAINERS
#define SC_EXPORT_LIBRARY_CONTAINERS 0
#endif
#define SC_CONTAINERS_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_CONTAINERS)
