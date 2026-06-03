// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Strings/CommandLine.h"
#include "../../Strings/StringFormat.h"

namespace SC
{
namespace CommandLineInternal
{
static bool appendAsciiChar(StringFormatOutput& output, char c)
{
    return output.append(StringView({&c, 1}, false, StringEncoding::Ascii));
}

static bool appendSpaces(StringFormatOutput& output, size_t numSpaces)
{
    static constexpr StringView spaces = "                                ";
    while (numSpaces > 0)
    {
        const size_t current = min(numSpaces, spaces.sizeInBytes());
        if (not output.append(spaces.sliceStartLength(0, current)))
        {
            return false;
        }
        numSpaces -= current;
    }
    return true;
}

static bool appendOptionName(StringFormatOutput& output, char shortName, StringView longName)
{
    if (shortName != 0 and not longName.isEmpty())
    {
        return output.append("-") and appendAsciiChar(output, shortName) and output.append(", --") and
               output.append(longName);
    }
    if (shortName != 0)
    {
        return output.append("-") and appendAsciiChar(output, shortName);
    }
    if (not longName.isEmpty())
    {
        return output.append("--") and output.append(longName);
    }
    return true;
}

static bool appendOptionDisplay(StringFormatOutput& output, const CommandLineOption& option)
{
    bool appendedAny = false;
    if (option.shortName != 0 or not option.longName.isEmpty())
    {
        if (not appendOptionName(output, option.shortName, option.longName))
        {
            return false;
        }
        appendedAny = true;
    }
    if (option.negativeShortName != 0 or not option.negativeLongName.isEmpty())
    {
        if (appendedAny and not output.append(", "))
        {
            return false;
        }
        if (not appendOptionName(output, option.negativeShortName, option.negativeLongName))
        {
            return false;
        }
    }
    if (option.value.requiresValue())
    {
        const StringView valueName = option.valueName.isEmpty() ? "VALUE" : option.valueName;
        if (not output.append(" <") or not output.append(valueName) or not output.append(">"))
        {
            return false;
        }
    }
    return true;
}

static size_t countCodePoints(StringView value)
{
    size_t numCodePoints = 0;
    value.withIterator(
        [&](auto it)
        {
            StringCodePoint codePoint = 0;
            while (it.advanceRead(codePoint))
            {
                (void)codePoint;
                numCodePoints += 1;
            }
        });
    return numCodePoints;
}

static size_t computeOptionDisplayWidth(const CommandLineOption& option)
{
    size_t width = 0;
    if (option.shortName != 0)
    {
        width += 2;
        if (not option.longName.isEmpty())
        {
            width += 2;
        }
    }
    if (not option.longName.isEmpty())
    {
        width += 2 + option.longName.sizeInBytes();
    }
    if (option.negativeShortName != 0 or not option.negativeLongName.isEmpty())
    {
        if (width > 0)
        {
            width += 2;
        }
        if (option.negativeShortName != 0)
        {
            width += 2;
            if (not option.negativeLongName.isEmpty())
            {
                width += 2;
            }
        }
        if (not option.negativeLongName.isEmpty())
        {
            width += 2 + option.negativeLongName.sizeInBytes();
        }
    }
    if (option.value.requiresValue())
    {
        const StringView valueName = option.valueName.isEmpty() ? "VALUE" : option.valueName;
        width += 3 + valueName.sizeInBytes();
    }
    return width;
}

static size_t computePositionalDisplayWidth(const CommandLinePositional& positional)
{
    size_t width = positional.name.sizeInBytes() + 2;
    if (positional.remaining)
    {
        width += 3;
    }
    return width;
}

static bool appendPositionalDisplay(StringFormatOutput& output, const CommandLinePositional& positional)
{
    if (positional.remaining)
    {
        return output.append("<") and output.append(positional.name) and output.append("...>");
    }
    return output.append("<") and output.append(positional.name) and output.append(">");
}

static bool readAsciiShortName(StringView token, size_t index, char& shortName)
{
    StringCodePoint codePoint = 0;
    bool            success   = false;
    token.withIterator(
        [&](auto it)
        {
            if (it.advanceCodePoints(index) and it.read(codePoint))
            {
                success = codePoint <= 0x7f;
            }
        });
    if (success)
    {
        shortName = static_cast<char>(codePoint);
    }
    return success;
}

static bool isLongOptionToken(StringView argument) { return argument.startsWith("--") and argument != "--"; }

static bool isShortOptionToken(StringView argument)
{
    return argument.startsWith("-") and not argument.startsWith("--") and argument != "-";
}

struct MatchedOption
{
    const CommandLineOption* option       = nullptr;
    bool                     boolAssigned = true;
    bool                     valid        = false;
};

static MatchedOption matchLongOption(const CommandLineSpec& spec, StringView name)
{
    for (const auto& option : spec.options)
    {
        if (not option.longName.isEmpty() and name == option.longName)
        {
            return {&option, true, true};
        }
        if ((option.value.type == CommandLineValue::Type::Bool or option.value.type == CommandLineValue::Type::None) and
            not option.negativeLongName.isEmpty() and name == option.negativeLongName)
        {
            return {&option, false, true};
        }
    }
    return {};
}

static MatchedOption matchShortOption(const CommandLineSpec& spec, char shortName)
{
    for (const auto& option : spec.options)
    {
        if (option.shortName != 0 and option.shortName == shortName)
        {
            return {&option, true, true};
        }
        if ((option.value.type == CommandLineValue::Type::Bool or option.value.type == CommandLineValue::Type::None) and
            option.negativeShortName != 0 and option.negativeShortName == shortName)
        {
            return {&option, false, true};
        }
    }
    return {};
}

static CommandLineParseResult makeError(CommandLineParseResult::Error error, StringView argument = {},
                                        StringView value = {}, StringView name = {})
{
    CommandLineParseResult result;
    result.status   = CommandLineParseResult::Status::Error;
    result.error    = error;
    result.argument = argument;
    result.value    = value;
    result.name     = name;
    return result;
}

static CommandLineParseResult assignTypedValue(const CommandLineValue& valueSpec, StringSpan value, StringView argument,
                                               StringView name)
{
    switch (valueSpec.type)
    {
    case CommandLineValue::Type::None: return {};
    case CommandLineValue::Type::Bool: *static_cast<bool*>(valueSpec.storage) = true; return {};
    case CommandLineValue::Type::Int32: {
        int32_t parsed = 0;
        if (not StringView(value).parseInt32(parsed))
        {
            return makeError(CommandLineParseResult::Error::InvalidOptionValue, argument, StringView(value), name);
        }
        *static_cast<int32_t*>(valueSpec.storage) = parsed;
        return {};
    }
    case CommandLineValue::Type::UInt16: {
        int32_t parsed = 0;
        if (not StringView(value).parseInt32(parsed) or parsed < 0 or parsed > 65535)
        {
            return makeError(CommandLineParseResult::Error::InvalidOptionValue, argument, StringView(value), name);
        }
        *static_cast<uint16_t*>(valueSpec.storage) = static_cast<uint16_t>(parsed);
        return {};
    }
    case CommandLineValue::Type::StringView:
        *static_cast<StringView*>(valueSpec.storage) = StringView(value);
        return {};
    case CommandLineValue::Type::StringSpan: *static_cast<StringSpan*>(valueSpec.storage) = value; return {};
    case CommandLineValue::Type::Custom:
        if (valueSpec.customParse and valueSpec.customParse(valueSpec.storage, value))
        {
            return {};
        }
        return makeError(CommandLineParseResult::Error::InvalidOptionValue, argument, StringView(value), name);
    }
    return makeError(CommandLineParseResult::Error::InvalidOptionValue, argument, StringView(value), name);
}

static CommandLineParseResult assignBooleanValue(const CommandLineValue& valueSpec, bool assignedValue,
                                                 StringView argument, StringView name)
{
    if (valueSpec.type == CommandLineValue::Type::Bool)
    {
        *static_cast<bool*>(valueSpec.storage) = assignedValue;
    }
    else if (valueSpec.type == CommandLineValue::Type::None)
    {
        (void)assignedValue;
    }
    else
    {
        return makeError(CommandLineParseResult::Error::UnexpectedOptionValue, argument, {}, name);
    }
    return {};
}

static bool isBuiltInHelpToken(StringView argument)
{
    if (argument == "--help" or argument == "-h")
    {
        return true;
    }
    if (isShortOptionToken(argument) and argument.sizeInBytes() > 2)
    {
        StringView shortGroup = argument.sliceStart(1);
        bool       foundHelp  = false;
        shortGroup.withIterator(
            [&](auto it)
            {
                StringCodePoint codePoint = 0;
                while (it.advanceRead(codePoint))
                {
                    if (codePoint == 'h')
                    {
                        foundHelp = true;
                        return;
                    }
                }
            });
        return foundHelp;
    }
    return false;
}

static bool optionWasProvidedBeforeTerminator(const CommandLineOption& option, Span<const StringSpan> args)
{
    for (size_t idx = 0; idx < args.sizeInElements(); ++idx)
    {
        const StringView argument(args[idx]);
        if (argument == "--")
        {
            return false;
        }
        if (isBuiltInHelpToken(argument))
        {
            continue;
        }
        if (isLongOptionToken(argument))
        {
            StringView longPart = argument.sliceStart(2);
            StringView longName;
            if (longPart.splitBefore("=", longName))
            {
                longPart = longName;
            }
            if ((not option.longName.isEmpty() and longPart == option.longName) or
                (not option.negativeLongName.isEmpty() and longPart == option.negativeLongName))
            {
                return true;
            }
            continue;
        }
        if (isShortOptionToken(argument))
        {
            StringView group       = argument.sliceStart(1);
            const bool singleShort = countCodePoints(group) == 1;
            size_t     shortIndex  = 0;
            char       shortName   = 0;
            while (readAsciiShortName(group, shortIndex, shortName))
            {
                if ((option.shortName != 0 and option.shortName == shortName) or
                    (option.negativeShortName != 0 and option.negativeShortName == shortName))
                {
                    return true;
                }
                if (singleShort)
                {
                    break;
                }
                ++shortIndex;
            }
        }
    }
    return false;
}

static CommandLineParseResult finishPositionalChecks(const CommandLineSpec& spec, size_t positionalIndex)
{
    for (size_t idx = positionalIndex; idx < spec.positionals.sizeInElements(); ++idx)
    {
        const auto& positional = spec.positionals[idx];
        if (positional.required)
        {
            return makeError(CommandLineParseResult::Error::MissingPositionalValue, {}, {}, positional.name);
        }
        if (positional.remaining)
        {
            break;
        }
    }
    return {};
}

static CommandLineParseResult consumePositional(const CommandLineSpec& spec, size_t& positionalIndex,
                                                StringSpan argument)
{
    if (positionalIndex >= spec.positionals.sizeInElements())
    {
        return makeError(CommandLineParseResult::Error::TooManyPositionals, StringView(argument));
    }

    const auto& positional = spec.positionals[positionalIndex];
    if (positional.remaining)
    {
        SC_STRINGS_ASSERT_RELEASE(positional.parsedValues != nullptr);
        const size_t used = positional.parsedValues->sizeInElements();
        if (used >= positional.remainingStorage.sizeInElements())
        {
            return makeError(CommandLineParseResult::Error::InsufficientPositionalStorage, StringView(argument), {},
                             positional.name);
        }
        Span<StringSpan> remainingStorage = positional.remainingStorage;
        remainingStorage[used]            = argument;
        *positional.parsedValues          = {remainingStorage.data(), used + 1};
        return {};
    }

    const auto result = assignTypedValue(positional.value, argument, StringView(argument), positional.name);
    if (result.status == CommandLineParseResult::Status::Error)
    {
        return result;
    }
    positionalIndex += 1;
    return {};
}

} // namespace CommandLineInternal
} // namespace SC

