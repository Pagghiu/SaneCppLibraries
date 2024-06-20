// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/SmallVector.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/Strings/String.h"

#include "../Interfaces/IExampleDrawing.h"
#include "imgui.h"

struct Plugin1 : public SC::IExampleDrawing
{
    Plugin1() { IExampleDrawing::onDraw.bind<Plugin1, &Plugin1::draw>(*this); }

    void draw() { ImGui::Text("Plugin 1"); }

    [[nodiscard]] bool init() { return true; }

    [[nodiscard]] bool close() { return true; }
};

// SC_BEGIN_PLUGIN
//
// Name:          Plugin 1
// Version:       1
// Description:   First plugin
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN

SC_PLUGIN_DEFINE(Plugin1)
SC_PLUGIN_EXPORT_INTERFACES(Plugin1, SC::IExampleDrawing)
