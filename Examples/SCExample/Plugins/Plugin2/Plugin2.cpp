// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/SmallVector.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/Strings/String.h"

#include "../Interfaces/IExampleDrawing.h"
#include "imgui.h"

struct Plugin2 : public SC::IExampleDrawing
{
    Plugin2() { IExampleDrawing::onDraw.bind<Plugin2, &Plugin2::draw>(*this); }

    void draw() { ImGui::Text("Plugin 2"); }

    [[nodiscard]] bool init() { return true; }

    [[nodiscard]] bool close() { return true; }
};

// SC_BEGIN_PLUGIN
//
// Name:          Plugin 2
// Version:       1
// Description:   Second plugin
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN

SC_PLUGIN_DEFINE(Plugin2)
SC_PLUGIN_EXPORT_INTERFACES(Plugin2, SC::IExampleDrawing)
