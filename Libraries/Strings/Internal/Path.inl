// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Result.h"
#include "../../Strings/Path.h"
#include "../../Strings/StringBuilder.h"
#if SC_COMPILER_MSVC
#include "../../Foundation/StringPath.h"
#endif

struct SC::Path::Internal
{
    // Parses a windows drive (Example C:\\)
    static StringView parseWindowsRoot(StringView input)
    {
        auto func = [](auto it)
        {
            const auto      itBackup = it;
            StringCodePoint letter;
            if (it.advanceRead(letter))
            {
                if ((letter >= 'a' and letter <= 'z') or (letter >= 'A' and letter <= 'Z'))
                {
                    if (it.advanceIfMatches(':') and it.advanceIfMatchesAny({'\\', '/'}))
                    {
                        return StringView::fromIterators(itBackup, it);
                    }
                }
                // Try parsing UNC path
                it = itBackup;
                if (it.advanceIfMatches('\\') and it.advanceIfMatches('\\'))
                {
                    auto itCheckpoint = it;
                    // Try parsing long path form that includes ? and another backslash
                    if (it.advanceIfMatches('?') and it.advanceIfMatches('\\'))
                    {
                        return StringView::fromIterators(itBackup, it);
                    }
                    return StringView::fromIterators(itBackup, itCheckpoint);
                }
                else if (it.advanceIfMatches('/') and it.advanceIfMatches('/'))
                {
                    auto itCheckpoint = it;
                    // Try parsing long path form that includes ? and another backslash
                    if (it.advanceIfMatches('?') and it.advanceIfMatches('/'))
                    {
                        return StringView::fromIterators(itBackup, it);
                    }
                    return StringView::fromIterators(itBackup, itCheckpoint);
                }
            }
            return StringView();
        };
        return input.withIterator(func);
    }

    // Parses a posix root (Example /)
    static StringView parsePosixRoot(StringView input)
    {
        auto func = [](auto it)
        {
            if (it.advanceIfMatches('/'))
            {
                return StringView::fromIteratorFromStart(it);
            }
            return StringView();
        };
        return input.withIterator(func);
    }

    template <char separator1, char separator2>
    static StringView parseBase(StringView input)
    {
        auto func = [](auto it)
        {
            it.setToEnd();
            StringCodePoint matched;
            (void)it.reverseAdvanceUntilMatchesAny({separator1, separator2}, matched);
            if (it.isAtStart())
                it.setToEnd();
            else
                (void)it.stepForward();
            return StringView::fromIteratorUntilEnd(it);
        };
        return input.withIterator(func);
    }

    template <char separator1, char separator2>
    static bool rootIsFollowedByOnlySeparators(const StringView input, const StringView root)
    {
        if (not input.startsWith(root))
            return false;
        StringView remaining;
        if (not input.splitAfter(root, remaining))
            return false;
        auto func = [](auto it)
        {
            auto itBackup = it;
            if (not it.advanceUntilDifferentFrom(separator1))
            {
                it = itBackup;
                (void)it.advanceUntilDifferentFrom(separator2);
            }
            return it.isAtEnd();
        };
        return remaining.withIterator(func);
    }

    template <char separator1, char separator2>
    static StringView parseDirectory(const StringView input, const StringView root)
    {
        auto func = [&](auto it)
        {
            const auto itBackup = it;
            it.setToEnd();
            StringCodePoint matched;
            if (it.reverseAdvanceUntilMatchesAny({separator1, separator2}, matched))
            {
                const StringView directory = StringView::fromIterators(itBackup, it);
                if (directory.isEmpty())
                {
                    return root;
                }
                if (rootIsFollowedByOnlySeparators<separator1, separator2>(input, root))
                {
                    return input;
                }
                return directory;
            }
            return StringView();
        };
        return input.withIterator(func);
    }

