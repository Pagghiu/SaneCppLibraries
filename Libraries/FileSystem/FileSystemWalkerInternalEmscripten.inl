// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystemWalker.h"

struct SC::FileSystemWalker::Internal
{
    Internal() {}
    ~Internal() {}

    [[nodiscard]] ReturnCode init(StringView directory) { return ReturnCode(true); }

    [[nodiscard]] ReturnCode enumerateNext(Entry& entry, const Options& options) { return ReturnCode(true); }

    [[nodiscard]] ReturnCode recurseSubdirectory(Entry& entry) { return ReturnCode(true); }
};
