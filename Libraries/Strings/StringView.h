// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Span.h"
#include "../Strings/StringIterator.h"

namespace SC
{
struct SC_COMPILER_EXPORT StringView;
struct SC_COMPILER_EXPORT StringViewTokenizer;
struct SC_COMPILER_EXPORT StringAlgorithms;

} // namespace SC

//! @defgroup group_strings Strings
//! @copybrief library_strings (see @ref library_strings for more details)

//! @addtogroup group_strings
//! @{

/// @brief Non-owning view over a range of characters with UTF Encoding.
///
/// It additional also holds the SC::StringEncoding information (`ASCII`, `UTF8` or `UTF16`).
/// During construction the encoding information and the null-termination state must be specified.
/// All methods are const because it's not possible to modify a string with it.
/// @n
/// Example (Construct)
/// @code{.cpp}
/// StringView s("asd");
/// SC_ASSERT_RELEASE(s.sizeInBytes() == 3);
/// SC_ASSERT_RELEASE(s.isNullTerminated());
/// @endcode
/// Example (Construct from null terminated string)
/// @code{.cpp}
/// const char* someString = "asdf";
/// // construct only "asd", not null terminated (as there is 'f' after 'd')
/// StringView s({someString, strlen(asd) - 1}, false, StringEncoding::Ascii);
/// SC_ASSERT_RELEASE(s.sizeInBytes() == 3);
/// SC_ASSERT_RELEASE(not s.isNullTerminated());
/// //
/// // ... or
/// StringView s2 = StringView::fromNullTerminated(s, StringEncoding::Ascii); // s2 == "asdf"
/// @endcode

struct SC::StringView : public StringSpan
{
    StringView() : StringSpan() {}

    using StringSpan::StringSpan;

    constexpr StringView(StringSpan ssv) : StringSpan(ssv) {}

    static constexpr StringView fromNullTerminated(const char* text, StringEncoding encoding)
    {
        return StringSpan::fromNullTerminated(text, encoding);
    }

#if SC_PLATFORM_WINDOWS
    static constexpr StringView fromNullTerminated(const wchar_t* text, StringEncoding encoding)
    {
        return StringSpan::fromNullTerminated(text, encoding);
    }
#endif

    /// @brief Directly access the memory of this null terminated-StringView.
    /// @return Pointer to start of StringView memory.
    /// On Windows return type will be `wchar_t`.
    /// On other platforms return type will be `char`.
    /// @warning This method will assert that the string is null terminated.
    ///          On Windows this will assert that encoding is UTF16.
    ///          On other platforms this will assert that encoding is UTF8 or ASCII.
    auto getNullTerminatedNative() const;

    /// @brief Obtain a `const uint8_t` Span from this StringView
    /// @return Span representing this StringView
    Span<const uint8_t> toBytesSpan() const SC_LANGUAGE_LIFETIME_BOUND
    {
        return Span<const uint8_t>::reinterpret_bytes(text, textSizeInBytes);
    }

    /// @brief Result of ordering comparison done by StringView::compare
    enum class Comparison
    {
        Smaller = -1, ////< Current string is smaller than the other
        Equals  = 0,  ////< Current string is equal to the other
        Bigger  = 1   ////< Current string is bigger than the other
    };

    /// @brief Ordering comparison between non-normalized StringView (operates on code points, not on utf graphemes)
    /// @param other The string being compared to current one
    /// @return Result of the comparison (smaller, equals or bigger)
    ///
    /// Example:
    /// @code{.cpp}
    /// // àèìòù (1 UTF16-LE sequence, 2 UTF8 sequence)
    /// SC_ASSERT_RELEASE("\xc3\xa0\xc3\xa8\xc3\xac\xc3\xb2\xc3\xb9"_u8.compare(
    ///                     "\xe0\x0\xe8\x0\xec\x0\xf2\x0\xf9\x0"_u16) == StringView::Comparison::Equals);
    ///
    /// // 日本語語語 (1 UTF16-LE sequence, 3 UTF8 sequence)
    /// StringView stringUtf8  = StringView("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe8\xaa\x9e\xe8\xaa\x9e"_u8);
    /// StringView stringUtf16 = StringView("\xE5\x65\x2C\x67\x9E\x8a\x9E\x8a\x9E\x8a\x00"_u16); // LE
    /// // Comparisons are on code points NOT grapheme clusters!!
    /// SC_ASSERT_RELEASE(stringUtf8.compare(stringUtf16) == StringView::Comparison::Equals);
    /// SC_ASSERT_RELEASE(stringUtf16.compare(stringUtf8) == StringView::Comparison::Equals);
    /// SC_ASSERT_RELEASE(stringUtf8 == stringUtf16);
    /// SC_ASSERT_RELEASE(stringUtf16 == stringUtf8);
    /// @endcode
    [[nodiscard]] Comparison compare(StringView other) const;

