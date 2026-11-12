// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <Libraries/Plugin/PluginMacros.h>

struct TestPluginLeft
{
    bool init() { return true; }
    bool close() { return true; }
};

// SC_BEGIN_PLUGIN
// Name:          Test Plugin Left
// Version:       1
// Description:   First dependency of the test plugin
// Category:      Generic
// Dependencies:  TestPluginParent
// SC_END_PLUGIN
SC_PLUGIN_DEFINE(TestPluginLeft)
