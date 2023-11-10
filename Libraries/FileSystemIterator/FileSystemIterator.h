// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../File/FileDescriptor.h"
#include "../Foundation/OpaqueObject.h"
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"

namespace SC
{
struct FileSystemIterator;
} // namespace SC

struct SC::FileSystemIterator
{
    enum class Type
    {
        Directory,
        File
    };
    struct Entry
    {
        StringView     name;
        StringView     path;
        uint32_t       level = 0;
        FileDescriptor parentFileDescriptor;
        Type           type = Type::File;
        bool           isDirectory() const { return type == Type::Directory; }
    };
    struct Options
    {
        bool recursive      = false;
        bool forwardSlashes = false;
    };

    Options options;

    ~FileSystemIterator();

    const Entry& get() const { return currentEntry; }

    [[nodiscard]] Result checkErrors()
    {
        errorsChecked = true;
        return errorResult;
    }
    [[nodiscard]] Result init(StringView directory);

    /// Returned string is only valid until next enumerateNext call and/or another init call
    [[nodiscard]] Result enumerateNext();

    [[nodiscard]] Result recurseSubdirectory();

  private:
    struct Internal;
    struct InternalDefinition
    {
        static constexpr int Windows = 4272;
        static constexpr int Apple   = 2104;
        static constexpr int Default = sizeof(void*);

        static constexpr size_t Alignment = alignof(void*);

        using Object = Internal;
    };

  public:
    using InternalOpaque = OpaqueObject<InternalDefinition>;

  private:
    InternalOpaque internal;

    Entry  currentEntry;
    Result errorResult   = Result(true);
    bool   errorsChecked = false;
};