SC::CommandLineValue SC::CommandLineValue::none() { return {}; }

SC::CommandLineValue SC::CommandLineValue::boolean(bool& value)
{
    CommandLineValue result;
    result.type    = Type::Bool;
    result.storage = &value;
    return result;
}

SC::CommandLineValue SC::CommandLineValue::int32(int32_t& value)
{
    CommandLineValue result;
    result.type    = Type::Int32;
    result.storage = &value;
    return result;
}

SC::CommandLineValue SC::CommandLineValue::uint16(uint16_t& value)
{
    CommandLineValue result;
    result.type    = Type::UInt16;
    result.storage = &value;
    return result;
}

SC::CommandLineValue SC::CommandLineValue::stringView(SC::StringView& value)
{
    CommandLineValue result;
    result.type    = Type::StringView;
    result.storage = &value;
    return result;
}

SC::CommandLineValue SC::CommandLineValue::stringSpan(SC::StringSpan& value)
{
    CommandLineValue result;
    result.type    = Type::StringSpan;
    result.storage = &value;
    return result;
}

SC::CommandLineValue SC::CommandLineValue::custom(void* userData, CustomParser customParse)
{
    CommandLineValue result;
    result.type        = Type::Custom;
    result.storage     = userData;
    result.customParse = customParse;
    return result;
}

