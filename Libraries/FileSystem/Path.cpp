// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Path.h"
#include "../Foundation/StringBuilder.h" // join

struct SC::Path::Internal
{
    // Parses a windows drive (Example C:\\)
    template <typename StringIterator>
    static StringView parseWindowsRootTemplate(StringIterator it)
    {
        const auto                         itBackup = it;
        typename StringIterator::CodePoint letter;
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
        }
        return StringView();
    }

    static StringView parseWindowsRoot(StringView input)
    {
        if (input.getEncoding() == StringEncoding::Utf16)
            return parseWindowsRootTemplate(input.getIterator<StringIteratorUTF16>());
        else
            return parseWindowsRootTemplate(input.getIterator<StringIteratorASCII>());
    }
    // Parses a Posix root
    static StringView parsePosixRoot(StringView input)
    {
        if (input.startsWithChar('/'))
        {
            // we want to return a string view pointing at the "/" char of the input string
            return input.sliceStartLength(0, 1);
        }
        return StringView();
    }

    template <typename StringIterator, char separator>
    static StringView parseBaseTemplate(StringView input)
    {
        // Parse the base
        auto it = input.getIterator<StringIterator>();
        it.setToEnd();
        (void)it.reverseAdvanceUntilMatches(separator);
        (void)it.stepForward();
        return StringView::fromIteratorUntilEnd(it);
    }

    template <char separator>
    static StringView parseBase(StringView input)
    {
        if (input.getEncoding() == StringEncoding::Utf16)
            return parseBaseTemplate<StringIteratorUTF16, separator>(input);
        else
            return parseBaseTemplate<StringIteratorASCII, separator>(input);
    }

    template <typename StringIterator, char separator>
    static bool rootIsFollowedByOnlySeparators(const StringView input, const StringView root)
    {
        // TODO: This doesn't work with UTF16
        SC_RELEASE_ASSERT(input.getEncoding() != StringEncoding::Utf16);
        StringView remaining = input.sliceStartEnd<StringIterator>(root.sizeASCII(), input.sizeASCII());

        auto it = remaining.getIterator<StringIterator>();
        it.advanceUntilDifferentFrom(separator);
        return it.isAtEnd();
    }

    template <typename StringIterator, char separator>
    static StringView parseDirectoryTemplate(const StringView input, const StringView root)
    {
        auto       it       = input.getIterator<StringIterator>();
        const auto itBackup = it;
        it.setToEnd();
        if (it.reverseAdvanceUntilMatches(separator))
        {
            const StringView directory = StringView::fromIterators(itBackup, it);
            if (directory.isEmpty())
            {
                return root;
            }
            if (rootIsFollowedByOnlySeparators<StringIterator, separator>(input, root))
            {
                return input;
            }
            return directory;
        }
        return StringView();
    }

    template <char separator>
    static StringView parseDirectory(const StringView input, const StringView root)
    {
        if (input.getEncoding() == StringEncoding::Utf16)
            return parseDirectoryTemplate<StringIteratorUTF16, separator>(input, root);
        else
            return parseDirectoryTemplate<StringIteratorASCII, separator>(input, root);
    }

    template <typename StringIterator, char separator>
    static SC::StringView dirnameTemplate(StringView input)
    {
        StringView dirn;
        StringView base = basename<separator>(input, &dirn);
        if (dirn.isEmpty())
        {
            return "."_a8;
        }
        return dirn;
    }

    template <char separator>
    static SC::StringView dirname(StringView input)
    {
        if (input.getEncoding() == StringEncoding::Utf16)
            return dirnameTemplate<StringIteratorUTF16, separator>(input);
        else
            return dirnameTemplate<StringIteratorASCII, separator>(input);
    }

    template <typename StringIterator, char separator>
    static SC::StringView basenameTemplate(StringView input, StringView* dir = nullptr)
    {
        auto it = input.getIterator<StringIterator>();
        it.setToEnd();
        while (it.stepBackward() and it.match(separator)) {}
        auto itEnd = it;
        (void)itEnd.stepForward();
        if (it.reverseAdvanceUntilMatches(separator))
        {
            if (dir)
            {
                *dir = StringView::fromIteratorFromStart(it);
            }
            (void)it.stepForward();
            return StringView::fromIterators(it, itEnd);
        }
        return input;
    }

    template <char separator>
    static SC::StringView basename(StringView input, StringView* dir = nullptr)
    {
        if (input.getEncoding() == StringEncoding::Utf16)
            return basenameTemplate<StringIteratorUTF16, separator>(input, dir);
        else
            return basenameTemplate<StringIteratorASCII, separator>(input, dir);
    }

    template <typename StringIterator, char separator>
    static SC::StringView basenameTemplate(StringView input, StringView suffix)
    {
        StringView name = basename<separator>(input);
        if (name.endsWith(suffix))
        {
            return name.sliceStartEnd<StringIterator>(0, name.sizeInBytes() - suffix.sizeInBytes());
        }
        return name;
    }

    template <char separator>
    static SC::StringView basename(StringView input, StringView suffix)
    {
        if (input.getEncoding() == StringEncoding::Utf16)
            return basenameTemplate<StringIteratorUTF16, separator>(input, suffix);
        else
            return basenameTemplate<StringIteratorASCII, separator>(input, suffix);
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
        directory = Internal::parseDirectory<'\\'>(input, root);
        if (root.startsWith(directory) && root.endsWithChar('\\'))
        {
            directory = root;
        }
        base              = Internal::parseBase<'\\'>(input);
        endsWithSeparator = input.endsWithChar('\\');
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
        directory         = Internal::parseDirectory<'/'>(input, root);
        base              = Internal::parseBase<'/'>(input);
        endsWithSeparator = input.endsWithChar('/');
        return !(root.isEmpty() && directory.isEmpty());
    }
};

