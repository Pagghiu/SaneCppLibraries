// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Opaque.h"
#include "../Foundation/Result.h"
#include "../Foundation/StringView.h"
#include "../System/FileDescriptor.h"

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
        bool recursive = false;
    };

    Options options;

    ~FileSystemWalker();

    const Entry& get() const { return currentEntry; }

    [[nodiscard]] ReturnCode checkErrors()
    {
        errorsChecked = true;
        return errorResult;
    }
    [[nodiscard]] ReturnCode init(StringView directory);

    /// Returned string is only valid until next enumerateNext call and/or another init call
    [[nodiscard]] ReturnCode enumerateNext();

    [[nodiscard]] ReturnCode recurseSubdirectory();

  private:
    struct Internal;
    struct InternalSizes
    {
        static constexpr int Windows = 3216;
        static constexpr int Apple   = 2104;
        static constexpr int Default = sizeof(void*);
    };

  public:
    using InternalTraits = OpaqueTraits<Internal, InternalSizes>;

  private:
    using InternalOpaque = OpaqueUniqueObject<OpaqueFuncs<InternalTraits>>;
    InternalOpaque internal;

    Entry      currentEntry;
    ReturnCode errorResult   = true;
    bool       errorsChecked = false;
};
