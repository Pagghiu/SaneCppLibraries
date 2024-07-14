// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Foundation/Function.h"
#include "Libraries/Strings/StringHashFNV.h"

namespace SC
{
struct ISCExample
{
    static constexpr unsigned int InterfaceHash = SC::StringHashFNV("ISCExample");

    Function<void(void)> onDraw;
};

} // namespace SC
