// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"
#include "StringsExport.h"

namespace SC
{
struct StringFormatOutput;

struct SC_STRINGS_EXPORT CommandLineParseResult
{
    enum class Status : uint8_t
    {
        Success,
        HelpRequested,
        Error
    };

    enum class Error : uint8_t
    {
        None,
        UnknownOption,
        MissingOptionValue,
        UnexpectedOptionValue,
        InvalidOptionValue,
        MissingPositionalValue,
        TooManyPositionals,
        InsufficientArgumentStorage,
        InsufficientPositionalStorage,
        InvalidShortOptionGroup
    };

    Status     status   = Status::Success;
    Error      error    = Error::None;
    StringView argument = {};
    StringView value    = {};
    StringView name     = {};
};

struct SC_STRINGS_EXPORT CommandLineValue
{
    enum class Type : uint8_t
    {
        None,
        Bool,
        Int32,
        UInt16,
        StringView,
        StringSpan,
        Custom
    };

    using CustomParser = bool (*)(void* userData, StringSpan value);

    Type         type        = Type::None;
    void*        storage     = nullptr;
    CustomParser customParse = nullptr;

    static CommandLineValue none();
    static CommandLineValue boolean(bool& value);
    static CommandLineValue int32(int32_t& value);
    static CommandLineValue uint16(uint16_t& value);
    static CommandLineValue stringView(SC::StringView& value);
    static CommandLineValue stringSpan(SC::StringSpan& value);
    static CommandLineValue custom(void* userData, CustomParser customParse);

    [[nodiscard]] bool requiresValue() const;
};

struct SC_STRINGS_EXPORT CommandLineOption
{
    StringView       longName          = {};
    StringView       negativeLongName  = {};
    StringView       help              = {};
    StringView       valueName         = {};
    char             shortName         = 0;
    char             negativeShortName = 0;
    bool             required          = false;
    CommandLineValue value             = {};
};

struct SC_STRINGS_EXPORT CommandLinePositional
{
    StringView name = {};
    StringView help = {};

    bool required  = true;
    bool remaining = false;

    CommandLineValue value = {};

    Span<StringSpan>  remainingStorage = {};
    Span<StringSpan>* parsedValues     = nullptr;
};

struct SC_STRINGS_EXPORT CommandLineSpec
{
    StringView programName = {};
    StringView summary     = {};

    Span<const CommandLineOption>     options     = {};
    Span<const CommandLinePositional> positionals = {};

    [[nodiscard]] CommandLineParseResult parse(Span<const StringSpan> args) const;

    [[nodiscard]] bool writeHelp(StringFormatOutput& output) const;
    [[nodiscard]] bool writeError(const CommandLineParseResult& result, StringFormatOutput& output) const;
};

struct SC_STRINGS_EXPORT CommandLineArguments
{
    Span<StringSpan> values = {};

    [[nodiscard]] Result setFromMainArguments(int argc, const char* const* argv, Span<StringSpan> storage);
    [[nodiscard]] Result setFromMainArguments(int argc, char** argv, Span<StringSpan> storage);
#if SC_PLATFORM_WINDOWS
    [[nodiscard]] Result setFromMainArguments(int argc, const wchar_t* const* argv, Span<StringSpan> storage);
    [[nodiscard]] Result setFromMainArguments(int argc, wchar_t** argv, Span<StringSpan> storage);
#endif
};

} // namespace SC
