// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <Libraries/Containers/Vector.h>
#include <Libraries/Plugin/PluginMacros.h>
#include <Libraries/Strings/Console.h>
#include <Libraries/Strings/String.h>

struct TestPluginParent
{
    char consoleBuffer[256];

    SC::Console console;

    TestPluginParent() : console(consoleBuffer) {}

    [[nodiscard]] bool init()
    {
        console.printLine("TestPluginParent Start");
        return true;
    }

    [[nodiscard]] bool close()
    {
        console.printLine("TestPluginParent End");
        return true;
    }
};

// SC_BEGIN_PLUGIN
//
// Name:          Test Plugin
// Version:       1
// Description:   A Simple text plugin
// Category:      Generic
// Dependencies:
//
// SC_END_PLUGIN

SC_PLUGIN_DEFINE(TestPluginParent)
