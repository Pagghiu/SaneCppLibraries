// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystemIterator.h"

struct SC::FileSystemIterator::Internal
{
    Internal() {}
    ~Internal() {}

    [[nodiscard]] Result init(StringView directory) { return Result(true); }

    [[nodiscard]] Result enumerateNext(Entry& entry, const Options& options) { return Result(true); }

    [[nodiscard]] Result recurseSubdirectory(Entry& entry) { return Result(true); }
};
