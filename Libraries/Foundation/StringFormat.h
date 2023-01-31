// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Result.h"
#include "SmallVector.h"
#include "StringIterator.h"
#include "StringView.h"
#include "Vector.h"

namespace SC
{

template <typename T>
struct StringFormatterFor;

struct StringFormatOutput
{
    SmallVector<char, 512> buffer;
    SmallVector<char, 512> data;

    bool           writeToStdout = false;
    StringEncoding encoding      = StringEncoding::Ascii;

    bool write(StringView text);
    void onFormatBegin();
    bool onFormatSucceded();
    void onFormatFailed();

  private:
    size_t backupSize = 0;
};

// clang-format off
template <> struct StringFormatterFor<float>        {static bool format(StringFormatOutput&, StringIteratorASCII, const float);};
template <> struct StringFormatterFor<double>       {static bool format(StringFormatOutput&, StringIteratorASCII, const double);};
#if SC_MSVC
#else
template <> struct StringFormatterFor<SC::size_t>   {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::size_t);};
template <> struct StringFormatterFor<SC::ssize_t>  {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::ssize_t);};
#endif
template <> struct StringFormatterFor<SC::int64_t>  {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::int64_t);};
template <> struct StringFormatterFor<SC::uint64_t> {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::uint64_t);};
template <> struct StringFormatterFor<SC::int32_t>  {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::int32_t);};
template <> struct StringFormatterFor<SC::uint32_t> {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::uint32_t);};
template <> struct StringFormatterFor<SC::int16_t>  {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::int16_t);};
template <> struct StringFormatterFor<SC::uint16_t> {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::uint16_t);};
template <> struct StringFormatterFor<SC::int8_t>   {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::int8_t);};
template <> struct StringFormatterFor<SC::uint8_t>  {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::uint8_t);};
template <> struct StringFormatterFor<SC::char_t>   {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::char_t);};
template <> struct StringFormatterFor<StringView>   {static bool format(StringFormatOutput&, StringIteratorASCII, const StringView);};
template <> struct StringFormatterFor<const SC::char_t*> {static bool format(StringFormatOutput&, StringIteratorASCII, const SC::char_t*);};
// clang-format on

template <typename RangeIterator>
struct StringFormat
{
    template <typename... Types>
    [[nodiscard]] static bool format(StringFormatOutput& data, StringView fmt, Types... args)
    {
        data.onFormatBegin();
        if (recursiveFormat(data, fmt.getEncoding(), fmt.getIterator<RangeIterator>(), args...))
            SC_LIKELY { return data.onFormatSucceded(); }
        else
        {
            data.onFormatFailed();
            return false;
        }
    }

  private:
    template <typename First, typename... Types>
    [[nodiscard]] static bool formatArgumentAndRecurse(StringFormatOutput& data, StringEncoding encoding,
                                                       RangeIterator it, First first, Types... args)
    {
        // We have an already matched '{' here
        const auto startOfSpecifier = it;
        if (it.advanceUntilMatchesAfter('}'))
        {
            auto specifier = startOfSpecifier.untilBefore(it);
            (void)specifier.advanceUntilMatchesAfter(':'); // optional
            const bool formattedSuccessfully = StringFormatterFor<First>::format(data, specifier, first);
            return formattedSuccessfully && recursiveFormat(data, encoding, it, args...);
        }
        return false;
    }

    [[nodiscard]] static bool formatArgumentAndRecurse(StringFormatOutput& data, StringEncoding encoding,
                                                       RangeIterator it)
    {
        return false;
    }

    template <typename... Types>
    [[nodiscard]] static bool recursiveFormat(StringFormatOutput& data, StringEncoding encoding, RangeIterator it,
                                              Types... args)
    {
        auto   startingPoint = it;
        char_t matchedChar;
        while (true)
        {
            if (it.advanceUntilMatches('{', '}', &matchedChar)) // match start or end of specifier
            {
                if (it.isFollowedBy(matchedChar))
                    SC_UNLIKELY // if it's the same matched, let's escape it
                    {
                        (void)it.skipNext(); // we want to make sure we insert the escaped '{' or '}'
                        SC_TRY_IF(data.write(StringView(startingPoint.sliceUntil(it), false, encoding)));
                        (void)it.skipNext(); // we don't want to insert the additional '{' or '}' needed for escaping
                        startingPoint = it;
                        continue; // or return recursiveFormat(data, it, args...); // recurse as alternative to
                                  // while(true)
                    }
                else if (matchedChar == '{') // it's a '{' not followed by itself, so let's parse specifier
                {
                    SC_TRY_IF(data.write(
                        StringView(startingPoint.sliceUntil(it), false, encoding))); // write everything before '{'
                    return formatArgumentAndRecurse(data, encoding, it, args...); // try parse '}' and eventually format
                }
                // arriving here means end of string with as single, unescaped '}'
                return false;
            }
            if (sizeof...(Types) == 0)
            {
                // All arguments have been eaten, so let's append all remaining chars
                return data.write(StringView(startingPoint.sliceUntilEnd(), false, encoding));
            }
            return false; // we have more arguments than specifiers
        }
    }
};

} // namespace SC
