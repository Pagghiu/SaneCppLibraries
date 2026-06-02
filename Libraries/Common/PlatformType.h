// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H
#if SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H != 1
#error "PlatformType.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H 1 // Increment to indicate a new version of the file

namespace SC
{

/// @brief Indicates the current host platform
enum class Platform
{
    Apple,
    Linux,
    Windows,
    Emscripten,
};

/// @brief Holds information about operating system
struct OperatingSystem
{
    enum Type
    {
        macOS,
        iOS,
        Emscripten,
        Windows,
        Linux,
    };
};

#if defined(__APPLE__)
static constexpr Platform HostPlatform = Platform::Apple;
#elif defined(_WIN32) || defined(_WIN64)
static constexpr Platform HostPlatform = Platform::Windows;
#elif defined(__EMSCRIPTEN__)
static constexpr Platform HostPlatform = Platform::Emscripten;
#elif defined(__linux__)
static constexpr Platform HostPlatform = Platform::Linux;
#else
#error "Unsupported platform"
#endif
} // namespace SC

#endif // SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H
