// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Interfaces.h"
#include <Libraries/Containers/SmallVector.h>
#include <Libraries/Plugin/PluginMacros.h>
#include <Libraries/Strings/Console.h>
#include <Libraries/Strings/String.h>
SC::StringView externallyDefinedFunc();

struct TestPluginChild : public ITestInterface1, public ITestInterface2
{
    SC::SmallBuffer<1024 * sizeof(SC::native_char_t)> consoleBuffer;

    SC::Console console;

    TestPluginChild() : console(consoleBuffer)
    {
        // Setup Interfaces table
        ITestInterface1::multiplyInt.bind<TestPluginChild, &TestPluginChild::multiply>(*this);
        ITestInterface2::divideFloat.bind<TestPluginChild, &TestPluginChild::divide>(*this);

        console.printLine("TestPluginChild original Start");
    }

    ~TestPluginChild() { console.printLine("TestPluginChild original End"); }

    int multiply(int value) { return value * 2; }

    float divide(float value) { return value / 2; }

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
SC_PLUGIN_DEFINE(TestPluginChild)
SC_PLUGIN_EXPORT_INTERFACES(TestPluginChild, ITestInterface1, ITestInterface2)