    /// @brief Ordering operator for StringView using StringView::compare
    /// @param other The string being compared to current one
    /// @return `true` if current string is Comparison::Smaller than other
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView sv[3] = {
    ///     StringView("3"),
    ///     StringView("1"),
    ///     StringView("2"),
    /// };
    /// Algorithms::bubbleSort(sv, sv + 3, [](StringView a, StringView b) { return a < b; });
    /// SC_TEST_EXPECT(sv[0] == "1");
    /// SC_TEST_EXPECT(sv[1] == "2");
    /// SC_TEST_EXPECT(sv[2] == "3");
    /// @endcode
    [[nodiscard]] bool operator<(StringView other) const { return compare(other) == Comparison::Smaller; }

    /// @brief Call given lambda with one of StringIteratorASCII, StringIteratorUTF8, StringIteratorUTF16 depending on
    /// encoding.
    /// @tparam Func A function/lambda with `auto operator()({StringIteratorASCII | StringIteratorUTF8 |
    /// StringIteratorUTF16})`
    /// @param func lambda / functor to be called
    /// @return whatever value is returned by invoking func
    template <typename Func>
    [[nodiscard]] constexpr auto withIterator(Func&& func) const;

    /// @brief Call given lambda with one of StringIteratorASCII, StringIteratorUTF8, StringIteratorUTF16 depending on
    /// encoding.
    /// @tparam Func Func A function/lambda with `auto operator()({StringIteratorASCII | StringIteratorUTF8 |
    /// StringIteratorUTF16}, {StringIteratorASCII | StringIteratorUTF8 | StringIteratorUTF16})`
    /// @param s1 StringView that will be passed as first iterator
    /// @param s2 StringView that will be passed as second iterator
    /// @param func the lambda / to be invoked
    /// @return whatever value is returned by invoking func
    template <typename Func>
    [[nodiscard]] static constexpr auto withIterators(StringView s1, StringView s2, Func&& func);

    /// @brief Returns a StringIterator from current StringView
    /// @tparam StringIterator can be StringIteratorASCII, StringIteratorUTF8 or StringIteratorUTF16
    /// @return StringIterator representing the StringView
    template <typename StringIterator>
    constexpr StringIterator getIterator() const;

    /// @brief Compare this StringView with another StringView for inequality
    /// @param other StringView to be compared with current one
    /// @return 'true' if the two StringView are different
    [[nodiscard]] constexpr bool operator!=(StringView other) const { return not operator==(other); }

    /// @brief Compare this StringView with another StringView for equality
    /// @param other StringView to be compared with current one
    /// @return 'true' if the two StringView are the same
    [[nodiscard]] constexpr bool operator==(StringSpan other) const;

    /// @brief Check if this StringView is equal to other StringView (operates on code points, not on utf graphemes).
    /// Returns the number of code points that are the same in both StringView-s.
    /// @param other The StringView to be compared to
    /// @param commonOverlappingPoints number of equal code points in both StringView
    /// @return `true` if the two StringViews are equal
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView asd = "123 456"_a8;
    /// size_t overlapPoints = 0;
    /// SC_TEST_EXPECT(not asd.fullyOverlaps("123___", overlapPoints) and overlapPoints == 3);
    /// @endcode
    [[nodiscard]] constexpr bool fullyOverlaps(StringView other, size_t& commonOverlappingPoints) const;

    /// @brief Check if StringView is empty
    /// @return `true` if string is empty
    [[nodiscard]] constexpr bool isEmpty() const { return text == nullptr or textSizeInBytes == 0; }

    /// @brief Check if StringView is immediately followed by a null termination character
    /// @return `true` if StringView is immediately followed by a null termination character
    [[nodiscard]] constexpr bool isNullTerminated() const { return hasNullTerm; }

    /// @brief Get size of the StringView in bytes
    /// @return Size in bytes of StringView
    [[nodiscard]] constexpr size_t sizeInBytes() const { return textSizeInBytes; }

    /// @brief Check if StringView ends with any utf code point in the given span
    /// @param codePoints The utf code points to check against
    /// @return Returns `true` if this StringView ends with any code point inside codePoints
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("123 456".endsWithAnyOf({'a', '6'})); // '6' will match
    /// @endcode
    [[nodiscard]] bool endsWithAnyOf(Span<const StringCodePoint> codePoints) const;

    /// @brief Check if StringView starts with any utf code point in the given span
    /// @param codePoints The utf code points to check against
    /// @return Returns `true` if this StringView starts with any code point inside codePoints
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("123 456".startsWithAnyOf({'1', '8'})); // '1' will match
    /// @endcode
    [[nodiscard]] bool startsWithAnyOf(Span<const StringCodePoint> codePoints) const;

    /// @brief Check if StringView starts with another StringView
    /// @param str The other StringView to check with current
    /// @return  Returns `true` if this StringView starts with str
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("123 456".startsWith("123"));
    /// @endcode
    [[nodiscard]] bool startsWith(const StringView str) const;

    /// @brief Check if StringView ends with another StringView
    /// @param str The other StringView to check with current
    /// @return  Returns `true` if this StringView ends with str
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("123 456".endsWith("456"));
    /// @endcode
    [[nodiscard]] bool endsWith(const StringView str) const;

    /// @brief Check if StringView contains another StringView with compatible encoding.
    /// @param str The other StringView to check with current
    /// @return  Returns `true` if this StringView contains str
    /// @warning This method will assert if strings have non compatible encoding.
    ///          It can be checked with StringView::hasCompatibleEncoding (str) == `true`
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView asd = "123 456";
    /// SC_TRY(asd.containsString("123"));
    /// SC_TRY(asd.containsString("456"));
    /// SC_TRY(not asd.containsString("124"));
    /// SC_TRY(not asd.containsString("4567"));
    /// @endcode
    [[nodiscard]] bool containsString(const StringView str) const;

    /// @brief Returns the remaining part of the string after matching stringToMatch
    /// @param stringToMatch String to match inside the source string
    /// @param remainingAfterSplit Portion of this StringView AFTER first match of stringToMatch (excluding the match)
    /// @return Returns `true` if stringToMatch has been found and split has been written to remainingAfterSplit
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView str("KEY = VALUE");
    /// StringView split;
    /// SC_TEST_EXPECT(str.splitAfter(" = ", split));
    /// SC_TEST_EXPECT(split == "VALUE");
    /// @endcode
    [[nodiscard]] bool splitAfter(const StringView stringToMatch, StringView& remainingAfterSplit) const;

    /// @brief Returns the part of the string before matching stringToMatch
    /// @param stringToMatch String to match inside the source string
    /// @param stringBeforeSplit Portion of this StringView BEFORE first match of stringToMatch (excluding the match)
    /// @return Returns `true` if stringToMatch has been found and split has been written to remainingAfterSplit
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView str("KEY = VALUE");
    /// StringView split;
    /// SC_TEST_EXPECT(str.splitBefore(" = ", split));
    /// SC_TEST_EXPECT(split == "KEY");
    /// @endcode
    [[nodiscard]] bool splitBefore(const StringView stringToMatch, StringView& stringBeforeSplit) const;

    /// @brief Check if StringView contains given utf code point
    /// @param c The utf code point to check against
    /// @return  Returns `true` if this StringView contains code point c
    [[nodiscard]] bool containsCodePoint(StringCodePoint c) const;

    /// @brief Check if current StringView has compatible encoding with str.
    ///        This means checking if two encodings have the same utf unit size.
    /// @param str The other StringView to check with current
    /// @return `true` if this StringView has compatible encoding with str
    [[nodiscard]] constexpr bool hasCompatibleEncoding(StringView str) const;

    /// Returns a StringView from two iterators. The from iterator will be shortened until the start of to

    /// @brief  Returns a StringView starting at `from` and ending at `to`.
    /// @tparam StringIterator One among StringIteratorASCII, StringIteratorUTF8 and StringIteratorUTF16
    /// @param from Indicates where the StringView will start.
    ///             The from iterator will be shortened until the start of to.
    /// @param to   Indicates where the StringView will end.
    /// @return A StringView  starting at `from` and ending at `to`.
    /// @note If from is `>` to an empty StringView will be returned
    template <typename StringIterator>
    static StringView fromIterators(StringIterator from, StringIterator to);

    /// @brief Returns a section of a string, from `it` to end of StringView.
    /// @tparam StringIterator One among StringIteratorASCII, StringIteratorUTF8 and StringIteratorUTF16
    /// @param it The iterator pointing at the start of the specified portion of StringView.
    /// @return Another StringView pointing at characters from `it` up to StringView end
    template <typename StringIterator>
    static StringView fromIteratorUntilEnd(StringIterator it);

    /// @brief Returns a section of a string, from start of StringView to `it`.
    /// @tparam StringIterator One among StringIteratorASCII, StringIteratorUTF8 and StringIteratorUTF16
    /// @param it The iterator pointing at the start of the specified portion of StringView.
    /// @return Another StringView pointing at characters from start of StringView until `it`
    template <typename StringIterator>
    static constexpr StringView fromIteratorFromStart(StringIterator it);

    /// @brief Get slice `[start, end)` starting at offset `start` and ending at `end` (measured in utf code points)
    /// @param start The initial code point where the slice starts
    /// @param end One after the final code point where the slice ends
    /// @return The `[start, end)` StringView slice
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView str = "123_567";
    /// SC_TEST_EXPECT(str.sliceStartEnd(0, 3) == "123");
    /// SC_TEST_EXPECT(str.sliceStartEnd(4, 7) == "567");
    /// @endcode
    [[nodiscard]] StringView sliceStartEnd(size_t start, size_t end) const;

    /// @brief Get slice `[start, start+length]` starting at offset `start` and of `length` code points
    /// @param start The initial code point where the slice starts
    /// @param length One after the final code point where the slice ends
    /// @return The `[start, start+length]` StringView slice
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView str = "123_567";
    /// SC_TEST_EXPECT(str.sliceStartLength(7, 0) == "");
    /// SC_TEST_EXPECT(str.sliceStartLength(0, 3) == "123");
    /// @endcode
    [[nodiscard]] StringView sliceStartLength(size_t start, size_t length) const;

    /// @brief Get slice `[offset, end]` measured in utf code points
    /// @param offset The initial code point where the slice starts
    /// @return The sliced StringView `[offset, end]`
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView str = "123_567";
    /// SC_TEST_EXPECT(str.sliceStart(4) == "567");
    /// @endcode
    [[nodiscard]] StringView sliceStart(size_t offset) const;

    /// @brief Get slice `[end-offset, end]` measured in utf code points
    /// @param offset The initial code point where the slice starts
    /// @return The sliced StringView `[end-offset, end]`
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView str = "123_567";
    /// SC_TEST_EXPECT(str.sliceEnd(4) == "123");
    /// @endcode
    [[nodiscard]] StringView sliceEnd(size_t offset) const;

    /// @brief Returns a shortened StringView removing ending utf code points matching the `codePoints` span
    /// @param codePoints The span of utf code points to look for
    /// @return The trimmed StringView
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("myTest_\n__"_a8.trimEndAnyOf({'_', '\n'}) == "myTest");
    /// SC_TEST_EXPECT("myTest"_a8.trimEndAnyOf({'_'}) == "myTest");
    /// @endcode
    [[nodiscard]] StringView trimEndAnyOf(Span<const StringCodePoint> codePoints) const;

    /// @brief Returns a shortened StringView removing starting utf code points matching the `codePoints` span
    /// @param codePoints The span of utf code points to look for
    /// @return The trimmed StringView
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("__\n_myTest"_a8.trimStartAnyOf({'_', '\n'}) == "myTest");
    /// SC_TEST_EXPECT("_myTest"_a8.trimStartAnyOf({'_'}) == "myTest");
    /// @endcode
    [[nodiscard]] StringView trimStartAnyOf(Span<const StringCodePoint> codePoints) const;

    /// @brief Returns a shortened StringView removing starting and ending utf code points inside the `codePoints` span
    /// @param codePoints The span of utf code points to look for
    /// @return The trimmed StringView
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("__\n_myTest__\n"_a8.trimAnyOf({'_', '\n'}) == "myTest");
    /// SC_TEST_EXPECT("_myTest"_a8.trimAnyOf({'_'}) == "myTest");
    /// @endcode
    [[nodiscard]] StringView trimAnyOf(Span<const StringCodePoint> codePoints) const;

    /// @brief Returns a shortened StringView without starting/ending utf code points inside {'\\r', '\\n', '\\t', ' '}
    /// @return The trimmed StringView
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("  \n_myTest__\n\t"_a8.trimWhiteSpaces() == "_myTest__");
    /// SC_TEST_EXPECT("\nmyTest \r"_a8.trimWhiteSpaces() == "myTest");
    /// @endcode
    [[nodiscard]] StringView trimWhiteSpaces() const;

    /// @brief Returns a shortened StringView from current cutting the first `start` bytes.
    /// @param start Offset in bytes where the slice starts.
    /// @return A sliced StringView starting at `start` bytes offset
    [[nodiscard]] constexpr StringView sliceStartBytes(size_t start) const;

    /// @brief Returns a shortened StringView taking a slice from `start` to `end` expressed in bytes.
    /// @param start Offset in bytes where the slice will start.
    /// @param end Offset in bytes where the slice will end.
    /// @return A sliced StringView starting at `start` bytes offset and ending at `end` bytes offset.
    [[nodiscard]] constexpr StringView sliceStartEndBytes(size_t start, size_t end) const;

    /// @brief Returns a shortened StringView taking a slice from `start` ending at `start`+`length` bytes.
    /// @param start Offset in bytes where the slice will start.
    /// @param length Length in bytes from `start` where the slice will end.
    /// @return A sliced StringView starting at `start` bytes offset and ending at `start`+`length` bytes offset.
    [[nodiscard]] constexpr StringView sliceStartLengthBytes(size_t start, size_t length) const;

    /// If the current view is an integer number, returns true

    /// @brief Check if StringView can be parsed as an integer number.
    /// @return `true` if StringView is an integer number.
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("-34"_a8.isIntegerNumber());
    /// SC_TEST_EXPECT("+12"_a8.isIntegerNumber());
    /// SC_TEST_EXPECT(not "+12$"_a8.isIntegerNumber());
    /// @endcode
    [[nodiscard]] bool isIntegerNumber() const;

    /// @brief Check if StringView can be parsed as an floating point number.
    /// @return `true` if StringView is a floating point number.
    ///
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT("-34."_a8.isFloatingNumber());
    /// SC_TEST_EXPECT("-34.0"_a8.isFloatingNumber());
    /// SC_TEST_EXPECT("0.34"_a8.isFloatingNumber());
    /// SC_TEST_EXPECT(not "+12$"_a8.isFloatingNumber());
    /// SC_TEST_EXPECT(not "$+12"_a8.isFloatingNumber());
    /// SC_TEST_EXPECT(not "+$12"_a8.isFloatingNumber());
    /// @endcode
    [[nodiscard]] bool isFloatingNumber() const;

    /// @brief Try parsing current StringView as a 32 bit integer.
    /// @param value Will receive the parsed 32 bit integer, if function returns `true`.
    /// @return `true` if the StringView has been successfully parsed as a 32 bit integer.
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView other("123");
    /// int32_t    value;
    /// if(other.parseInt32(value))
    /// {
    ///     // ... do something with value
    /// }
    /// @endcode
    [[nodiscard]] bool parseInt32(int32_t& value) const;

    /// @brief Try parsing current StringView as a floating point number.
    /// @param value Will receive the parsed floating point number, if function returns `true`.
    /// @return `true` if the StringView has been successfully parsed as a floating point number.
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView other("12.34");
    /// float    value;
    /// if(other.parseFloat(value))
    /// {
    ///     // ... do something with value
    /// }
    /// @endcode
    [[nodiscard]] bool parseFloat(float& value) const;

    /// @brief Try parsing current StringView as a double precision floating point number.
    /// @param value Will receive the parsed double precision floating point number, if function returns `true`.
    /// @return `true` if the StringView has been successfully parsed as a double precision floating point number.
    ///
    /// Example:
    /// @code{.cpp}
    /// StringView other("12.342321");
    /// double    value;
    /// if(other.parseDouble(value))
    /// {
    ///     // ... do something with value
    /// }
    /// @endcode
    [[nodiscard]] bool parseDouble(double& value) const;

  private:
    template <typename T>
    struct identity
    {
    };
    template <typename Type>
    constexpr StringIteratorASCII getIterator(identity<Type>) const;
    constexpr StringIteratorUTF8  getIterator(identity<StringIteratorUTF8>) const;
    constexpr StringIteratorUTF16 getIterator(identity<StringIteratorUTF16>) const;
    template <typename StringIterator1, typename StringIterator2>
    static constexpr bool equalsIterator(StringIterator1 t1, StringIterator2 t2, size_t& points);

    template <typename StringIterator>
    constexpr bool equalsIterator(StringView other, size_t& points) const;
};

/// @brief Splits a StringView in tokens according to separators
struct SC::StringViewTokenizer
{
    StringCodePoint splittingCharacter = 0; ///< The last splitting character matched in current tokenization

