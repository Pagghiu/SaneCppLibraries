// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <stddef.h>

#ifdef SC_TEST_CURL_MISSING
#include <dlfcn.h>
#include <string.h>

void* dlopen(const char* name, int mode)
{
    if (name != NULL && (strcmp(name, "libcurl.so.4") == 0 || strcmp(name, "libcurl.so") == 0))
        return NULL;

    void* (*openLibrary)(const char*, int) = dlsym(RTLD_NEXT, "dlopen");
    return openLibrary(name, mode);
}
#else
struct VersionInfo
{
    int age;
    const char* version;
    unsigned int versionNumber;
    const char* host;
    int features;
};

void* curl_easy_init(void) { return NULL; }
void curl_easy_cleanup(void* handle) { (void)handle; }
void curl_easy_reset(void* handle) { (void)handle; }
int curl_easy_perform(void* handle) { (void)handle; return 1; }
int curl_easy_setopt(void* handle, int option, ...) { (void)handle; (void)option; return 1; }
int curl_easy_getinfo(void* handle, int option, ...) { (void)handle; (void)option; return 1; }
void* curl_slist_append(void* list, const char* value) { (void)list; (void)value; return NULL; }

#ifndef SC_TEST_CURL_INCOMPLETE
void curl_slist_free_all(void* list) { (void)list; }
#endif

const struct VersionInfo* curl_version_info(int age)
{
    static const struct VersionInfo version = {0, "test fixture", 0, "fixture", SC_TEST_CURL_FEATURES};
    (void)age;
    return &version;
}
#endif
