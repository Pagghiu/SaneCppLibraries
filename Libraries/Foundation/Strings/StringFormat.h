// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Language/Result.h"
#include "StringIterator.h"
#include "StringView.h"

namespace SC
{
template <typename T>
struct Vector;
struct Console;
template <typename T>
struct StringFormatterFor;

struct StringFormatOutput
{
    StringFormatOutput(StringEncoding encoding) : encoding(encoding) {}

    [[nodiscard]] bool write(StringView text);
    [[nodiscard]] bool onFormatSucceded();

    void onFormatBegin();
    void onFormatFailed();

    void redirectToBuffer(Vector<char>& destination);
    void redirectToConsole(Console& newConsole);

    StringEncoding getEncoding() const { return encoding; }

  private:
    Vector<char>*  data    = nullptr;
    Console*       console = nullptr;
    StringEncoding encoding;
    size_t         backupSize = 0;
};

// clang-format off
template <> struct StringFormatterFor<float>        {static bool format(StringFormatOutput&, const StringView, const float);};
template <> struct StringFormatterFor<double>       {static bool format(StringFormatOutput&, const StringView, const double);};
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#if SC_PLATFORM_64_BIT == 0
template <> struct StringFormatterFor<SC::ssize_t> { static bool format(StringFormatOutput&, const StringView, const SC::ssize_t); };
#endif
#else
template <> struct StringFormatterFor<SC::size_t>   {static bool format(StringFormatOutput&, const StringView, const SC::size_t);};
template <> struct StringFormatterFor<SC::ssize_t>  {static bool format(StringFormatOutput&, const StringView, const SC::ssize_t);};
#endif
template <> struct StringFormatterFor<SC::int64_t>  {static bool format(StringFormatOutput&, const StringView, const SC::int64_t);};
template <> struct StringFormatterFor<SC::uint64_t> {static bool format(StringFormatOutput&, const StringView, const SC::uint64_t);};
template <> struct StringFormatterFor<SC::int32_t>  {static bool format(StringFormatOutput&, const StringView, const SC::int32_t);};
template <> struct StringFormatterFor<SC::uint32_t> {static bool format(StringFormatOutput&, const StringView, const SC::uint32_t);};
template <> struct StringFormatterFor<SC::int16_t>  {static bool format(StringFormatOutput&, const StringView, const SC::int16_t);};
template <> struct StringFormatterFor<SC::uint16_t> {static bool format(StringFormatOutput&, const StringView, const SC::uint16_t);};
template <> struct StringFormatterFor<SC::int8_t>   {static bool format(StringFormatOutput&, const StringView, const SC::int8_t);};
template <> struct StringFormatterFor<SC::uint8_t>  {static bool format(StringFormatOutput&, const StringView, const SC::uint8_t);};
template <> struct StringFormatterFor<char>   {static bool format(StringFormatOutput&, const StringView, const char);};
template <> struct StringFormatterFor<bool>         {static bool format(StringFormatOutput&, const StringView, const bool);};
template <> struct StringFormatterFor<StringView>   {static bool format(StringFormatOutput&, const StringView, const StringView);};
template <> struct StringFormatterFor<const char*> {static bool format(StringFormatOutput&, const StringView, const char*);};
#if SC_PLATFORM_WINDOWS
template <> struct StringFormatterFor<wchar_t>      {static bool format(StringFormatOutput&, const StringView, const wchar_t);};
template <> struct StringFormatterFor<const wchar_t*> {static bool format(StringFormatOutput&, const StringView, const wchar_t*);};
#endif
// clang-format on
template <int N>
struct StringFormatterFor<char[N]>
{
    static bool format(StringFormatOutput& data, const StringView specifier, const char* str)
    {
        return StringFormatterFor<StringView>::format(data, specifier,
                                                      StringView(str, N - 1, true, StringEncoding::Ascii));
    }
};

template <typename RangeIterator>
struct StringFormat
{
    template <typename... Types>
    [[nodiscard]] static bool format(StringFormatOutput& data, StringView fmt, Types&&... args)
    {
        data.onFormatBegin();
        if (executeFormat(data, fmt.getIterator<RangeIterator>(), forward<Types>(args)...))
            SC_LANGUAGE_LIKELY { return data.onFormatSucceded(); }
        else
        {
            data.onFormatFailed();
            return false;
        }
    }