    size_t numSplitsNonEmpty = 0; ///< How many non-empty splits have occurred in current tokenization
    size_t numSplitsTotal    = 0; ///< How many total splits have occurred in current tokenization

    StringView component; ///< Current component that has been tokenized by tokenizeNext
    StringView processed; ///< Substring of original string passed in constructor processed so far
    StringView remaining; ///< Substring from current position until the end of original text

    enum Options
    {
        IncludeEmpty, ///< If to tokenizeNext should return also empty tokens
        SkipEmpty     ///< If to tokenizeNext should NOT return also empty tokens
    };

    /// @brief Build a tokenizer operating on the given text string view
    StringViewTokenizer(StringView text) : remaining(text), originalText(text) {}

    /// @brief Splits the string along a list of separators
    /// @param separators List of separators
    /// @param options If to skip empty tokens or not
    /// @return `true` if there are additional tokens to parse
    /// @n
    /// Example:
    /// @code{.cpp}
    /// StringViewTokenizer tokenizer("bring,me,the,horizon");
    /// while (tokenizer.tokenizeNext(',', StringViewTokenizer::SkipEmpty))
    /// {
    ///     console.printLine(tokenizer.component);
    /// }
    /// @endcode
    [[nodiscard]] bool tokenizeNext(Span<const StringCodePoint> separators, Options options = Options::SkipEmpty);

