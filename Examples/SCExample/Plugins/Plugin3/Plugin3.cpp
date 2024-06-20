// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/SmallVector.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/Strings/String.h"

#include "../Interfaces/IExampleDrawing.h"
#include "imgui.h"

struct Plugin3 : public SC::IExampleDrawing
{
    Plugin3() { IExampleDrawing::onDraw.bind<Plugin3, &Plugin3::draw>(*this); }

    void draw() { ImGui::Text("Plugin 3"); }

    [[nodiscard]] bool init() { return true; }

    [[nodiscard]] bool close() { return true; }
};

// SC_BEGIN_PLUGIN
//
// Name:          Plugin 3
// Version:       1
// Description:   Third plugin
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN

SC_PLUGIN_DEFINE(Plugin3)
SC_PLUGIN_EXPORT_INTERFACES(Plugin3, SC::IExampleDrawing)
