#include "PlatformApplication.h"
#include "PlatformResource.h"

#include <stdio.h>

extern "C" const void* sapp_win32_get_hwnd(void);

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

const char* SC::PlatformResourceLoader::lookupPathNative(char* buffer, int bufferLength, const char* directory,
                                                         const char* file)
{
    char executablePath[2048] = {0};
    if (executablePath[0] == 0)
    {
        // TODO: This will fail on non ASCII paths
        GetModuleFileNameA(NULL, executablePath, 2048);
        PathRemoveFileSpecA(executablePath);
    }
    snprintf(buffer, bufferLength, "%s\\Resources\\%s\\%s", executablePath, directory, file);
    return buffer;
}

void SC::PlatformApplication::initNative()
{
    HWND  hwnd  = (HWND)sapp_win32_get_hwnd();
    HICON hIcon = LoadIcon(GetModuleHandleW(NULL), MAKEINTRESOURCE(100));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
}

void SC::PlatformApplication::openFiles() {}

void SC::PlatformApplication::saveFiles() {}