    /// @brief Tokenizes from current position to first newline.
    ///
    /// @return `true` if a new line has been found
    /// @n
    /// Example:
    /// @code{.cpp}
    /// StringViewTokenizer lines("Line1\nLine2\nLine3\n");
    /// SC_TEST_EXPECT(lines.tokenizeNextLine());
    /// SC_TEST_EXPECT(lines.component == "Line1");
    /// SC_TEST_EXPECT(lines.tokenizeNextLine());
    /// SC_TEST_EXPECT(lines.component == "Line2");
    /// SC_TEST_EXPECT(lines.tokenizeNextLine());
    /// SC_TEST_EXPECT(lines.component == "Line3");
    /// SC_TEST_EXPECT(not lines.tokenizeNextLine());
    /// @endcode
    [[nodiscard]] bool tokenizeNextLine() { return tokenizeNext({'\n'}); }

    /// @brief Count the number of tokens that exist in the string view passed in constructor, when splitted along the
    /// given separators
    /// @param separators Separators to split the original string with
    /// @return Current StringViewTokenizer to inspect SC::StringViewTokenizer::numSplitsNonEmpty or
    /// SC::StringViewTokenizer::numSplitsTotal.
    /// @n
    /// Example:
    /// @code{.cpp}
    /// SC_TEST_EXPECT(StringViewTokenizer("___").countTokens('_').numSplitsNonEmpty == 0);
    /// SC_TEST_EXPECT(StringViewTokenizer("___").countTokens('_').numSplitsTotal == 3);
    /// @endcode
    StringViewTokenizer& countTokens(Span<const StringCodePoint> separators);