    template <char separator1, char separator2>
    static SC::StringView dirname(StringView input, int repeat)
    {
        do
        {
            StringView dirnameOut;
            StringView base = basename<separator1, separator2>(input, &dirnameOut);
            if (dirnameOut.isEmpty())
            {
                return "."_a8;
            }
            else
            {
                input = dirnameOut;
            }
        } while (repeat-- > 0);
        return input;
    }

    template <char separator1, char separator2>
    static SC::StringView basename(StringView input, StringView* dir = nullptr)
    {
        auto func = [&](auto it)
        {
            it.setToEnd();
            while (it.stepBackward() and it.match(separator1)) {}
            auto itEnd = it;
            (void)itEnd.stepForward();
            StringCodePoint matched;
            if (it.reverseAdvanceUntilMatchesAny({separator1, separator2}, matched))
            {
                if (dir)
                {
                    *dir = StringView::fromIteratorFromStart(it);
                }
                (void)it.stepForward();
                return StringView::fromIterators(it, itEnd);
            }
            return input;
        };
        return input.withIterator(func);
    }

    template <char separator1, char separator2>
    static SC::StringView basename(StringView input, StringView suffix)
    {
        const StringView name = basename<separator1, separator2>(input);

        auto func = [&](auto inputIt, auto suffixIt) -> StringView
        {
            inputIt.setToEnd();
            suffixIt.setToEnd();
            StringCodePoint c;
            while (suffixIt.advanceBackwardRead(c))
            {
                if (not inputIt.advanceBackwardIfMatches(c))
                    return name; // Suffix not found
            }
            if (suffixIt.isAtStart())
            {
                return StringView::fromIteratorFromStart(inputIt); // Suffix found entirely
            }
            return name; // Suffix not found entirely
        };
        return StringView::withIterators(name, suffix, func);
    }

    /// Splits a Windows path of type "C:\\directory\\base" into root=C:\\ - directory=C:\\directory\\ - base=base
    /// @param input            Windows path of the form "C:\directory\base"
    /// @param root             if input == "C:\\directory\\base" it would return "C:\\"
    /// @param directory        if input == "C:\\directory\\base" it would return "C:\\directory\\"
    /// @param base             if input == "C:\\directory\\base" it would return "base"
    /// @param endsWithSeparator true if the path ends with a backslash
    [[nodiscard]] static bool parseWindows(StringView input, StringView& root, StringView& directory, StringView& base,
                                           bool& endsWithSeparator)
    {
        // Parse the drive, then look for rightmost backslash separator to get directory
        // Everything after it will be the base.
        root      = Internal::parseWindowsRoot(input);
        directory = Internal::parseDirectory<'\\', '/'>(input, root);
        if (root.startsWith(directory))
        {
            if (root.endsWithAnyOf({'\\', '/'}))
                directory = root;
        }
        base              = Internal::parseBase<'\\', '/'>(input);
        endsWithSeparator = input.endsWithAnyOf({'\\', '/'});
        return !(root.isEmpty() && directory.isEmpty());
    }

    /// Splits a Posix path of type "/usr/dir/base" into root=/ - directory=/usr/dir - base=base
    /// @param input            Posix path of the form "/usr/dir/base"
    /// @param root             if input == "/usr/dir/base" it would return "/"
    /// @param directory        if input == "/usr/dir/base" it would return "/usr/dir'"
    /// @param base             if input == "/usr/dir/base" it would return "base"
    /// @param endsWithSeparator true if the path ends with a backslash
    [[nodiscard]] static bool parsePosix(StringView input, StringView& root, StringView& directory, StringView& base,
                                         bool& endsWithSeparator)
    {
        root              = Internal::parsePosixRoot(input);
        directory         = Internal::parseDirectory<'/', '/'>(input, root);
        base              = Internal::parseBase<'/', '/'>(input);
        endsWithSeparator = input.endsWithAnyOf({'/'});
        return !(root.isEmpty() && directory.isEmpty());
    }
};

