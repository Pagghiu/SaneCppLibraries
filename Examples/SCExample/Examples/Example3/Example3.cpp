// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/SmallVector.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/Strings/String.h"

#include "../ISCExample.h"
#include "imgui.h"

struct Example3 : public SC::ISCExample
{
    Example3() { ISCExample::onDraw.bind<Example3, &Example3::draw>(*this); }

    void draw() { ImGui::Text("Example 3"); }

    [[nodiscard]] bool init() { return true; }

    [[nodiscard]] bool close() { return true; }
};

// SC_BEGIN_PLUGIN
//
// Name:          Example 3
// Version:       1
// Description:   Third example
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN

SC_PLUGIN_DEFINE(Example3)
SC_PLUGIN_EXPORT_INTERFACES(Example3, SC::ISCExample)