    /// @brief Check if the tokenizer has processed the entire the string view passed in the constructor
    [[nodiscard]] bool isFinished() const;

  private:
    StringView originalText; // Original text as passed in the constructor
};

/// @brief Algorithms operating on strings (glob / wildcard).
/// @n
/// Example
/// @code{.cpp}
/// SC_ASSERT(StringAlgorithms::matchWildcard("", ""));
/// SC_ASSERT(StringAlgorithms::matchWildcard("1?3", "123"));
/// SC_ASSERT(StringAlgorithms::matchWildcard("1*3", "12223"));
/// SC_ASSERT(StringAlgorithms::matchWildcard("*2", "12"));
/// SC_ASSERT(not StringAlgorithms::matchWildcard("*1", "12"));
/// SC_ASSERT(not StringAlgorithms::matchWildcard("*1", "112"));
/// SC_ASSERT(not StringAlgorithms::matchWildcard("**1", "112"));
/// SC_ASSERT(not StringAlgorithms::matchWildcard("*?1", "112"));
/// SC_ASSERT(StringAlgorithms::matchWildcard("1*", "12123"));
/// SC_ASSERT(StringAlgorithms::matchWildcard("*/myString", "myString/myString/myString"));
/// SC_ASSERT(StringAlgorithms::matchWildcard("**/myString", "myString/myString/myString"));
/// SC_ASSERT(not StringAlgorithms::matchWildcard("*/String", "myString/myString/myString"));
/// SC_ASSERT(StringAlgorithms::matchWildcard("*/Directory/File.cpp", "/Root/Directory/File.cpp"));
/// @endcode
struct SC::StringAlgorithms
{
    [[nodiscard]] static bool matchWildcard(StringView s1, StringView s2);