bool SC::CommandLineValue::requiresValue() const { return type != Type::None and type != Type::Bool; }

template <typename CharType>
static SC::Result commandLineSetMainArgumentsImpl(int argc, const CharType* const* argv,
                                                  SC::Span<SC::StringSpan> storage, SC::CommandLineArguments& arguments,
                                                  SC::StringEncoding encoding)
{
    SC_TRY_MSG(argc >= 0, "CommandLine argc cannot be negative");
    const SC::size_t numArgs = argc > 0 ? static_cast<SC::size_t>(argc - 1) : 0;
    SC_TRY_MSG(storage.sizeInElements() >= numArgs, "CommandLine insufficient argument storage");
    for (SC::size_t idx = 0; idx < numArgs; ++idx)
    {
        storage[idx] = SC::StringSpan::fromNullTerminated(argv[idx + 1], encoding);
    }
    arguments.values = {storage.data(), numArgs};
    return SC::Result(true);
}

SC::Result SC::CommandLineArguments::setFromMainArguments(int argc, const char* const* argv, Span<StringSpan> storage)
{
    return commandLineSetMainArgumentsImpl(argc, argv, storage, *this, StringEncoding::Utf8);
}

SC::Result SC::CommandLineArguments::setFromMainArguments(int argc, char** argv, Span<StringSpan> storage)
{
    return commandLineSetMainArgumentsImpl(argc, const_cast<const char* const*>(argv), storage, *this,
                                           StringEncoding::Utf8);
}

