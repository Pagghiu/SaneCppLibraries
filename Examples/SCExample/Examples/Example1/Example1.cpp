// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/SmallVector.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/Strings/String.h"

#include "../ISCExample.h"
#include "imgui.h"

struct Example1 : public SC::ISCExample
{
    Example1() { ISCExample::onDraw.bind<Example1, &Example1::draw>(*this); }

    void draw() { ImGui::Text("Example 1"); }

    [[nodiscard]] bool init() { return true; }

    [[nodiscard]] bool close() { return true; }
};

// SC_BEGIN_PLUGIN
//
// Name:          Example 1
// Version:       1
// Description:   First example
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN

SC_PLUGIN_DEFINE(Example1)
SC_PLUGIN_EXPORT_INTERFACES(Example1, SC::ISCExample)