  private:
    template <typename StringIterator1, typename StringIterator2>
    [[nodiscard]] static bool matchWildcardIterator(StringIterator1 pattern, StringIterator2 text);
};

//! @}

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
namespace SC
{
constexpr SC::StringView operator""_a8(const char* txt, size_t sz)
{
    return StringView({txt, sz}, true, StringEncoding::Ascii);
}
constexpr StringView operator""_u8(const char* txt, size_t sz)
{
    return StringView({txt, sz}, true, StringEncoding::Utf8);
}
constexpr StringView operator""_u16(const char* txt, size_t sz)
{
    const bool isNullTerminated = sz > 0 and sz % 2 == 1 and txt[sz - 1] == 0;
    return StringView({txt, isNullTerminated ? sz - 1 : sz}, isNullTerminated, StringEncoding::Utf16);
}
} // namespace SC

template <typename StringIterator>
constexpr StringIterator SC::StringView::getIterator() const
{
    // For GCC complaining about specialization in non-namespace scope
    return getIterator(identity<StringIterator>());
}

template <typename Type>
constexpr SC::StringIteratorASCII SC::StringView::getIterator(identity<Type>) const
{
    return StringIteratorASCII(text, text + textSizeInBytes);
}
constexpr SC::StringIteratorUTF8 SC::StringView::getIterator(identity<StringIteratorUTF8>) const
{
    return StringIteratorUTF8(text, text + textSizeInBytes);
}
constexpr SC::StringIteratorUTF16 SC::StringView::getIterator(identity<StringIteratorUTF16>) const
{
    return StringIteratorUTF16(text, text + textSizeInBytes);
}

template <typename StringIterator1, typename StringIterator2>
constexpr bool SC::StringView::equalsIterator(StringIterator1 t1, StringIterator2 t2, size_t& points)
{
    StringCodePoint c1 = 0;
    StringCodePoint c2 = 0;
    while (t1.advanceRead(c1) and t2.advanceRead(c2))
    {
        if (c1 != c2)
        {
            return false;
        }
        points++;
    }
    return t1.isAtEnd() and t2.isAtEnd();
}

template <typename StringIterator>
constexpr bool SC::StringView::equalsIterator(StringView other, size_t& points) const
{
    auto it = getIterator<StringIterator>();
    switch (other.getEncoding())
    {
    case StringEncoding::Ascii: return equalsIterator(it, other.getIterator<StringIteratorASCII>(), points);
    case StringEncoding::Utf8: return equalsIterator(it, other.getIterator<StringIteratorUTF8>(), points);
    case StringEncoding::Utf16: return equalsIterator(it, other.getIterator<StringIteratorUTF16>(), points);
    }
    Assert::unreachable();
}

[[nodiscard]] inline auto SC::StringView::getNullTerminatedNative() const
{
#if SC_PLATFORM_WINDOWS
    SC_ASSERT_RELEASE(hasNullTerm && (getEncoding() == StringEncoding::Utf16));
    return reinterpret_cast<const wchar_t*>(text);
#else
    SC_ASSERT_RELEASE(hasNullTerm && (getEncoding() == StringEncoding::Utf8 || getEncoding() == StringEncoding::Ascii));
    return text;
#endif
}

[[nodiscard]] constexpr bool SC::StringView::operator==(StringSpan other) const
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
    if (hasCompatibleEncoding(other))
    {
        if (textSizeInBytes != other.textSizeInBytes)
            return false;
        if (__builtin_is_constant_evaluated())
        {
            auto it1 = text;
            auto it2 = other.text;
            auto sz  = textSizeInBytes;
            for (size_t idx = 0; idx < sz; ++idx)
                if (it1[idx] != it2[idx])
                    return false;
        }
        else
        {
            if (text == nullptr)
            {
                return other.textSizeInBytes == 0;
            }
            if (other.text == nullptr)
            {
                return textSizeInBytes == 0;
            }
            return memcmp(text, other.text, textSizeInBytes) == 0;
        }
    }
    size_t commonOverlappingPoints = 0;
    return fullyOverlaps(other, commonOverlappingPoints);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

constexpr bool SC::StringView::fullyOverlaps(StringView other, size_t& commonOverlappingPoints) const
{
    commonOverlappingPoints = 0;
    switch (getEncoding())
    {
    case StringEncoding::Ascii: return equalsIterator<StringIteratorASCII>(other, commonOverlappingPoints);
    case StringEncoding::Utf8: return equalsIterator<StringIteratorUTF8>(other, commonOverlappingPoints);
    case StringEncoding::Utf16: return equalsIterator<StringIteratorUTF16>(other, commonOverlappingPoints);
    }
    Assert::unreachable();
}

template <typename Func>
constexpr auto SC::StringView::withIterator(Func&& func) const
{
    switch (getEncoding())
    {
    case StringEncoding::Ascii: return func(getIterator<StringIteratorASCII>());
    case StringEncoding::Utf8: return func(getIterator<StringIteratorUTF8>());
    case StringEncoding::Utf16: return func(getIterator<StringIteratorUTF16>());
    }
    Assert::unreachable();
}

template <typename Func>
constexpr auto SC::StringView::withIterators(StringView s1, StringView s2, Func&& func)
{
    return s1.withIterator([&s2, &func](auto it1)
                           { return s2.withIterator([&it1, &func](auto it2) { return func(it1, it2); }); });
}

constexpr bool SC::StringView::hasCompatibleEncoding(StringView str) const
{
    return StringEncodingAreBinaryCompatible(getEncoding(), str.getEncoding());
}

template <typename StringIterator>
inline SC::StringView SC::StringView::fromIterators(StringIterator from, StringIterator to)
{
    const ssize_t numBytes = to.bytesDistanceFrom(from);
    if (numBytes >= 0)
    {
        StringIterator fromEnd = from;
        fromEnd.setToEnd();
        if (fromEnd.bytesDistanceFrom(to) >= 0) // If current iterator of to is inside from range
            return StringView({from.it, static_cast<size_t>(numBytes)}, false, StringIterator::getEncoding());
    }
    return StringView(); // TODO: Make StringView::fromIterators return bool to make it fallible
}

template <typename StringIterator>
inline SC::StringView SC::StringView::fromIteratorUntilEnd(StringIterator it)
{
    StringIterator endIt = it;
    endIt.setToEnd();
    const size_t numBytes = static_cast<size_t>(endIt.bytesDistanceFrom(it));
    return StringView({it.it, numBytes}, false, StringIterator::getEncoding());
}

template <typename StringIterator>
constexpr SC::StringView SC::StringView::fromIteratorFromStart(StringIterator it)
{
    StringIterator start = it;
    start.setToStart();
    const size_t numBytes = static_cast<size_t>(it.bytesDistanceFrom(start));
    return StringView({start.it, numBytes}, false, StringIterator::getEncoding());
}

constexpr SC::StringView SC::StringView::sliceStartBytes(size_t start) const
{
    if (start < sizeInBytes())
        return sliceStartLengthBytes(start, sizeInBytes() - start);
    SC_ASSERT_RELEASE(start < sizeInBytes());
    return StringView({text, 0}, false, getEncoding());
}

constexpr SC::StringView SC::StringView::sliceStartEndBytes(size_t start, size_t end) const
{
    if (end >= start)
        return sliceStartLengthBytes(start, end - start);
    SC_ASSERT_RELEASE(end >= start);
    return StringView({text, 0}, false, getEncoding());
}

constexpr SC::StringView SC::StringView::sliceStartLengthBytes(size_t start, size_t length) const
{
    if (start + length > sizeInBytes())
    {
        SC_ASSERT_RELEASE(start + length > sizeInBytes());
        return StringView({text, 0}, false, getEncoding());
    }
    return StringView({text + start, length}, hasNullTerm and (start + length == sizeInBytes()), getEncoding());
}
