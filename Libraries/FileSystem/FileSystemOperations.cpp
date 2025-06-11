// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef _WIN32
#include "Internal/FileSystemOperationsWindows.inl"
#else
#include "Internal/FileSystemOperationsPosix.inl"
#endif
