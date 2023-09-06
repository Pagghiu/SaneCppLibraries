// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include <SC/Libraries/Foundation/SmallVector.h>
#include <SC/Libraries/Foundation/String.h>
#include <SC/Libraries/Plugin/PluginMacros.h>
#include <SC/Libraries/System/Console.h>

struct TestPluginParent
{
    SC::SmallVector<char, 1024 * sizeof(SC::utf_char_t)> consoleBuffer;

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

SC_DEFINE_PLUGIN(TestPluginParent)
