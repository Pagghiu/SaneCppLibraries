// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description: Compiles the pinned external Taskflow Skynet backend with third-party-only warning policy.
//---------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

#include "taskflow.cpp"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
