// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "SC-build/Build.h"
#include "Tools.h"

namespace SC
{
namespace Build
{
Result configure(Definition& definition, const Parameters& parameters);
}
} // namespace SC

#if !defined(SC_TOOLS_COMPILED_SEPARATELY) && !defined(SC_TOOLS_IMPORT) && !defined(SC_BUILD_SOURCE)
#include "../Libraries/Process/Process.h"
#include "SC-build/Build.inl"
#include "SC-build/BuildCLI.h"
#define SC_TOOLS_IMPORT
#include "SC-package.cpp"
#undef SC_TOOLS_IMPORT
#include "SC-build/BuildCLI.inl"
#endif
