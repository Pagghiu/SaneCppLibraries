#pragma once

namespace SC
{
struct PlatformResourceLoader
{
    static const char* lookupPathNative(char* buffer, int bufferLength, const char* directory, const char* file);
    template <int bufferLength>
    static const char* lookupPath(char (&buffer)[bufferLength], const char* directory, const char* file)
    {
        return lookupPathNative(buffer, bufferLength, directory, file);
    }
};
} // namespace SC
