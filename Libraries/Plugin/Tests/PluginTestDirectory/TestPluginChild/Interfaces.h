// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Foundation/Function.h"
#include "Libraries/Strings/StringHashFNV.h"

struct ITestInterface1
{
    static constexpr const char*  InterfaceName = "ITestInterface1";
    static constexpr SC::uint32_t InterfaceHash = SC::StringHashFNV("ITestInterface1");

    SC::Function<int(int)> multiplyInt;
};
struct ITestInterface2
{
    static constexpr const char*  InterfaceName = "ITestInterface2";
    static constexpr SC::uint32_t InterfaceHash = SC::StringHashFNV("ITestInterface2");

    SC::Function<float(float)> divideFloat;
};
