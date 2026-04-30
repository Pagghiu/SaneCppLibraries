// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SaneCppBuild.h"

namespace
{
auto* buildConfigurePointer = &SC::Build::configure;
auto* runBuildToolPointer   = &SC::Tools::runBuildTool;

struct ConsumePublicBuildHeaderSymbols
{
    ConsumePublicBuildHeaderSymbols()
    {
        (void)buildConfigurePointer;
        (void)runBuildToolPointer;
    }
} consumePublicBuildHeaderSymbols;
} // namespace