bool SC::Path::parseNameExtension(const StringView input, StringView& name, StringView& extension)
{
    return input.withIterator(
        [&](auto it)
        {
            auto itBackup = it;
            // Try searching for a '.' but if it's not found then just set the entire content
            // to be the name.
            it.setToEnd();
            if (it.reverseAdvanceUntilMatches('.'))
            {
                name = StringView::fromIterators(itBackup, it);   // from 'name.ext' keep 'name'
                (void)it.stepForward();                           // skip the .
                extension = StringView::fromIteratorUntilEnd(it); // from 'name.ext' keep 'ext'
            }
            else
            {
                name      = input;
                extension = StringView();
            }
            return !(name.isEmpty() && extension.isEmpty());
        });
}

bool SC::Path::parse(StringView input, Path::ParsedView& pathView, Type type)
{
    switch (type)
    {
    case AsWindows: return pathView.parseWindows(input);
    case AsPosix: return pathView.parsePosix(input);
    }
    Assert::unreachable();
}

bool SC::Path::ParsedView::parseWindows(StringView input)
{
    if (!Path::Internal::parseWindows(input, root, directory, base, endsWithSeparator))
    {
        return false;
    }
    if (!base.isEmpty())
    {
        if (!Path::parseNameExtension(base, name, ext))
            return false;
    }
    type = AsWindows;
    return true;
}

bool SC::Path::ParsedView::parsePosix(StringView input)
{
    if (!Path::Internal::parsePosix(input, root, directory, base, endsWithSeparator))
    {
        return false;
    }
    if (!base.isEmpty())
    {
        if (!Path::parseNameExtension(base, name, ext))
            return false;
    }
    type = AsPosix;
    return true;
}

SC::StringView SC::Path::dirname(StringView input, Type type, int repeat)
{
    switch (type)
    {
    case AsWindows: return Internal::dirname<Windows::Separator, Posix::Separator>(input, repeat);
    case AsPosix: return Internal::dirname<Posix::Separator, Posix::Separator>(input, repeat);
    }
    Assert::unreachable();
}

SC::StringView SC::Path::basename(StringView input, Type type)
{
    switch (type)
    {
    case AsWindows: return Internal::basename<Windows::Separator, Posix::Separator>(input);
    case AsPosix: return Internal::basename<Posix::Separator, Posix::Separator>(input);
    }
    Assert::unreachable();
}

SC::StringView SC::Path::basename(StringView input, StringView suffix)
{
    return Internal::basename<Windows::Separator, Posix::Separator>(input, suffix);
}

bool SC::Path::isAbsolute(StringView input, Type type)
{
    switch (type)
    {
    case AsPosix: return input.startsWithAnyOf({'/'});
    case AsWindows: return not Internal::parseWindowsRoot(input).isEmpty();
    }
    Assert::unreachable();
}

bool SC::Path::join(IGrowableBuffer&& output, StringEncoding encoding, Span<const StringView> inputs,
                    StringView separator, bool skipEmpty)
{
    StringBuilder sb(output, encoding, StringBuilder::Clear);
    const size_t  numElements = inputs.sizeInElements();
    for (size_t idx = 0; idx < numElements; ++idx)
    {
        const StringView element = inputs.data()[idx];
        if (skipEmpty and element.isEmpty())
            continue;
        SC_TRY(sb.append(element));
        if (idx + 1 != numElements)
        {
            SC_TRY(sb.append(separator));
        }
    }
    return true;
}