#if SC_PLATFORM_WINDOWS
SC::Result SC::CommandLineArguments::setFromMainArguments(int argc, const wchar_t* const* argv,
                                                          Span<StringSpan> storage)
{
    return commandLineSetMainArgumentsImpl(argc, argv, storage, *this, StringEncoding::Native);
}

SC::Result SC::CommandLineArguments::setFromMainArguments(int argc, wchar_t** argv, Span<StringSpan> storage)
{
    return commandLineSetMainArgumentsImpl(argc, const_cast<const wchar_t* const*>(argv), storage, *this,
                                           StringEncoding::Native);
}
#endif

SC::CommandLineParseResult SC::CommandLineSpec::parse(Span<const StringSpan> args) const
{
    for (const auto& positional : positionals)
    {
        if (positional.parsedValues)
        {
            *positional.parsedValues = {};
        }
    }

    size_t positionalIndex    = 0;
    bool   positionalOnlyMode = false;
    for (size_t idx = 0; idx < args.sizeInElements(); ++idx)
    {
        const StringView argument(args[idx]);

        if (argument == "--help" or argument == "-h")
        {
            CommandLineParseResult result;
            result.status = CommandLineParseResult::Status::HelpRequested;
            return result;
        }
        if (argument == "--")
        {
            positionalOnlyMode = true;
            continue;
        }

        if (not positionalOnlyMode and CommandLineInternal::isLongOptionToken(argument))
        {
            StringView longPart = argument.sliceStart(2);
            StringView inlineValue;
            StringView longName       = longPart;
            const bool hasInlineValue = longPart.splitAfter("=", inlineValue);
            if (hasInlineValue)
            {
                (void)longPart.splitBefore("=", longName);
            }

            const auto matched = CommandLineInternal::matchLongOption(*this, longName);
            if (not matched.valid)
            {
                return CommandLineInternal::makeError(CommandLineParseResult::Error::UnknownOption, argument);
            }

            if (matched.option->value.requiresValue())
            {
                if (not matched.boolAssigned)
                {
                    return CommandLineInternal::makeError(CommandLineParseResult::Error::UnknownOption, argument);
                }
                if (not hasInlineValue)
                {
                    if (idx + 1 >= args.sizeInElements())
                    {
                        return CommandLineInternal::makeError(CommandLineParseResult::Error::MissingOptionValue,
                                                              argument, {}, matched.option->longName);
                    }
                    const auto valueResult = CommandLineInternal::assignTypedValue(matched.option->value, args[idx + 1],
                                                                                   argument, matched.option->longName);
                    if (valueResult.status == CommandLineParseResult::Status::Error)
                    {
                        return valueResult;
                    }
                    idx += 1;
                    continue;
                }

                const auto valueResult = CommandLineInternal::assignTypedValue(matched.option->value, inlineValue,
                                                                               argument, matched.option->longName);
                if (valueResult.status == CommandLineParseResult::Status::Error)
                {
                    return valueResult;
                }
                continue;
            }

            if (hasInlineValue)
            {
                return CommandLineInternal::makeError(CommandLineParseResult::Error::UnexpectedOptionValue, argument,
                                                      inlineValue, matched.option->longName);
            }

            const auto boolResult = CommandLineInternal::assignBooleanValue(
                matched.option->value, matched.boolAssigned, argument,
                matched.boolAssigned ? matched.option->longName : matched.option->negativeLongName);
            if (boolResult.status == CommandLineParseResult::Status::Error)
            {
                return boolResult;
            }
            continue;
        }

        if (not positionalOnlyMode and CommandLineInternal::isShortOptionToken(argument))
        {
            StringView shortGroup = argument.sliceStart(1);
            if (CommandLineInternal::countCodePoints(shortGroup) == 1)
            {
                char shortName = 0;
                if (not CommandLineInternal::readAsciiShortName(shortGroup, 0, shortName))
                {
                    return CommandLineInternal::makeError(CommandLineParseResult::Error::UnknownOption, argument);
                }
                if (shortName == 'h')
                {
                    CommandLineParseResult result;
                    result.status = CommandLineParseResult::Status::HelpRequested;
                    return result;
                }
                const auto matched = CommandLineInternal::matchShortOption(*this, shortName);
                if (not matched.valid)
                {
                    return CommandLineInternal::makeError(CommandLineParseResult::Error::UnknownOption, argument);
                }
                if (matched.option->value.requiresValue())
                {
                    if (idx + 1 >= args.sizeInElements())
                    {
                        return CommandLineInternal::makeError(CommandLineParseResult::Error::MissingOptionValue,
                                                              argument, {}, matched.option->longName);
                    }
                    const auto valueResult = CommandLineInternal::assignTypedValue(matched.option->value, args[idx + 1],
                                                                                   argument, matched.option->longName);
                    if (valueResult.status == CommandLineParseResult::Status::Error)
                    {
                        return valueResult;
                    }
                    idx += 1;
                    continue;
                }

                const auto boolResult = CommandLineInternal::assignBooleanValue(
                    matched.option->value, matched.boolAssigned, argument,
                    matched.boolAssigned ? matched.option->longName : matched.option->negativeLongName);
                if (boolResult.status == CommandLineParseResult::Status::Error)
                {
                    return boolResult;
                }
                continue;
            }

            size_t shortIndex = 0;
            char   shortName  = 0;
            while (CommandLineInternal::readAsciiShortName(shortGroup, shortIndex, shortName))
            {
                if (shortName == 'h')
                {
                    CommandLineParseResult result;
                    result.status = CommandLineParseResult::Status::HelpRequested;
                    return result;
                }
                const auto matched = CommandLineInternal::matchShortOption(*this, shortName);
                if (not matched.valid)
                {
                    return CommandLineInternal::makeError(CommandLineParseResult::Error::UnknownOption, argument);
                }
                if (matched.option->value.requiresValue())
                {
                    return CommandLineInternal::makeError(CommandLineParseResult::Error::InvalidShortOptionGroup,
                                                          argument, {}, matched.option->longName);
                }

                const auto boolResult = CommandLineInternal::assignBooleanValue(
                    matched.option->value, matched.boolAssigned, argument,
                    matched.boolAssigned ? matched.option->longName : matched.option->negativeLongName);
                if (boolResult.status == CommandLineParseResult::Status::Error)
                {
                    return boolResult;
                }
                shortIndex += 1;
            }
            continue;
        }

        const auto positionalResult = CommandLineInternal::consumePositional(*this, positionalIndex, args[idx]);
        if (positionalResult.status == CommandLineParseResult::Status::Error)
        {
            return positionalResult;
        }
        if (positionalIndex < positionals.sizeInElements() and positionals[positionalIndex].remaining)
        {
            continue;
        }
    }

    for (const auto& option : options)
    {
        if (option.required and not CommandLineInternal::optionWasProvidedBeforeTerminator(option, args))
        {
            return CommandLineInternal::makeError(CommandLineParseResult::Error::MissingOptionValue, {}, {},
                                                  option.longName);
        }
    }

    return CommandLineInternal::finishPositionalChecks(*this, positionalIndex);
}

