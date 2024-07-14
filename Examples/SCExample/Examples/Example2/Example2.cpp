// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/SmallVector.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/Strings/String.h"

#include "../ISCExample.h"
#include "imgui.h"

struct Example2 : public SC::ISCExample
{
    Example2() { ISCExample::onDraw.bind<Example2, &Example2::draw>(*this); }

    void draw() { ImGui::Text("Example 2"); }

    [[nodiscard]] bool init() { return true; }

    [[nodiscard]] bool close() { return true; }
};

// SC_BEGIN_PLUGIN
//
// Name:          Example 2
// Version:       1
// Description:   Second example
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN

SC_PLUGIN_DEFINE(Example2)
SC_PLUGIN_EXPORT_INTERFACES(Example2, SC::ISCExample)
