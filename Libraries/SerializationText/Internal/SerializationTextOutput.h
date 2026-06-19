// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/CompilerBuiltins.h"
#include "../../Common/IGrowableBuffer.h"
#include "../../Common/StringSpan.h"

namespace SC
{
struct SerializationTextOutput;
} // namespace SC

/// @brief Minimal transactional UTF-8/ASCII byte output used by Serialization Text writers.
struct SC::SerializationTextOutput
{
    IGrowableBuffer& buffer;
    size_t           backupSize = 0;

    SerializationTextOutput(IGrowableBuffer& buffer) : buffer(buffer) {}

    void onFormatBegin() { backupSize = buffer.getDirectAccess().sizeInBytes; }

    void onFormatFailed() { (void)buffer.resizeWithoutInitializing(backupSize); }

    [[nodiscard]] bool onFormatSucceeded() { return true; }

    [[nodiscard]] bool append(StringSpan text)
    {
        if (text.isEmpty())
        {
            return true;
        }
        const size_t oldSize = buffer.getDirectAccess().sizeInBytes;
        const size_t newSize = oldSize + text.sizeInBytes();
        if (not buffer.resizeWithoutInitializing(newSize))
        {
            return false;
        }
        CompilerBuiltins::copy(buffer.data() + oldSize, text.bytesWithoutTerminator(), text.sizeInBytes());
        return true;
    }
};
