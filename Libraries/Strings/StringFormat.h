// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Internal/IGrowableBuffer.h"
#include "../Strings/StringView.h"

namespace SC
{
struct Console;

struct StringFormatOutput;
template <typename T>
struct StringFormatterFor
{
    static bool format(StringFormatOutput& data, const StringSpan specifier, const T& value)
    {
        return StringFormatterFor<decltype(value.view())>::format(data, specifier, value.view());
    };
};

//! @addtogroup group_strings
//! @{
/// @brief Allows pushing results of StringFormat to a buffer or to the console
struct SC_COMPILER_EXPORT StringFormatOutput
{
    StringFormatOutput(StringEncoding encoding, IGrowableBuffer& buffer) : encoding(encoding), growableBuffer(&buffer)
    {}

    /// @brief Constructs a StringFormatOutput object pushing to a console
    /// @param encoding The given encoding
    /// @param destination The destination console
    StringFormatOutput(StringEncoding encoding, Console& destination);

    /// @brief Appends the StringView (eventually converting it) to destination buffer
    /// @param text The StringView to be appended to buffer or console
    /// @return `true` if conversion succeeded
    [[nodiscard]] bool append(StringView text);

    /// @brief Method to be called when format begins, so that it can be rolled back on failure
    void onFormatBegin();

    /// @brief Method to be called when format fails (will rollback buffer to length before `onFormatBegin`)
    void onFormatFailed();

    /// @brief Method to be called when format succeeds
    /// @return `true` if IGrowableBuffer is still valid
    [[nodiscard]] bool onFormatSucceeded();

  private:
    IGrowableBuffer* growableBuffer = nullptr;
    StringEncoding   encoding;

    Console* console    = nullptr;
    size_t   backupSize = 0;
};

/// @brief Formats String with a simple DSL embedded in the format string
///
/// This is a small implementation to format using a minimal string based DSL, but good enough for simple usages.
/// It uses the same `{}` syntax and supports positional arguments. @n
/// `StringFormat::format(output, "{1} {0}", "World", "Hello")` is formatted as `"Hello World"`. @n
///  Inside the `{}` after a colon (`:`) a specification string can be used to indicate how to format
/// the given value. As the backend for actual number to string formatting is `snprintf`, such specification strings are
/// the same as what would be given to snprintf. For example passing `"{:02}"` is transformed to `"%.02f"` when passed
/// to snprintf. @n
/// `{` is escaped if found near to another `{`. In other words `format("{{")` will print a single `{`.
///
/// Example:
/// @code{.cpp}
/// String buffer(StringEncoding::Ascii);
/// SC_TEST_EXPECT(StringBuilder::format(buffer, "{1}_{0}_{1}", 1, 0));
/// SC_TEST_EXPECT(buffer == "0_1_0");
/// SC_TEST_EXPECT(StringBuilder::format(buffer, "{0:.2}_{1}_{0:.4}", 1.2222, "salve"));
/// SC_TEST_EXPECT(buffer == "1.22_salve_1.2222");
/// @endcode
/// @note It's not convenient to use SC::StringFormat directly, as you should probably use SC::StringBuilder
/// @tparam RangeIterator Type of the specific StringIterator used
template <typename RangeIterator>
struct StringFormat
{
    /// @brief Formats fmt StringView using simple DSL where `{}` are replaced with args
    /// @tparam Types Types of the arguments being formatted
    /// @param data Destination abstraction (buffer or console)
    /// @param fmt The format string to be used
    /// @param args Actual arguments being formatted
    /// @return `true` if format succeeded
    template <typename... Types>
    [[nodiscard]] static bool format(StringFormatOutput& data, StringView fmt, Types&&... args);

  private:
    struct Implementation;
};
//! @}

} // namespace SC

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
template <typename RangeIterator>
struct SC::StringFormat<RangeIterator>::Implementation
{
    template <int Total, int N, typename T, typename... Rest>
    static bool formatArgument(StringFormatOutput& data, StringView specifier, int position, T&& arg, Rest&&... rest)
    {
        if (position == Total - N)
        {
            using First = typename TypeTraits::RemoveConst<typename TypeTraits::RemoveReference<T>::type>::type;
            return StringFormatterFor<First>::format(data, specifier, arg);
        }
        else
        {
            return formatArgument<Total, N - 1>(data, specifier, position, forward<Rest>(rest)...);
        }
    }

    template <int Total, int N, typename... Args>
    static typename SC::TypeTraits::EnableIf<sizeof...(Args) == 0, bool>::type formatArgument(StringFormatOutput&,
                                                                                              StringView, int, Args...)
    {
        return false;
    }