bool SC::Path::normalizeUNCAndTrimQuotes(IGrowableBuffer&& outputPath, StringEncoding encoding, StringView fileLocation,
                                         Type type, Span<StringView> components)
{
    // The macro escaping Library Path from defines adds escaped double quotes
    fileLocation = fileLocation.trimAnyOf({'"'});
#if SC_COMPILER_MSVC
    StringPath fixUncPathsOnMSVC;
    if (fileLocation.startsWithAnyOf({'\\'}) and not fileLocation.startsWith("\\\\"))
    {
        // On MSVC __FILE__ when building on UNC paths reports a single starting backslash...
        SC_TRY(fixUncPathsOnMSVC.append("\\"));
        SC_TRY(fixUncPathsOnMSVC.append(fileLocation));
        fileLocation = fixUncPathsOnMSVC.view();
    }
#endif
    SC_TRY(Path::normalize(move(outputPath), encoding, fileLocation, type, components));
    return true;
}

bool SC::Path::normalize(IGrowableBuffer&& output, StringEncoding encoding, StringView view, Type type,
                         Span<StringView> components)
{
    if (view.isEmpty())
        return false;
    auto newView = removeTrailingSeparator(view);
    if (not newView.isEmpty())
    {
        view = newView;
    }
    else
    {
        switch (type)
        {
        case AsWindows: view = "\\\\"; break;
        case AsPosix: view = "/"; break;
        }
        StringBuilder sb(output, encoding, StringBuilder::Clear);
        return sb.append(view);
    }
    bool normalizationHappened = false;

    StringViewTokenizer tokenizer(view);

    auto isDoubleDot = [](StringView it) -> bool
    {
        constexpr StringView utf8dot = "..";
#if SC_PLATFORM_WINDOWS
        constexpr StringView utf16dot = L"..";
        return it == (it.getEncoding() == StringEncoding::Utf16 ? utf16dot : utf8dot);
#else
        return it == utf8dot;
#endif
    };
    auto isDot = [&](StringView it) -> bool
    {
        constexpr StringView utf8dot = ".";
#if SC_PLATFORM_WINDOWS
        constexpr StringView utf16dot = L".";
        return it == (it.getEncoding() == StringEncoding::Utf16 ? utf16dot : utf8dot);
#else
        return it == utf8dot;
#endif
    };
    // Need to IncludeEmpty in order to preserve starting /
    size_t     numComponents = 0;
    StringView remaining     = tokenizer.remaining;
    while (tokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::IncludeEmpty))
    {
        const auto component = tokenizer.component;
        if (not component.isEmpty())
        {
            tokenizer = remaining;
            break;
        }
        remaining = tokenizer.remaining;
        numComponents++;
    }
    while (tokenizer.tokenizeNext({'/', '\\'}))
    {
        const auto component = tokenizer.component;

        if (tokenizer.splittingCharacter == '\\' and type == Type::AsPosix)
        {
            normalizationHappened = true;
        }
        else if (tokenizer.splittingCharacter == '/' and type == Type::AsWindows)
        {
            normalizationHappened = true;
        }
        if (isDoubleDot(component))
        {
            if (numComponents == 0 or isDoubleDot(components[numComponents - 1]))
            {
                if (numComponents >= components.sizeInElements())
                {
                    return false;
                }
                components[numComponents] = component;
                numComponents += 1;
            }
            else
            {
                numComponents -= 1;
            }
            normalizationHappened = true;
        }
        else if (isDot(component))
        {
            normalizationHappened = true;
        }
        else
        {
            if (numComponents >= components.sizeInElements())
            {
                return false;
            }
            components[numComponents] = component;
            numComponents += 1;
        }
    }
    normalizationHappened = normalizationHappened or (tokenizer.numSplitsTotal != tokenizer.numSplitsNonEmpty);
    if (normalizationHappened)
    {
        StringBuilder          sb(output, encoding, StringBuilder::Clear);
        Span<const StringSpan> inputs = {components.data(), numComponents};
        // Preserve UNC start '\\' even when normalizing as posix paths
        if (view.startsWith("\\\\"))
        {
            SC_TRY(sb.append("\\\\"))
            inputs = {inputs.data() + 2, numComponents - 2}; // first two components are empty
        }
        auto         separator   = type == AsPosix ? Posix::SeparatorStringView() : Windows::SeparatorStringView();
        const size_t numElements = inputs.sizeInElements();
        for (size_t idx = 0; idx < numElements; ++idx)
        {
            const StringView element = inputs.data()[idx];
            SC_TRY(sb.append(element));
            if (idx + 1 != numElements)
            {
                SC_TRY(sb.append(separator));
            }
        }
        return true;
    }
    else
    {
        return StringBuilder(output, encoding, StringBuilder::Clear).append(view);
    }
}

