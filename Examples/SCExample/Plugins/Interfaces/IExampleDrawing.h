// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Foundation/Function.h"
#include "Libraries/Strings/StringHashFNV.h"

namespace SC
{
struct IExampleDrawing
{
    static constexpr unsigned int InterfaceHash = SC::StringHashFNV("IExampleDrawing");

    Function<void(void)> onDraw;
};

} // namespace SC