bool SC::CommandLineSpec::writeHelp(StringFormatOutput& output) const
{
    const StringView displayedProgramName = this->programName.isEmpty() ? StringView("program") : this->programName;
    if (not output.append("Usage: ") or not output.append(displayedProgramName))
    {
        return false;
    }
    if (options.sizeInElements() > 0)
    {
        if (not output.append(" [options]"))
        {
            return false;
        }
    }
    for (const auto& positional : positionals)
    {
        if (positional.required)
        {
            if (not output.append(" "))
            {
                return false;
            }
        }
        else
        {
            if (not output.append(" ["))
            {
                return false;
            }
        }
        if (positional.remaining)
        {
            if (not output.append("<") or not output.append(positional.name) or not output.append("...>"))
            {
                return false;
            }
        }
        else
        {
            if (not output.append("<") or not output.append(positional.name) or not output.append(">"))
            {
                return false;
            }
        }
        if (not positional.required)
        {
            if (not output.append("]"))
            {
                return false;
            }
        }
    }
    if (not output.append("\n"))
    {
        return false;
    }

    if (not summary.isEmpty())
    {
        if (not output.append("\n") or not output.append(summary) or not output.append("\n"))
        {
            return false;
        }
    }

    size_t maxOptionWidth = 12;
    for (const auto& option : options)
    {
        maxOptionWidth = max(maxOptionWidth, CommandLineInternal::computeOptionDisplayWidth(option));
    }

    if (not output.append("\nOptions:\n"))
    {
        return false;
    }
    if (not output.append("  -h, --help"))
    {
        return false;
    }
    if (not CommandLineInternal::appendSpaces(output, maxOptionWidth - 10 + 2))
    {
        return false;
    }
    if (not output.append("Show this help message\n"))
    {
        return false;
    }
    for (const auto& option : options)
    {
        if (not output.append("  "))
        {
            return false;
        }
        if (not CommandLineInternal::appendOptionDisplay(output, option))
        {
            return false;
        }
        const size_t width = CommandLineInternal::computeOptionDisplayWidth(option);
        if (not CommandLineInternal::appendSpaces(output, maxOptionWidth - width + 2))
        {
            return false;
        }
        if (not output.append(option.help))
        {
            return false;
        }
        if (option.required)
        {
            if (not output.append(" (required)"))
            {
                return false;
            }
        }
        if (not output.append("\n"))
        {
            return false;
        }
    }

    if (positionals.sizeInElements() > 0)
    {
        size_t maxPositionalWidth = 0;
        for (const auto& positional : positionals)
        {
            maxPositionalWidth =
                max(maxPositionalWidth, CommandLineInternal::computePositionalDisplayWidth(positional));
        }
        if (not output.append("\nPositionals:\n"))
        {
            return false;
        }
        for (const auto& positional : positionals)
        {
            if (not output.append("  "))
            {
                return false;
            }
            if (not CommandLineInternal::appendPositionalDisplay(output, positional))
            {
                return false;
            }
            const size_t width = CommandLineInternal::computePositionalDisplayWidth(positional);
            if (not CommandLineInternal::appendSpaces(output, maxPositionalWidth - width + 2))
            {
                return false;
            }
            if (not output.append(positional.help))
            {
                return false;
            }
            if (not positional.required)
            {
                if (not output.append(" (optional)"))
                {
                    return false;
                }
            }
            if (not output.append("\n"))
            {
                return false;
            }
        }
    }
    return true;
}

