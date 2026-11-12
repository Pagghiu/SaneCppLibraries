// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Common/Function.h"
#include "Libraries/Plugin/PluginHash.h"

struct ITestInterface1
{
    static constexpr auto InterfaceHash = SC::PluginHash("ITestInterface1");

    SC::Function<int(int)> multiplyInt;
};
struct ITestInterface2
{
    static constexpr auto InterfaceHash = SC::PluginHash("ITestInterface2");

    struct Coordinates
    {
        double x;
        double y;
        double z;
    };

    SC::Function<float(float)>             divideFloat;
    SC::Function<Coordinates(Coordinates)> translate;
};
