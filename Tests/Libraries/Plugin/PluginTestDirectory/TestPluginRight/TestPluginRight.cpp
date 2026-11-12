// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <Libraries/Plugin/PluginMacros.h>

struct TestPluginRight
{
    bool init() { return true; }
    bool close() { return true; }
};

// SC_BEGIN_PLUGIN
// Name:          Test Plugin Right
// Version:       1
// Description:   Second dependency of the test plugin
// Category:      Generic
// Dependencies:  TestPluginParent
// SC_END_PLUGIN
SC_PLUGIN_DEFINE(TestPluginRight)