bool SC::CommandLineSpec::writeError(const CommandLineParseResult& result, StringFormatOutput& output) const
{
    (void)this;
    switch (result.error)
    {
    case CommandLineParseResult::Error::None: return true;
    case CommandLineParseResult::Error::UnknownOption:
        return output.append("Unknown option: ") and output.append(result.argument) and
               output.append("\nUse --help to show usage.\n");
    case CommandLineParseResult::Error::MissingOptionValue:
        if (not result.argument.isEmpty())
        {
            return output.append("Missing value for option: ") and output.append(result.argument) and
                   output.append("\nUse --help to show usage.\n");
        }
        return output.append("Missing required option: --") and output.append(result.name) and
               output.append("\nUse --help to show usage.\n");
    case CommandLineParseResult::Error::UnexpectedOptionValue:
        return output.append("Option does not accept a value: ") and output.append(result.argument) and
               output.append("\nUse --help to show usage.\n");
    case CommandLineParseResult::Error::InvalidOptionValue:
        return output.append("Invalid value for option --") and output.append(result.name) and output.append(": ") and
               output.append(result.value) and output.append("\nUse --help to show usage.\n");
    case CommandLineParseResult::Error::MissingPositionalValue:
        return output.append("Missing required positional argument: ") and output.append(result.name) and
               output.append("\nUse --help to show usage.\n");
    case CommandLineParseResult::Error::TooManyPositionals:
        return output.append("Unexpected positional argument: ") and output.append(result.argument) and
               output.append("\nUse --help to show usage.\n");
    case CommandLineParseResult::Error::InsufficientArgumentStorage:
        return output.append("Insufficient argument storage\n");
    case CommandLineParseResult::Error::InsufficientPositionalStorage:
        return output.append("Insufficient storage for remaining positional arguments: ") and
               output.append(result.name) and output.append("\n");
    case CommandLineParseResult::Error::InvalidShortOptionGroup:
        return output.append("Cannot group short options when one requires a value: ") and
               output.append(result.argument) and output.append("\nUse --help to show usage.\n");
    }
    return false;
}
