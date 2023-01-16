// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

namespace SC
{
struct PlatformApplication
{
    static void draw();
    static void openFiles();
    static void saveFiles();

    struct Internal;

  private:
    static void initNative();
};
} // namespace SC