    template <typename... Types>
    static bool parsePosition(StringFormatOutput& data, RangeIterator& it, int32_t& parsedPosition, Types&&... args)
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
                if (not positionString.parseInt32(parsedPosition))
                {
                    return false;
                }
            }
            constexpr auto maxArgs = sizeof...(args);
            return formatArgument<maxArgs, maxArgs>(data, specifierString, parsedPosition, forward<Types>(args)...);
        }
        return false;
    }

    template <typename... Types>
    static bool executeFormat(StringFormatOutput& data, RangeIterator it, Types&&... args)
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
                        if (not data.append(StringView::fromIterators(start, it)))
                            return false;
                        (void)it.stepForward(); // we don't want to insert the additional '{' or '}' needed for escaping
                        start = it;
                    }
                else if (matchedChar == '{') // it's a '{' not followed by itself, so let's parse specifier
                {
                    if (not data.append(StringView::fromIterators(start, it))) // write everything before '{
                        return false;
                    // try parse '}' and eventually format
                    int32_t parsedPosition = position;
                    if (not parsePosition(data, it, parsedPosition, forward<Types>(args)...))
                        return false;
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
                if (not data.append(StringView::fromIterators(start, it))) // write everything before '{
                    return false;
                return maxPosition == static_cast<int32_t>(sizeof...(args)); // check right number of args
            }
        }
    }
};

template <typename RangeIterator>
template <typename... Types>
bool SC::StringFormat<RangeIterator>::format(StringFormatOutput& data, StringView fmt, Types&&... args)
{
    SC_TRY(fmt.getEncoding() != StringEncoding::Utf16); // UTF16 format strings are not supported
    data.onFormatBegin();
    if (Implementation::executeFormat(data, fmt.getIterator<RangeIterator>(), forward<Types>(args)...))
        SC_LANGUAGE_LIKELY { return data.onFormatSucceeded(); }
    else
    {
        data.onFormatFailed();
        return false;
    }
}

namespace SC
{
// clang-format off
template <> struct SC_COMPILER_EXPORT StringFormatterFor<float>        {static bool format(StringFormatOutput&, const StringSpan, const float);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<double>       {static bool format(StringFormatOutput&, const StringSpan, const double);};
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#if SC_PLATFORM_64_BIT == 0
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::ssize_t>  {static bool format(StringFormatOutput&, const StringSpan, const SC::ssize_t);};
#endif
#else
#if !SC_PLATFORM_LINUX
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::size_t>   {static bool format(StringFormatOutput&, const StringSpan, const SC::size_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::ssize_t>  {static bool format(StringFormatOutput&, const StringSpan, const SC::ssize_t);};
#endif
#endif
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::int64_t>  {static bool format(StringFormatOutput&, const StringSpan, const SC::int64_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::uint64_t> {static bool format(StringFormatOutput&, const StringSpan, const SC::uint64_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::int32_t>  {static bool format(StringFormatOutput&, const StringSpan, const SC::int32_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::uint32_t> {static bool format(StringFormatOutput&, const StringSpan, const SC::uint32_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::int16_t>  {static bool format(StringFormatOutput&, const StringSpan, const SC::int16_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::uint16_t> {static bool format(StringFormatOutput&, const StringSpan, const SC::uint16_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::int8_t>   {static bool format(StringFormatOutput&, const StringSpan, const SC::int8_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<SC::uint8_t>  {static bool format(StringFormatOutput&, const StringSpan, const SC::uint8_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<char>         {static bool format(StringFormatOutput&, const StringSpan, const char);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<bool>         {static bool format(StringFormatOutput&, const StringSpan, const bool);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<StringView>   {static bool format(StringFormatOutput&, const StringSpan, const StringView);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<const char*>  {static bool format(StringFormatOutput&, const StringSpan, const char*);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<const void*>  {static bool format(StringFormatOutput&, const StringSpan, const void*);};
#if SC_PLATFORM_WINDOWS
template <> struct SC_COMPILER_EXPORT StringFormatterFor<wchar_t>        {static bool format(StringFormatOutput&, const StringSpan, const wchar_t);};
template <> struct SC_COMPILER_EXPORT StringFormatterFor<const wchar_t*> {static bool format(StringFormatOutput&, const StringSpan, const wchar_t*);};
#endif
template <> struct SC_COMPILER_EXPORT StringFormatterFor<StringSpan> {static bool format(StringFormatOutput&, const StringSpan, const StringSpan);};
struct StringPath;
template <> struct SC_COMPILER_EXPORT StringFormatterFor<StringPath> {static bool format(StringFormatOutput&, const StringSpan, const StringPath&);};

// clang-format on

template <int N>
struct StringFormatterFor<char[N]>
{
    static bool format(StringFormatOutput& data, const StringView specifier, const char* str)
    {
        const StringView sv({str, N - 1}, true, StringEncoding::Ascii);
        return StringFormatterFor<StringView>::format(data, specifier, sv);
    }
};

} // namespace SC
