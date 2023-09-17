// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Objects/Result.h"
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
#if SC_MSVC || SC_CLANG_CL
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
template <> struct StringFormatterFor<SC::char_t>   {static bool format(StringFormatOutput&, const StringView, const SC::char_t);};
template <> struct StringFormatterFor<bool>         {static bool format(StringFormatOutput&, const StringView, const bool);};
template <> struct StringFormatterFor<wchar_t>      {static bool format(StringFormatOutput&, const StringView, const wchar_t);};
template <> struct StringFormatterFor<StringView>   {static bool format(StringFormatOutput&, const StringView, const StringView);};
template <> struct StringFormatterFor<const SC::char_t*> {static bool format(StringFormatOutput&, const StringView, const SC::char_t*);};
template <> struct StringFormatterFor<const wchar_t*> {static bool format(StringFormatOutput&, const StringView, const wchar_t*);};
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
        const auto startOfSpecifier = it;
        if (it.advanceUntilMatches('}')) // We have an already matched '{' when arriving here
        {
            auto specifier = startOfSpecifier.sliceFromStartUntil(it);
            if (specifier.advanceUntilMatches(':'))
                (void)specifier.stepForward();
            (void)it.stepForward();
            const bool formattedSuccessfully =
                StringFormatterFor<First>::format(data, StringView::fromIteratorUntilEnd(specifier), first);
            return formattedSuccessfully && recursiveFormat(data, encoding, it, args...);
        }
        return false;
    }

    [[nodiscard]] static bool formatArgumentAndRecurse(StringFormatOutput& data, StringEncoding encoding,
                                                       RangeIterator it)
    {
        SC_UNUSED(data);
        SC_UNUSED(encoding);
        SC_UNUSED(it);
        return false;
    }

    template <typename... Types>
    [[nodiscard]] static bool recursiveFormat(StringFormatOutput& data, StringEncoding encoding, RangeIterator it,
                                              Types... args)
    {
        auto                              start = it;
        typename RangeIterator::CodePoint matchedChar;
        while (true)
        {
            if (it.advanceUntilMatchesAny({'{', '}'}, matchedChar)) // match start or end of specifier
            {
                if (it.isFollowedBy(matchedChar))
                    SC_UNLIKELY // if it's the same matched, let's escape it
                    {
                        (void)it.stepForward(); // we want to make sure we insert the escaped '{' or '}'
                        // SC_TRY_IF(data.write(StringView(startingPoint.sliceUntil(it), false, encoding)));
                        SC_TRY_IF(data.write(StringView::fromIterators(start, it)));
                        (void)it.stepForward(); // we don't want to insert the additional '{' or '}' needed for escaping
                        start = it;
                        continue; // or return recursiveFormat(data, it, args...); as alternative to while(true)
                    }
                else if (matchedChar == '{') // it's a '{' not followed by itself, so let's parse specifier
                {
                    SC_TRY_IF(data.write(StringView::fromIterators(start, it)));  // write everything before '{'
                    return formatArgumentAndRecurse(data, encoding, it, args...); // try parse '}' and eventually format
                }
                return false; // arriving here means end of string with as single, unescaped '}'
            }
            return writeIfLastArg<sizeof...(Types) == 0>(data, start); // avoids 'conditional expression is constant'
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
