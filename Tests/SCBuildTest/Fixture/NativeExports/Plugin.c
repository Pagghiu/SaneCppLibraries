// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
int                                        sc_native_exported(void);
__attribute__((visibility("default"))) int sc_native_plugin_value(void) { return sc_native_exported(); }
