// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H
#if SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H != 4
#error "PlatformType.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H 4 // Increment to indicate a new version of the file

namespace SC
{

/// @brief Indicates the current host platform
enum class Platform
{
    Apple,
    Linux,
    Windows,
    Emscripten,
    NetBSD,
    FreeBSD,
    OpenBSD,
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
        NetBSD,
        FreeBSD,
        OpenBSD,
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
#elif defined(__NetBSD__)
static constexpr Platform HostPlatform = Platform::NetBSD;
#elif defined(__FreeBSD__)
static constexpr Platform HostPlatform = Platform::FreeBSD;
#elif defined(__OpenBSD__)
static constexpr Platform HostPlatform = Platform::OpenBSD;
#else
#error "Unsupported platform"
#endif
} // namespace SC

#endif // SC_FOUNDATION_PLATFORM_TYPE_DEFINITION_H