bool SC::Path::relativeFromTo(IGrowableBuffer&& output, StringEncoding encoding, StringView source,
                              StringView destination, Type inputType, Type outputType)
{
    bool skipRelativeCheck = false;
    if (inputType == AsPosix)
    {
        if (source.startsWith("\\\\"))
        {
            source            = source.sliceStart(2);
            skipRelativeCheck = true;
        }
        if (destination.startsWith("\\\\"))
        {
            destination       = destination.sliceStart(2);
            skipRelativeCheck = true;
        }
    }

    if (!skipRelativeCheck)
    {
        Path::ParsedView pathViewSource, pathViewDestination;
        SC_TRY(Path::parse(source, pathViewSource, inputType));
        SC_TRY(Path::parse(destination, pathViewDestination, inputType));
        if (pathViewSource.root.isEmpty() or pathViewDestination.root.isEmpty())
            return false; // Relative paths are not supported
    }

    if (source == destination)
    {
        return StringBuilder(output, encoding, StringBuilder::Clear).append(".");
    }

    const StringView separator =
        outputType == AsWindows ? Windows::SeparatorStringView() : Posix::SeparatorStringView();
    StringViewTokenizer srcTokenizer(source);
    StringViewTokenizer dstTokenizer(destination);

    size_t numMatches    = 0;
    size_t numSeparators = 0;

    StringBuilder builder(output, encoding, StringBuilder::Clear);
    StringView    destRemaining = destination;

    while (srcTokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::IncludeEmpty))
    {
        if (not dstTokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::IncludeEmpty) or
            (srcTokenizer.component != dstTokenizer.component))
        {
            numSeparators++;
            SC_TRY(builder.append(".."));
            break;
        }
        destRemaining = dstTokenizer.remaining;
        numMatches++;
    }

    if (numMatches == 0)
    {
        return false; // no common part
    }

    while (srcTokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::SkipEmpty))
    {
        if (numSeparators > 0)
        {
            SC_TRY(builder.append(separator));
        }
        numSeparators++;
        SC_TRY(builder.append(".."));
    }
    StringView destToAppend = Path::removeTrailingSeparator(destRemaining);
    if (not destToAppend.isEmpty())
    {
        if (numSeparators > 0)
        {
            SC_TRY(builder.append(separator));
        }
        SC_TRY(builder.append(Path::removeTrailingSeparator(destRemaining)));
    }
    return true;
}

SC::StringView SC::Path::removeTrailingSeparator(StringView path) { return path.trimEndAnyOf({'/', '\\'}); }

SC::StringView SC::Path::removeStartingSeparator(StringView path) { return path.trimStartAnyOf({'/', '\\'}); }

bool SC::Path::endsWithSeparator(StringView path) { return path.endsWithAnyOf({'/', '\\'}); }

bool SC::Path::append(IGrowableBuffer&& output, StringEncoding encoding, Span<const StringView> paths, Type type)
{
    StringBuilder builder(output, encoding, StringBuilder::Append);
    for (auto& path : paths)
    {
        if (isAbsolute(path, type))
        {
            return false;
        }
        switch (type)
        {
        case AsWindows: SC_TRY(builder.append(Windows::SeparatorStringView())); break;
        case AsPosix: SC_TRY(builder.append(Posix::SeparatorStringView())); break;
        }
        SC_TRY(builder.append(removeTrailingSeparator(path)));
    }
    return true;
}
