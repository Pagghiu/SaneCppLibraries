@page library_strings Strings

@brief 🟩 String formatting / conversion / manipulation (ASCII / UTF8 / UTF16)

[TOC]

Strings library allow read-only and write string operations and UTF Conversions.
Path is able to parse and manipulate windows and posix paths.

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory)
- All dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory)

# Statistics
- Lines of code (excluding comments): 3387
- Lines of code (including comments): 4950

# Features

| Class                     | Description
|:--------------------------|:--------------------------------------|
| SC::String                | @copybrief SC::String                 |
| SC::StringBuilder         | @copybrief SC::StringBuilder          |
| SC::StringConverter       | @copybrief SC::StringConverter        |
| SC::StringIterator        | @copybrief SC::StringIterator         |
| SC::StringIteratorASCII   | @copybrief SC::StringIteratorASCII    |
| SC::StringIteratorUTF8    | @copybrief SC::StringIteratorUTF8     |
| SC::StringIteratorUTF16   | @copybrief SC::StringIteratorUTF16    |
| SC::StringView            | @copybrief SC::StringView             |
| SC::StringAlgorithms      | @copybrief SC::StringAlgorithms       |
| SC::StringViewTokenizer   | @copybrief SC::StringViewTokenizer    |
| SC::StringFormat          | @copybrief SC::StringFormat           |
| SC::Path                  | @copybrief SC::Path                   |
| SC::Console               | @copybrief SC::Console                |

# Status
🟩 Usable  
Library is usable and can be successfully used to mix operations with strings made in different encodings.

# Definition

## StringView
@copydoc SC::StringView

### StringView::containsString
@copydoc SC::StringView::containsString

### StringView::compare
@copydoc SC::StringView::compare

### StringView::fullyOverlaps
@copydoc SC::StringView::fullyOverlaps

### StringView::startsWithAnyOf
@copydoc SC::StringView::startsWithAnyOf

### StringView::endsWithAnyOf
@copydoc SC::StringView::endsWithAnyOf

### StringView::startsWith
@copydoc SC::StringView::startsWith

### StringView::endsWith
@copydoc SC::StringView::endsWith

### StringView::containsString
@copydoc SC::StringView::containsString

### StringView::containsCodePoint
@copydoc SC::StringView::containsCodePoint

### StringView::sliceStartEnd
@copydoc SC::StringView::sliceStartEnd

### StringView::sliceStartLength
@copydoc SC::StringView::sliceStartLength

### StringView::sliceStart
@copydoc SC::StringView::sliceStart

### StringView::sliceEnd
@copydoc SC::StringView::sliceEnd

### StringView::trimEndAnyOf
@copydoc SC::StringView::trimEndAnyOf

### StringView::trimStartAnyOf
@copydoc SC::StringView::trimStartAnyOf

## StringViewTokenizer
@copydoc SC::StringViewTokenizer

### StringViewTokenizer::tokenizeNext
@copydoc SC::StringViewTokenizer::tokenizeNext

### StringViewTokenizer::countTokens
@copydoc SC::StringViewTokenizer::countTokens

## StringBuilder
@copydoc SC::StringBuilder

### StringBuilder::format
@copydoc SC::StringBuilder::format

### StringBuilder::append
@copydoc SC::StringBuilder::append

### StringBuilder::appendReplaceAll
@copydoc SC::StringBuilder::appendReplaceAll

### StringBuilder::appendReplaceMultiple
@copydoc SC::StringBuilder::appendReplaceMultiple

### StringBuilder::appendHex
@copydoc SC::StringBuilder::appendHex

## String
@copydoc SC::String

## StringIterator
@copydoc SC::StringIterator

## StringFormat
@copydoc SC::StringFormat

## StringConverter
@copydoc SC::StringConverter

## StringAlgorithms
@copydoc SC::StringAlgorithms

## Console
@copydoc SC::Console

## Path
@copydoc SC::Path

### Path::isAbsolute
@copydoc SC::Path::isAbsolute

### Path::dirname
@copydoc SC::Path::dirname

### Path::basename
@copydoc SC::Path::basename

### Path::parseNameExtension
@copydoc SC::Path::parseNameExtension

### Path::normalize
@copydoc SC::Path::normalize

### Path::relativeFromTo
@copydoc SC::Path::relativeFromTo

# Implementation
A design choice of the library is that strings cannot be modified.
Strings are either read-only (SC::StringView) or they need to be built from scratch with SC::StringBuilder.
Another design choice is to support different encodings (`ASCII`, `UTF8` or `UTF16`).
The reason is that `ASCII` is efficient when it's known that the strings manipulated have Code Points made of a single byte.
UTF8 is useful on Posix platforms and UTF16 is needed because that's the default encoding used by Win32 API.
All functions interacting with filesystem, for example the ones in [FileSystem](@ref library_file_system) or 
[FileSystemIterator](@ref library_file_system_iterator), return strings in the operating system native encoding.
This means that on windows they will be UTF16 strings and on Apple Devices (or Linux) they are UTF8.

# Roadmap
We need to understand if we want to allow iterating *grapheme clusters* (perceived end-user 'characters') or advanced
capabilities like normalization and uppercase / lowercase conversions. As doing these operations from scratch is non trivial
we will investigate if there OS functions allowing to achieve that functionality

🟦 Complete Features:
- UTF Normalization
- UTF Case Conversion

💡 Unplanned Features:
- UTF word breaking
- Grapheme Cluster iteration
