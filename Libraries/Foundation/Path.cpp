// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Path.h"

struct SC::Path::Internal
{
    // Parses a windows drive (Example C:\\)
    // TODO: Path: Add support for UNC paths
    static StringView parseWindowsRoot(StringView input)
    {
        auto it       = input.getIterator<StringIteratorASCII>();
        auto itBackup = it;
        if (!it.isEmpty())
        {
            const char_t letter = *it.getIt();
            if ((letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z'))
            {
                if (it.skipNext() && it.matches(':') && it.skipNext() && it.matches('\\'))
                {
                    (void)it.skipNext();
                    return StringView::fromIterators(itBackup, it);
                }
            }
            // Try parasing network form
            it = itBackup;
            if (it.matches('\\') && it.skipNext() && it.matches('\\'))
            {
                (void)it.skipNext();
                auto itCheckpoint = it;
                // Try parsing long path form that includes ? and another backslash
                if (it.matches('?') && it.skipNext() && it.matches('\\'))
                {
                    (void)it.skipNext();
                    return StringView::fromIterators(itBackup, it);
                }
                return StringView::fromIterators(itBackup, itCheckpoint);
            }
        }
        return StringView();
    }

    // Parses a Posix root
    static StringView parsePosixRoot(StringView input)
    {
        if (input.startsWith('/'))
        {
            StringView root = input;
            (void)root.setSizeInBytesWithoutTerminator(1);
            // we want to return a string view pointing at the "/" char of the input string
            return root;
        }
        return StringView();
    }

    template <char separator>
    static StringView parseBase(StringView input)
    {
        // Parse the base
        auto it = input.getIterator<StringIteratorASCII>();
        it.rewindToEnd();
        (void)it.reverseUntilMatches(separator);
        (void)it.skipNext();
        return StringView::fromIteratorUntilEnd(it);
    }

    template <char separator>
    static bool rootIsFollowedByOnlySeparators(const StringView input, const StringView root)
    {
        StringView remaining = input.sliceStartEnd<StringIteratorASCII>(root.sizeASCII(), input.sizeASCII());
        auto       it        = remaining.getIterator<StringIteratorASCII>();
        bool       endsWithAllSeparators = true;
        while (!it.isEmpty())
        {
            endsWithAllSeparators &= it.matches(separator);
            (void)it.skipNext();
        }
        return endsWithAllSeparators;
    }

    template <char separator>
    static StringView parseDirectory(const StringView input, const StringView root)
    {
        auto       it       = input.getIterator<StringIteratorASCII>();
        const auto itBackup = it;
        it.rewindToEnd();
        if (it.reverseUntilMatches(separator))
        {
            const StringView directory = StringView::fromIterators(itBackup, it);
            if (directory.isEmpty())
            {
                return root;
            }
            if (rootIsFollowedByOnlySeparators<separator>(input, root))
            {
                return input;
            }
            return directory;
        }
        return StringView();
    }

    template <char separator>
    static SC::StringView dirname(StringView input)
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
    static SC::StringView basename(StringView input, StringView* dir = nullptr)
    {
        auto it = input.getIterator<StringIteratorASCII>();
        it.rewindToEnd();
        while (it.skipPrev() and it.matches(separator)) {}
        auto itEnd = it;
        (void)itEnd.skipNext();
        if (it.reverseUntilMatches(separator))
        {
            (void)it.skipNext();
            if (dir)
            {
                (void)it.skipPrev();
                *dir = StringView::fromIteratorFromStart(it);
            }
            return StringView::fromIterators(it, itEnd);
        }
        return input;
    }

    template <char separator>
    static SC::StringView basename(StringView input, StringView suffix)
    {
        StringView name = basename<separator>(input);
        if (name.endsWith(suffix))
        {
            return name.sliceStartEnd<StringIteratorASCII>(0, name.sizeInBytes() - suffix.sizeInBytes());
        }
        return name;
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
        if (root.startsWith(directory) && root.endsWith('\\'))
        {
            directory = root;
        }
        base              = Internal::parseBase<'\\'>(input);
        endsWithSeparator = input.endsWith('\\');
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
        endsWithSeparator = input.endsWith('/');
        return !(root.isEmpty() && directory.isEmpty());
    }
};

bool SC::Path::parseNameExtension(const StringView input, StringView& name, StringView& extension)
{
    StringIteratorASCII it       = input.getIterator<StringIteratorASCII>();
    StringIteratorASCII itBackup = it;
    // Try searching for a '.' but if it's not found then just set the entire content
    // to be the name.
    it.rewindToEnd();
    if (it.reverseUntilMatches('.'))
    {
        name = StringView::fromIterators(itBackup, it);   // from 'name.ext' keep 'name'
        (void)it.skipNext();                              // skip the .
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

bool SC::Path::Posix::isAbsolute(StringView input) { return input.startsWith('/'); }