bool SC::Path::parseNameExtension(const StringView input, StringView& name, StringView& extension)
{
    StringIteratorASCII it       = input.getIterator<StringIteratorASCII>();
    StringIteratorASCII itBackup = it;
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
}

bool SC::Path::parse(StringView input, PathParsedView& pathView)
{
#if SC_PLATFORM_WINDOWS
    return pathView.parseWindows(input);
#else
    return pathView.parsePosix(input);
#endif
}

bool SC::PathParsedView::parseWindows(StringView input)
{
    type = TypeInvalid;
    if (!Path::Internal::parseWindows(input, root, directory, base, endsWithSeparator))
    {
        return false;
    }
    if (!base.isEmpty())
    {
        if (!Path::parseNameExtension(base, name, ext))
            return false;
    }
    type = TypeWindows;
    return true;
}

bool SC::PathParsedView::parsePosix(StringView input)
{
    type = TypeInvalid;
    if (!Path::Internal::parsePosix(input, root, directory, base, endsWithSeparator))
    {
        return false;
    }
    if (!base.isEmpty())
    {
        if (!Path::parseNameExtension(base, name, ext))
            return false;
    }
    type = TypePosix;
    return true;
}

SC::StringView SC::Path::dirname(StringView input) { return Internal::dirname<Separator>(input); }

SC::StringView SC::Path::basename(StringView input) { return Internal::basename<Separator>(input); }

SC::StringView SC::Path::basename(StringView input, StringView suffix)
{
    return Internal::basename<Separator>(input, suffix);
}

bool SC::Path::isAbsolute(StringView input)
{
#if SC_PLATFORM_WINDOWS
    return Windows::isAbsolute(input);
#else
    return Posix::isAbsolute(input);
#endif
}

SC::StringView SC::Path::Windows::dirname(StringView input) { return Internal::dirname<Separator>(input); }

SC::StringView SC::Path::Windows::basename(StringView input) { return Internal::basename<Separator>(input); }

SC::StringView SC::Path::Windows::basename(StringView input, StringView suffix)
{
    return Internal::basename<Separator>(input, suffix);
}

bool SC::Path::Windows::isAbsolute(StringView input)
{
    StringView root = Internal::parseWindowsRoot(input);
    return not root.isEmpty();
}

SC::StringView SC::Path::Posix::dirname(StringView input) { return Internal::dirname<Separator>(input); }

SC::StringView SC::Path::Posix::basename(StringView input) { return Internal::basename<Separator>(input); }

SC::StringView SC::Path::Posix::basename(StringView input, StringView suffix)
{
    return Internal::basename<Separator>(input, suffix);
}

bool SC::Path::Posix::isAbsolute(StringView input) { return input.startsWithChar('/'); }

bool SC::Path::join(String& output, Span<const StringView> inputs)
{
    static constexpr auto localSep = Path::Separator;
    // TODO: This will bring unnecessary conversions...
    StringView    sep(&localSep, sizeof(localSep), false, StringEncoding::Ascii);
    StringBuilder sb(output);
    const size_t  numElements = inputs.sizeInElements();
    for (size_t idx = 0; idx < numElements; ++idx)
    {
        SC_TRY_IF(sb.append(inputs.data()[idx]));
        if (idx + 1 != numElements)
        {
            SC_TRY_IF(sb.append(sep));
        }
    }
    return true;
}
