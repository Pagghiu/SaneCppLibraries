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
            const char_t letter = *it.getStart();
            if ((letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z'))
            {
                if (it.skipNext() && it.matches(':') && it.skipNext() && it.matches('\\'))
                {
                    (void)it.skipNext();
                    return itBackup.viewUntil(it);
                }
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
        (void)it.reverseUntilMatches(separator);
        (void)it.skipNext();
        return it.viewUntilEnd();
    }

    template <char separator>
    static bool rootIsFollowedByOnlySeparators(const StringView input,const StringView root)
    {
        StringView remaining = input.functions<StringIteratorASCII>().fromTo(root.sizeInBytesWithoutTerminator(), input.sizeInBytesWithoutTerminator());
        auto it = remaining.getIterator<StringIteratorASCII>();
        bool endsWithAllSeparators = true;
        while(!it.isEmpty())
        {
            endsWithAllSeparators &= it.matches(separator);
            (void)it.skipNext();
        }
        return endsWithAllSeparators;
    }
    
    template <char separator>
    static StringView parseDirectoryPosix(const StringView input, const StringView root)
    {
        auto       it       = input.getIterator<StringIteratorASCII>();
        const auto itBackup = it;
        if (it.reverseUntilMatches(separator))
        {
            const StringView directory = itBackup.viewUntil(it);
            if(directory.isEmpty())
            {
                return root;
            }
            if(rootIsFollowedByOnlySeparators<separator>(input, root))
            {
                return input;
            }
            
            return directory;
        }
        return StringView();
    }
    
    template <char separator>
    static StringView parseDirectoryWindows(const StringView input, const StringView root)
    {
        auto       it       = input.getIterator<StringIteratorASCII>();
        const auto itBackup = it;
        if (it.reverseUntilMatches(separator))
        {
            const StringView directory = itBackup.viewUntil(it);
            if(directory.isEmpty())
            {
                return root;
            }
            if(rootIsFollowedByOnlySeparators<separator>(input, root))
            {
                return input;
            }
                        
            return directory;
        }
        return StringView();
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
        root              = Internal::parseWindowsRoot(input);
        directory         = Internal::parseDirectoryWindows<'\\'>(input, root);
        if(root.startsWith(directory) && root.endsWith('\\'))
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
        directory         = Internal::parseDirectoryPosix<'/'>(input, root);
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
    if (it.reverseUntilMatches('.'))
    {
        name = itBackup.viewUntil(it); // from 'name.ext' keep 'name'
        (void)it.skipNext();           // skip the .
        extension = it.viewUntilEnd(); // from 'name.ext' keep 'ext'
    }
    else
    {
        name      = input;
        extension = StringView();
    }
    return !(name.isEmpty() && extension.isEmpty());
}

SC::Error SC::Path::parse(StringView input, PathView& pathView)
{
#if SC_PLATFORM_WINDOWS
    if (pathView.parseWindows(input))
    {
        return {};
    }
#else
    if (pathView.parsePosix(input))
    {
        return {};
    }
#endif
    return "Cannot Parse path"_sv;
}

bool SC::PathView::parseWindows(StringView input)
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

bool SC::PathView::parsePosix(StringView input)
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
