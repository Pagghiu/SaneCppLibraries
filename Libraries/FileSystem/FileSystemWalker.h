// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../File/FileDescriptor.h"
#include "../Foundation/Language/Opaque.h"
#include "../Foundation/Language/Result.h"
#include "../Foundation/Strings/StringView.h"

namespace SC
{
struct FileSystemWalker;
} // namespace SC

struct SC::FileSystemWalker
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

    ~FileSystemWalker();

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
    struct InternalSizes
    {
        static constexpr int Windows = 4272;
        static constexpr int Apple   = 2104;
        static constexpr int Default = sizeof(void*);
    };

  public:
    using InternalTraits = OpaqueTraits<Internal, InternalSizes, alignof(uint64_t)>;

  private:
    using InternalOpaque = OpaqueUniqueObject<OpaqueFuncs<InternalTraits>>;
    InternalOpaque internal;

    Entry  currentEntry;
    Result errorResult   = Result(true);
    bool   errorsChecked = false;
};