  private:
    template <int Total, int N, typename T, typename... Rest>
    static bool formatArgument(StringFormatOutput& data, StringView specifier, int position, T&& arg, Rest&&... rest)
    {
        if (position == Total - N)
        {
            using First = typename RemoveConst<typename RemoveReference<T>::type>::type;
            return StringFormatterFor<First>::format(data, specifier, arg);
        }
        else
        {
            return formatArgument<Total, N - 1>(data, specifier, position, forward<Rest>(rest)...);
        }
    }

    template <int Total, int N, typename... Args>
    static typename EnableIf<sizeof...(Args) == 0, bool>::type formatArgument(StringFormatOutput&, StringView, int,
                                                                              Args...)
    {
        return false;
    }

    template <typename... Types>
    [[nodiscard]] static bool parsePosition(StringFormatOutput& data, RangeIterator& it, int32_t& parsedPosition,
                                            Types&&... args)
    {
        const auto startOfSpecifier = it;
        if (it.advanceUntilMatches('}')) // We have an already matched '{' when arriving here
        {
            auto specifier         = startOfSpecifier.sliceFromStartUntil(it);
            auto specifierPosition = specifier;
            if (specifier.advanceUntilMatches(':'))
            {
                specifierPosition = startOfSpecifier.sliceFromStartUntil(specifier);
                (void)specifier.stepForward(); // eat '{'
            }
            (void)specifierPosition.stepForward(); // eat '{'
            (void)it.stepForward();                // eat '}'
            const StringView positionString  = StringView::fromIteratorUntilEnd(specifierPosition);
            const StringView specifierString = StringView::fromIteratorUntilEnd(specifier);
            if (not positionString.isEmpty())
            {
                SC_TRY(positionString.parseInt32(parsedPosition));
            }
            constexpr auto maxArgs = sizeof...(args);
            return formatArgument<maxArgs, maxArgs>(data, specifierString, parsedPosition, forward<Types>(args)...);
        }
        return false;
    }

    template <typename... Types>
    [[nodiscard]] static bool executeFormat(StringFormatOutput& data, RangeIterator it, Types&&... args)
    {
        StringCodePoint matchedChar;

        auto    start       = it;
        int32_t position    = 0;
        int32_t maxPosition = 0;
        while (true)
        {
            if (it.advanceUntilMatchesAny({'{', '}'}, matchedChar)) // match start or end of specifier
            {
                if (it.isFollowedBy(matchedChar))
                    SC_LANGUAGE_UNLIKELY // if it's the same matched, let's escape it
                    {
                        (void)it.stepForward(); // we want to make sure we insert the escaped '{' or '}'
                        SC_TRY(data.write(StringView::fromIterators(start, it)));
                        (void)it.stepForward(); // we don't want to insert the additional '{' or '}' needed for escaping
                        start = it;
                    }
                else if (matchedChar == '{') // it's a '{' not followed by itself, so let's parse specifier
                {
                    SC_TRY(data.write(StringView::fromIterators(start, it))); // write everything before '{'
                    // try parse '}' and eventually format
                    int32_t parsedPosition = position;
                    SC_TRY(parsePosition(data, it, parsedPosition, forward<Types>(args)...));
                    start = it;
                    position += 1;
                    maxPosition = max(maxPosition, parsedPosition + 1);
                }
                else
                {
                    return false; // arriving here means end of string with as single, unescaped '}'
                }
            }
            else
            {
                SC_TRY(data.write(StringView::fromIterators(start, it)));    // write everything before '{'
                return maxPosition == static_cast<int32_t>(sizeof...(args)); // check right number of args
            }
        }
    }

    template <bool sizeofTypesIsZero>
    static typename EnableIf<sizeofTypesIsZero == true, bool>::type // sizeof...(Types) == 0
    writeIfLastArg(StringFormatOutput& data, const RangeIterator& startingPoint)
    {
        return data.write(StringView::fromIteratorUntilEnd(startingPoint));
    }

    template <bool sizeofTypesIsZero>
    static typename EnableIf<sizeofTypesIsZero == false, bool>::type // sizeof...(Types) > 0
    writeIfLastArg(StringFormatOutput&, const RangeIterator&)
    {
        return false; // we have more arguments than specifiers
    }
};

} // namespace SC
