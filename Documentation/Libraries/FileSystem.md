@page library_file_system File System

@brief 🟩 File System operations { exists, copy, delete } for { files and directories }

[TOC]

FileSystem executed executing operations on files and directories.  

# Quick Sheet

\snippet Tests/Libraries/FileSystem/FileSystemTest.cpp FileSystemQuickSheetSnippet

# Features

| SC::FileSystem                                | @copybrief SC::FileSystem                                 |
|:----------------------------------------------|:----------------------------------------------------------|
| **Copy Files**                                |                                                           |
| SC::FileSystem::copyFile                      | @copybrief SC::FileSystem::copyFile                       |
| **Delete Files**                              |                                                           |
| SC::FileSystem::removeFile                    | @copybrief SC::FileSystem::removeFile                     |
| SC::FileSystem::removeFileIfExists            | @copybrief SC::FileSystem::removeFileIfExists             |
| **Copy Directories**                          |                                                           |
| SC::FileSystem::copyDirectory                 | @copybrief SC::FileSystem::copyDirectory                  |
| **Delete Directories**                        |                                                           |
| SC::FileSystem::removeEmptyDirectory          | @copybrief SC::FileSystem::removeEmptyDirectory           |
| SC::FileSystem::removeDirectoryRecursive      | @copybrief SC::FileSystem::removeDirectoryRecursive       |
| **Create Directories**                        |                                                           |
| SC::FileSystem::makeDirectory                 | @copybrief SC::FileSystem::makeDirectory                  |
| SC::FileSystem::makeDirectoryIfNotExists      | @copybrief SC::FileSystem::makeDirectoryIfNotExists       |
| SC::FileSystem::makeDirectoryRecursive        | @copybrief SC::FileSystem::makeDirectoryRecursive         |
| **Check Existence**                           |                                                           |
| SC::FileSystem::exists                        | @copybrief SC::FileSystem::exists                         |
| SC::FileSystem::existsAndIsFile               | @copybrief SC::FileSystem::existsAndIsFile                |
| SC::FileSystem::existsAndIsDirectory          | @copybrief SC::FileSystem::existsAndIsDirectory           |
| **Rename files or directories**               |                                                           |
| SC::FileSystem::rename                        | @copybrief SC::FileSystem::rename                         |
| **Read / Change modification time**           |                                                           |
| SC::FileSystem::getFileStat                   | @copybrief SC::FileSystem::getFileStat                    |
| SC::FileSystem::setLastModifiedTime           | @copybrief SC::FileSystem::setLastModifiedTime            |

| Miscellaneous Classes                                     |                                                                       |
|:----------------------------------------------------------|:----------------------------------------------------------------------|
| SC::FileSystem::Operations                                | @copybrief SC::FileSystem::Operations                                 |
| **Get Executable / Application Path**                     |                                                                       |
| SC::FileSystem::Operations::getExecutablePath             | @copybrief SC::FileSystem::Operations::getExecutablePath              |
| SC::FileSystem::Operations::getApplicationRootDirectory   | @copybrief SC::FileSystem::Operations::getApplicationRootDirectory    |

# Status
🟩 Usable  
The library contains commonly used function but it's missing some notable ones like `stat`.
SC::FileSystem::getFileTime and SC::FileSystem::setLastModifiedTime will probably be refactored in a future dedicated class for handling `stat` based operations.

# Description 

SC::FileSystem allows all typical file operations ( exists | copy | delete | make files or directory).
Some less used functions are  SC::FileSystem::getFileTime and SC::FileSystem::setLastModifiedTime .
The library doesn't allow reading or writing seeking inside a file, as that is domain of the [File](@ref library_file) library.
SC::FileSystem::init needs an absolute path to a directory and makes it a the *base directory*.
All paths passed later on as arguments to all methods can be either absolute paths or relative.
If they are relative, they will be interpreted as relative to the base directory and **NOT** current directory of the process.
The class wants explicitly to make sure its behavior doesn't implicitly depend on current directory of process 
(unless it's passed explicitly to SC::FileSystem::init of course).

Use SC::Path from [Strings](@ref library_strings) library to parse and compose paths.

# FileSystem
## copyFile
@copydoc SC::FileSystem::copyFile

## copyDirectory
@copydoc SC::FileSystem::copyDirectory

## removeDirectoryRecursive
@copydoc SC::FileSystem::removeDirectoryRecursive

## makeDirectoryRecursive
@copydoc SC::FileSystem::makeDirectoryRecursive

## existsAndIsFile
@copydoc SC::FileSystem::existsAndIsFile

## existsAndIsDirectory
@copydoc SC::FileSystem::existsAndIsDirectory

## rename
@copydoc SC::FileSystem::rename

## write
@copydoc SC::FileSystem::write

## read
@copydoc SC::FileSystem::read

# Roadmap

🟦 Complete Features:
- `access`
- `chmod`
- `chown`
- `fchmod`
- `fchown`
- `fdatasync`
- `fstat`
- `fsync`
- `ftruncate`
- `ftruncate`
- `lchown`
- `link` (hardlink)
- `lstat`
- `readlink`
- `sendfile`
- `stat`
- `statfs`
