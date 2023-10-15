// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../FileSystemWalker.h"

struct SC::FileSystemWalker::Internal
{
    Internal() {}
    ~Internal() {}

    [[nodiscard]] Result init(StringView directory) { return Result(true); }

    [[nodiscard]] Result enumerateNext(Entry& entry, const Options& options) { return Result(true); }

    [[nodiscard]] Result recurseSubdirectory(Entry& entry) { return Result(true); }
};
