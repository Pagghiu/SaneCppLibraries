// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include <SC/Libraries/Containers/SmallVector.h>
#include <SC/Libraries/Plugin/PluginMacros.h>
#include <SC/Libraries/Strings/Console.h>
#include <SC/Libraries/Strings/String.h>
SC::StringView externallyDefinedFunc();

struct TestPluginChild
{
    SC::SmallVector<char, 1024 * sizeof(SC::native_char_t)> consoleBuffer;

    SC::Console console;

    TestPluginChild() : console(consoleBuffer) { console.printLine("TestPluginChild original Start"); }

    ~TestPluginChild() { console.printLine("TestPluginChild original End"); }

    [[nodiscard]] bool init()
    {
        using namespace SC;
        StringView sv = "123";
        int32_t    value;

        // Let's test using something that must be linked from the caller
        return sv.parseInt32(value) and value == 123 and externallyDefinedFunc() == "Yeah";
    }

    [[nodiscard]] bool close() { return true; }
};

extern "C" SC_PLUGIN_EXPORT bool isPluginOriginal() { return true; }

// SC_BEGIN_PLUGIN
//
// Name:          Test Plugin
// Version:       1
// Description:   A Simple text plugin
// Category:      Generic
// Dependencies:  TestPluginParent
//
// SC_END_PLUGIN
SC_DEFINE_PLUGIN(TestPluginChild)
