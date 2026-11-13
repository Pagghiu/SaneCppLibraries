// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <dlfcn.h>
#include <iso646.h>
#include <stdio.h>

int sc_native_hidden(void);

typedef int (*PluginValue)(void);

int main(int argc, char** argv)
{
    if (argc != 2 or sc_native_hidden() != 11)
        return 1;
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (library == NULL)
    {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 2;
    }
    PluginValue value = (PluginValue)dlsym(library, "sc_native_plugin_value");
    const int valid   = value != NULL and value() == 42 and dlsym(RTLD_DEFAULT, "sc_native_hidden") == NULL;
    if (dlclose(library) != 0 or not valid)
        return 3;
    return 0;
}
