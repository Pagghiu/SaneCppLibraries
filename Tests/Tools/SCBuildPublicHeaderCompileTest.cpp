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
        auto supportMatrix = SC::Build::getNativeBackendSupportMatrix();
        (void)buildConfigurePointer;
        (void)runBuildToolPointer;
        (void)supportMatrix;
    }
} consumePublicBuildHeaderSymbols;
} // namespace
