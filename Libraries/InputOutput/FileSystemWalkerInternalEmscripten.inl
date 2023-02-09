// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemWalker.h"

struct SC::FileSystemWalker::Internal
{
    Internal() {}
    ~Internal() {}

    [[nodiscard]] ReturnCode init(StringView directory) { return true; }

    [[nodiscard]] ReturnCode enumerateNext(Entry& entry, const Options& options) { return true; }

    [[nodiscard]] ReturnCode recurseSubdirectory(Entry& entry) { return true; }
};
