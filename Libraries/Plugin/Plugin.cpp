// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Plugin.h"

#define SC_ASSERT_PROVIDER PluginAssert
#include "../Common/Assert.inl"

#include "../Common/Deferred.h"
#include "../Common/PlatformInstructionSet.h"
#include "../Process/Internal/StringsArena.h"
#include "../Process/Process.h"

#include "Internal/PluginFileSystem.h" // This must be included before VisualStudioPathFinder.h for the unity build
#include "Internal/PluginString.h"
#if SC_PLATFORM_WINDOWS
#include "Internal/DebuggerWindows.inl"
#include "Internal/VisualStudioPathFinder.h"
#include <sys/timeb.h>
#else
#include <time.h>
#endif
#include "Internal/DynamicLibrary.inl"
#include "Internal/PluginFileSystemIterator.h"

namespace SC
{

namespace
{
//! @brief Get current time in milliseconds since epoch (realtime clock)
inline TimeMs PluginNow()
{
#if SC_PLATFORM_WINDOWS
    struct _timeb t;
    _ftime_s(&t);
    return {static_cast<int64_t>(t.time) * 1000 + t.millitm};
#else
    struct timespec nowTimeSpec;
    clock_gettime(CLOCK_REALTIME, &nowTimeSpec);
    return {static_cast<int64_t>((nowTimeSpec.tv_nsec >> 20) + nowTimeSpec.tv_sec * 1000)};
#endif
}

} // anonymous namespace
} // namespace SC
struct SC::PluginCompilerEnvironment::Internal
{
    [[nodiscard]] static bool writeFlags(StringSpan flags, StringsArena& arena)
    {
        PluginString::Tokenizer tokenizer(flags);
        while (tokenizer.next(' '))
        {
            if (not arena.appendAsSingleString(tokenizer.component))
                return false;
        }
        return true;
    }
};

bool SC::PluginDefinition::find(StringSpan text, StringSpan& extracted)
{
    size_t begin = 0;
    SC_TRY(PluginString::find(text, "SC_BEGIN_PLUGIN", 0, begin));
    while (begin < text.sizeInBytes() and text.bytesWithoutTerminator()[begin] != '\n')
        ++begin;
    SC_TRY(begin < text.sizeInBytes());
    ++begin;

    size_t end = 0;
    SC_TRY(PluginString::find(text, "SC_END_PLUGIN", begin, end));
    while (end > begin and text.bytesWithoutTerminator()[end - 1] != '\n')
        --end;
    extracted = PluginString::slice(text, begin, end - begin);
    return true;
}

SC::ResultPlugin SC::PluginDefinition::getDynamicLibraryAbsolutePath(StringPath& fullDynamicPath) const
{
    SC_TRY(PluginString::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
#if SC_PLATFORM_WINDOWS
    if (not fullDynamicPath.append(".dll"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAppend);
#elif SC_PLATFORM_APPLE
    if (not fullDynamicPath.append(".dylib"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAppend);
#else
    if (not fullDynamicPath.append(".so"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAppend);
#endif
    return ResultPlugin(true);
}

SC::ResultPlugin SC::PluginDefinition::getDynamicLibraryPDBAbsolutePath(StringPath& fullDynamicPath) const
{
    SC_TRY(PluginString::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
#if SC_PLATFORM_WINDOWS
    if (not fullDynamicPath.append(".pdb"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAppend);
#elif SC_PLATFORM_APPLE
    if (not fullDynamicPath.append(".dSYM"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAppend);
#else
    if (not fullDynamicPath.append(".sym"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAppend);
#endif
    return ResultPlugin(true);
}

SC::ResultPlugin SC::PluginDefinition::parse(StringSpan text, PluginDefinition& pluginDefinition, bool& parsed)
{
    parsed = false;
    if (text.getEncoding() == StringEncoding::Utf16)
        return ResultPlugin(PluginError::UnsupportedPathEncoding);
    struct Cursor
    {
        StringSpan text;
        size_t     offset = 0;

        bool parseLine(StringSpan& key, StringSpan& value)
        {
            const auto isSkipped = [](char current)
            { return current == '\t' or current == '\n' or current == '\r' or current == ' ' or current == '/'; };
            while (offset < text.sizeInBytes() and isSkipped(text.bytesWithoutTerminator()[offset]))
                ++offset;
            const size_t keyStart = offset;
            while (offset < text.sizeInBytes() and text.bytesWithoutTerminator()[offset] != ':' and
                   not isSkipped(text.bytesWithoutTerminator()[offset]))
                ++offset;
            key = PluginString::slice(text, keyStart, offset - keyStart);
            while (offset < text.sizeInBytes() and isSkipped(text.bytesWithoutTerminator()[offset]))
                ++offset;
            if (offset >= text.sizeInBytes() or text.bytesWithoutTerminator()[offset++] != ':')
                return false;
            while (offset < text.sizeInBytes() and isSkipped(text.bytesWithoutTerminator()[offset]))
                ++offset;
            const size_t valueStart = offset;
            while (offset < text.sizeInBytes() and text.bytesWithoutTerminator()[offset] != '\n' and
                   text.bytesWithoutTerminator()[offset] != '\r')
                ++offset;
            value                     = PluginString::slice(text, valueStart, offset - valueStart);
            const bool endedByNewLine = offset < text.sizeInBytes();
            while (offset < text.sizeInBytes() and
                   (text.bytesWithoutTerminator()[offset] == '\n' or text.bytesWithoutTerminator()[offset] == '\r'))
                ++offset;
            return endedByNewLine or not value.isEmpty();
        }
    } cursor{text};

    StringSpan key, value;
    bool       gotFields[4] = {false};
    while (cursor.parseLine(key, value))
    {
        if (key == "Name")
        {
            gotFields[0] = true;
            if (not pluginDefinition.identity.name.assign(value))
                return ResultPlugin::withRequiredBytes(PluginError::DefinitionCapacityExceeded,
                                                       PluginErrorDetail::MetadataName,
                                                       static_cast<uint32_t>(value.sizeInBytes() + 1));
        }
        else if (key == "Version")
        {
            gotFields[1] = true;
            if (not pluginDefinition.identity.version.assign(value))
                return ResultPlugin::withRequiredBytes(PluginError::DefinitionCapacityExceeded,
                                                       PluginErrorDetail::MetadataVersion,
                                                       static_cast<uint32_t>(value.sizeInBytes() + 1));
        }
        else if (key == "Description")
        {
            gotFields[2] = true;
            if (not pluginDefinition.description.assign(value))
                return ResultPlugin::withRequiredBytes(PluginError::DefinitionCapacityExceeded,
                                                       PluginErrorDetail::MetadataDescription,
                                                       static_cast<uint32_t>(value.sizeInBytes() + 1));
        }
        else if (key == "Category")
        {
            gotFields[3] = true;
            if (not pluginDefinition.category.assign(value))
                return ResultPlugin::withRequiredBytes(PluginError::DefinitionCapacityExceeded,
                                                       PluginErrorDetail::MetadataCategory,
                                                       static_cast<uint32_t>(value.sizeInBytes() + 1));
        }
        else if (key == "Dependencies") // Optional
        {
            PluginString::Tokenizer tokenizer(value);
            while (tokenizer.next(','))
            {
                PluginIdentifier identifier;
                if (not identifier.assign(tokenizer.component))
                    return ResultPlugin::withRequiredBytes(
                        PluginError::DefinitionCapacityExceeded, PluginErrorDetail::MetadataDependency,
                        static_cast<uint32_t>(tokenizer.component.sizeInBytes() + 1));
                if (not pluginDefinition.dependencies.push_back(move(identifier)))
                    return ResultPlugin::withRequiredElements(
                        PluginError::DefinitionCapacityExceeded, PluginErrorDetail::MetadataDependency,
                        static_cast<uint32_t>(pluginDefinition.dependencies.size() + 1));
            }
        }
        else if (key == "Build") // Optional
        {
            PluginString::Tokenizer tokenizer(value);
            while (tokenizer.next(','))
            {
                PluginBuildOption option;
                if (not option.assign(tokenizer.component))
                    return ResultPlugin::withRequiredBytes(
                        PluginError::DefinitionCapacityExceeded, PluginErrorDetail::MetadataBuildOption,
                        static_cast<uint32_t>(tokenizer.component.sizeInBytes() + 1));
                if (not pluginDefinition.build.push_back(option))
                    return ResultPlugin::withRequiredElements(PluginError::DefinitionCapacityExceeded,
                                                              PluginErrorDetail::MetadataBuildOption,
                                                              static_cast<uint32_t>(pluginDefinition.build.size() + 1));
            }
        }
    }
    for (size_t i = 0; i < sizeof(gotFields) / sizeof(bool); ++i)
    {
        if (not gotFields[i])
        {
            constexpr PluginErrorDetail requiredFields[] = {
                PluginErrorDetail::MetadataName, PluginErrorDetail::MetadataVersion,
                PluginErrorDetail::MetadataDescription, PluginErrorDetail::MetadataCategory};
            return ResultPlugin(PluginError::InvalidDefinition, requiredFields[i]);
        }
    }
    parsed = true;
    return ResultPlugin(true);
}

bool SC::PluginDefinition::parse(StringSpan text, PluginDefinition& pluginDefinition)
{
    bool parsed = false;
    return static_cast<bool>(parse(text, pluginDefinition, parsed)) and parsed;
}

struct SC::PluginScanner::ScannerState
{
    Span<PluginDefinition> definitions;

    size_t numDefinitions      = 0;
    bool   multipleDefinitions = false;

    ResultPlugin storeTentativePluginFolder(StringSpan pluginDirectory)
    {
        if (numDefinitions == 0 or not definitions[numDefinitions - 1].identity.identifier.isEmpty())
        {
            if (numDefinitions >= definitions.sizeInElements())
                return ResultPlugin::withRequiredElements(PluginError::ScannerCapacityExceeded,
                                                          PluginErrorDetail::ScannerDefinitionStorage,
                                                          static_cast<uint32_t>(numDefinitions + 1));
            numDefinitions++;
            definitions[numDefinitions - 1] = {};
        }
        PluginDefinition& pluginDefinition = definitions[numDefinitions - 1];
        pluginDefinition.files.clear();
        if (not pluginDefinition.directory.assign(pluginDirectory))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::ScannerCandidatePath);
        multipleDefinitions = false;
        return ResultPlugin(true);
    }

    ResultPlugin tryParseCandidate(StringSpan candidate, IGrowableBuffer&& tempFileBuffer)
    {
        PluginDefinition& pluginDefinition = definitions[numDefinitions - 1];
        {
            PluginFile pluginFile;
            if (not pluginFile.absolutePath.assign(candidate))
                return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::ScannerCandidatePath);
            if (not pluginDefinition.files.push_back(move(pluginFile)))
                return ResultPlugin::withRequiredElements(PluginError::DefinitionCapacityExceeded,
                                                          PluginErrorDetail::ScannerCandidateFile,
                                                          static_cast<uint32_t>(pluginDefinition.files.size() + 1));
        }
        SC_TRY(PluginFileSystem::readAbsoluteFile(candidate, move(tempFileBuffer)));
        StringSpan extracted;
        StringSpan fileView = {{tempFileBuffer.data(), tempFileBuffer.size()}, false, StringEncoding::Utf8};
        if (PluginDefinition::find(fileView, extracted))
        {
            bool parsed = false;
            SC_TRY(PluginDefinition::parse(extracted, pluginDefinition, parsed));
            if (parsed)
            {
                if (pluginDefinition.identity.identifier.isEmpty())
                {
                    const StringSpan identifier = PluginString::basename(pluginDefinition.directory.view());
                    if (not pluginDefinition.identity.identifier.assign(identifier))
                        return ResultPlugin(PluginError::DefinitionCapacityExceeded,
                                            PluginErrorDetail::ScannerCandidatePath);
                    pluginDefinition.pluginFileIndex = pluginDefinition.files.size() - 1;
                }
                else
                {
                    multipleDefinitions                  = true;
                    pluginDefinition.identity.identifier = {};
                }
            }
        }
        return ResultPlugin(true);
    }

    template <typename T, typename P>
    static void bubbleSort(T* first, T* last, P predicate)
    {
        if (first >= last)
        {
            return;
        }
        bool doSwap = true;
        while (doSwap)
        {
            doSwap  = false;
            auto p0 = first;
            auto p1 = first + 1;
            while (p1 != last)
            {
                if (predicate(*p1, *p0))
                {
                    swap(*p1, *p0);
                    doSwap = true;
                }
                ++p0;
                ++p1;
            }
        }
    };

    void writeDefinitions(Span<PluginDefinition>& foundDefinitions)
    {
        // Cleanup the last definition if case it's not valid
        if (numDefinitions != 0)
        {
            PluginDefinition& pluginDefinition = definitions[numDefinitions - 1];
            if (pluginDefinition.identity.identifier.isEmpty())
            {
                numDefinitions--;
                definitions[numDefinitions] = {};
            }
        }
        bubbleSort(definitions.begin(), definitions.begin() + numDefinitions,
                   [](const PluginDefinition& a, const PluginDefinition& b)
                   { return a.identity.name.view() < b.identity.name.view(); });

        if (numDefinitions > 0)
        {
            foundDefinitions = {definitions.data(), numDefinitions};
        }
        else
        {
            foundDefinitions = {};
        }
    }
};

SC::ResultPlugin SC::PluginScanner::scanDirectory(StringSpan directory, Span<PluginDefinition> definitions,
                                                  IGrowableBuffer&&       tempFileBuffer,
                                                  Span<PluginDefinition>& foundDefinitions)
{
    ScannerState scannerState = {definitions};

    StringPath pathBuffer;
    if (not pathBuffer.assign(directory))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAssign);

    PluginFileSystemIterator iterator;
    SC_TRY(iterator.init(directory));
    PluginFileSystemIterator::Entry entry;
    bool                            hasEntry = false;
    while (true)
    {
        SC_TRY(iterator.next(entry, hasEntry));
        if (not hasEntry)
            break;
        if (entry.name == SC_NATIVE_STR(".") or entry.name == SC_NATIVE_STR(".."))
        {
            continue; // skip . and ..
        }
        StringPath fullPath = pathBuffer;
        if (not fullPath.append(iterator.pathSeparator) or not fullPath.append(entry.name))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::ScannerCandidatePath);
        if (entry.isDirectory)
        {
            // Immediately recurse to find candidates
            SC_TRY(scannerState.storeTentativePluginFolder(fullPath.view()));
            // Scan subdirectory for .cpp files
            PluginFileSystemIterator subIterator;
            SC_TRY(subIterator.init(fullPath.view()));
            PluginFileSystemIterator::Entry subEntry;
            while (true)
            {
                SC_TRY(subIterator.next(subEntry, hasEntry));
                if (not hasEntry)
                    break;
                if (subEntry.name == SC_NATIVE_STR(".") or subEntry.name == SC_NATIVE_STR(".."))
                {
                    continue; // skip . and ..
                }
                StringPath subFullPath = fullPath;
                if (not subFullPath.append(subIterator.pathSeparator) or not subFullPath.append(subEntry.name))
                    return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::ScannerCandidatePath);
                if (not subEntry.isDirectory and PluginString::endsWith(subEntry.name, SC_NATIVE_STR(".cpp")))
                {
                    // It's a regular file ending with .cpp
                    if (scannerState.multipleDefinitions)
                    {
                        continue;
                    }
                    SC_TRY(scannerState.tryParseCandidate(subFullPath.view(), move(tempFileBuffer)));
                }
            }
        }
    }
    scannerState.writeDefinitions(foundDefinitions);
    return ResultPlugin(true);
}
#if SC_PLATFORM_WINDOWS
struct SC::PluginCompiler::CompilerFinder
{
    struct Version
    {
        unsigned char version[3] = {0};

        uint64_t value() const { return version[0] * 256 * 256 * 256 + version[1] * 256 * 256 + version[2] * 256; }

        bool operator<(const Version other) const { return value() < other.value(); }
    };

    StringPath bestCompiler;
    StringPath bestLinker;
    bool       found = false;
    Version    version, bestVersion;

    ResultPlugin tryFindCompiler(StringSpan base, StringSpan candidate, PluginCompiler& compiler)
    {
        SC_TRY(PluginString::assign(bestCompiler, {base, SC_NATIVE_STR("/"), candidate}));
#if SC_PLATFORM_ARM64
        if (not bestCompiler.append(SC_NATIVE_STR("/bin/Hostarm64/arm64/")))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerFindConfiguration);
#else
#if SC_PLATFORM_64_BIT
        if (not bestCompiler.append(SC_NATIVE_STR("/bin/Hostx64/x64/")))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerFindConfiguration);
#else
        if (not bestCompiler.append(SC_NATIVE_STR("/bin/Hostx64/x86/")))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerFindConfiguration);
#endif
#endif

        if (not bestLinker.assign(bestCompiler.view()) or not bestLinker.append(SC_NATIVE_STR("link.exe")) or
            not bestCompiler.append(SC_NATIVE_STR("cl.exe")))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerFindConfiguration);
        {
            if (PluginFileSystem::existsAndIsFileAbsolute(bestCompiler.view()) and
                PluginFileSystem::existsAndIsFileAbsolute(bestLinker.view()))
            {
                PluginString::Tokenizer tokenizer(candidate);
                int                     idx = 0;
                version                     = {};
                while (tokenizer.next('.'))
                {
                    unsigned char number = 0;
                    if (not PluginString::parseUnsignedByte(tokenizer.component, number) or idx > 2)
                    {
                        continue;
                    }
                    version.version[idx] = number;
                    idx++;
                }
                if (bestVersion < version)
                {
                    bestVersion = version;
                    if (not compiler.compilerPath.assign(bestCompiler.view()) or
                        not compiler.linkerPath.assign(bestLinker.view()))
                        return ResultPlugin(PluginError::PathCapacityExceeded,
                                            PluginErrorDetail::CompilerFindConfiguration);
                    StringPath sysrootInclude;
                    SC_TRY(PluginString::assign(sysrootInclude,
                                                {base, SC_NATIVE_STR("/"), candidate, SC_NATIVE_STR("/include")}));
                    if (not compiler.compilerIncludePaths.push_back(sysrootInclude))
                        return ResultPlugin::withRequiredElements(
                            PluginError::CompilerConfigurationCapacityExceeded,
                            PluginErrorDetail::CompilerFindConfiguration,
                            static_cast<uint32_t>(compiler.compilerIncludePaths.size() + 1));
                    StringPath sysrootLib;
                    StringSpan instructionSet = "x86_64";
                    switch (HostInstructionSet)
                    {
                    case InstructionSet::Intel32: instructionSet = "x86"; break;
                    case InstructionSet::Intel64: instructionSet = "x64"; break;
                    case InstructionSet::ARM64: instructionSet = "arm64"; break;
                    }
                    SC_TRY(PluginString::assign(
                        sysrootLib, {base, SC_NATIVE_STR("/"), candidate, SC_NATIVE_STR("/lib/"), instructionSet}));
                    if (not compiler.compilerLibraryPaths.push_back(sysrootLib))
                        return ResultPlugin::withRequiredElements(
                            PluginError::CompilerConfigurationCapacityExceeded,
                            PluginErrorDetail::CompilerFindConfiguration,
                            static_cast<uint32_t>(compiler.compilerLibraryPaths.size() + 1));
                }
                found = true;
            }
        }
        return ResultPlugin(true);
    }
};
#endif
SC::ResultPlugin SC::PluginCompiler::findBestCompiler(PluginCompiler& compiler)
{
#if SC_PLATFORM_WINDOWS
    // TODO: can we use findLatest in order to avoid finding best compiler version...?
    FixedVector<StringPath, 8> rootPaths;
    SC_TRY(VisualStudioPathFinder().findAll(rootPaths))
    for (auto& base : rootPaths)
        SC_TRY(PluginString::append(base, {SC_NATIVE_STR("/VC/Tools/MSVC")}));

    compiler.type = Type::MicrosoftCompiler;
    CompilerFinder compilerFinder;
    for (const auto& basePath : rootPaths)
    {
        StringSpan               base = basePath.view();
        PluginFileSystemIterator iterator;
        if (not iterator.init(base))
            continue;
        PluginFileSystemIterator::Entry entry;

        bool hasEntry = false;
        while (iterator.next(entry, hasEntry) and hasEntry)
        {
            if (entry.isDirectory and entry.name != SC_NATIVE_STR(".") and entry.name != SC_NATIVE_STR(".."))
            {
                SC_TRY(compilerFinder.tryFindCompiler(base, entry.name, compiler));
                if (compilerFinder.found)
                    break;
            }
        }

        if (compilerFinder.found)
        {
            break;
        }
    }
    if (not compilerFinder.found)
        return ResultPlugin(PluginError::CompilerNotFound, PluginErrorDetail::CompilerFindConfiguration);
#elif SC_PLATFORM_APPLE
    compiler.type = Type::ClangCompiler;
    if (not compiler.compilerPath.assign("clang") or not compiler.linkerPath.assign("clang"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerFindConfiguration);
#elif SC_PLATFORM_LINUX
    compiler.type = Type::GnuCompiler;
    if (not compiler.compilerPath.assign("g++") or not compiler.linkerPath.assign("g++"))
        return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerFindConfiguration);
#endif
    return ResultPlugin(true);
}

SC::ResultPlugin SC::PluginCompiler::compileFile(const PluginDefinition& definition, const PluginSysroot& sysroot,
                                                 const PluginCompilerEnvironment& compilerEnvironment,
                                                 StringSpan sourceFile, StringSpan objectFile,
                                                 Span<char>& standardOutput) const
{
    static constexpr size_t MAX_PROCESS_ARGUMENTS = 32;

    size_t argumentsLengths[MAX_PROCESS_ARGUMENTS];
    size_t numberOfArguments = 0;

    StringSpan::NativeWritable bufferWritable = {buffer};

    StringsArena argumentsArena{bufferWritable,
                                numberOfArguments,
                                {argumentsLengths},
                                Result::Error(PluginResultCategory, PluginError::ArgumentCapacityExceeded)};
    if (not argumentsArena.appendAsSingleString(compilerPath.view()))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
#if SC_PLATFORM_WINDOWS
    for (size_t idx = 0; idx < includePaths.size(); ++idx)
    {
        if (not argumentsArena.appendAsSingleString({L"/I\"", includePaths[idx].view(), L"\""}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }

    if (definition.build.contains("libc"))
    {
        for (size_t idx = 0; idx < compilerIncludePaths.size(); ++idx)
        {
            if (not argumentsArena.appendAsSingleString({L"/I\"", compilerIncludePaths[idx].view(), L"\""}))
                return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
        }

        for (size_t idx = 0; idx < sysroot.includePaths.size(); ++idx)
        {
            if (not argumentsArena.appendAsSingleString({L"/I\"", sysroot.includePaths[idx].view(), L"\""}))
                return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
        }
    }

    if (not argumentsArena.appendAsSingleString({L"/Fo:", objectFile}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    if (not definition.build.contains("libc"))
    {
        if (not argumentsArena.appendMultipleStrings({L"/DSC_INCLUDE_STD_CPP=0"}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }
    if (not definition.build.contains("libc++"))
    {
        if (not argumentsArena.appendMultipleStrings({L"/DSC_PROVIDE_CPP_RUNTIME_SHIMS=1"}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }
    if (not argumentsArena.appendMultipleStrings({L"/std:c++17", L"/GR-", L"/WX", L"/W4", L"/permissive-", L"/GS-",
                                                  L"/Zi", L"/DSC_PLUGIN_LIBRARY=1", L"/D_HAS_EXCEPTIONS=0", L"/nologo",
                                                  L"/c", sourceFile}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
#else
    (void)sysroot;
    for (size_t idx = 0; idx < includePaths.size(); ++idx)
    {
        if (not argumentsArena.appendAsSingleString({"-I", includePaths[idx].view()}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }
    if (not definition.build.contains("libc"))
    {
        if (not argumentsArena.appendMultipleStrings({"-nostdinc", "-nostdinc++", "-fno-stack-protector"}) or
            not argumentsArena.appendMultipleStrings({"-DSC_INCLUDE_STD_CPP=0"}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }
    if (not definition.build.contains("libc++"))
    {
        if (not argumentsArena.appendMultipleStrings({"-DSC_PROVIDE_CPP_RUNTIME_SHIMS=1"}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }
    if (not definition.build.contains("exceptions"))
    {
        if (not argumentsArena.appendMultipleStrings({"-fno-exceptions"}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }
    if (not definition.build.contains("rtti"))
    {
        if (not argumentsArena.appendMultipleStrings({"-fno-rtti"}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }

#if defined(__SANITIZE_ADDRESS__)
    if (not argumentsArena.appendMultipleStrings({"-fsanitize=address,undefined"}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
#if SC_PLATFORM_APPLE
    if (not argumentsArena.appendMultipleStrings({"-fno-sanitize=enum,return,float-divide-by-zero,function,vptr"}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
#endif
#endif

    // This is really important on macOS because otherwise symbols exported on some plugin .dylib that
    // match the signature and assembly content, will be re-used by other plugin.dylib making the first
    // plugin .dylib not re-loadable until the other .dylibs having references to it are unloaded too...
    if (not argumentsArena.appendMultipleStrings({"-fvisibility=hidden", "-fvisibility-inlines-hidden"}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);

    if (not argumentsArena.appendMultipleStrings(
            {"-DSC_PLUGIN_LIBRARY=1", "-std=c++14", "-g", "-c", "-fpic", sourceFile, "-o", objectFile}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
#endif
    if (not sysroot.isysroot.isEmpty())
    {
        if (not argumentsArena.appendMultipleStrings({"-isysroot", sysroot.isysroot.view()}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments);
    }
    if (not PluginCompilerEnvironment::Internal::writeFlags(compilerEnvironment.cFlags, argumentsArena))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerWriteArguments);
    StringSpan arguments[MAX_PROCESS_ARGUMENTS];
    if (not argumentsArena.writeTo(arguments))
        return ResultPlugin::withRequiredElements(PluginError::ArgumentCapacityExceeded,
                                                  PluginErrorDetail::CompilerWriteArguments,
                                                  static_cast<uint32_t>(numberOfArguments));
    Process process;
    if (type == Type::ClangCompiler)
    {
        SC_TRY(process.exec({arguments, numberOfArguments}, Process::StdOut::Inherit(), Process::StdIn::Inherit(),
                            standardOutput));
    }
    else
    {
        SC_TRY(process.exec({arguments, numberOfArguments}, standardOutput));
    }
    if (process.getExitStatus() == 0)
    {
        standardOutput = {};
        return ResultPlugin(true);
    }
    else
    {
        return ResultPlugin::withExitCode(PluginError::CompilerExitedWithFailure,
                                          PluginErrorDetail::CompilerBuildArguments, process.getExitStatus());
    }
}

SC::ResultPlugin SC::PluginCompiler::compile(const PluginDefinition& plugin, const PluginSysroot& sysroot,
                                             const PluginCompilerEnvironment& compilerEnvironment,
                                             Span<char>&                      standardOutput) const
{
    // TODO: Spawn parallel tasks
    StringPath destFile;
    for (auto& file : plugin.files)
    {
        StringSpan outputName = PluginString::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SC_TRY(PluginString::join(destFile, {plugin.directory.view(), outputName}));
        if (not destFile.append(SC_NATIVE_STR(".o")))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerBuildObjectPath);
        SC_TRY(compileFile(plugin, sysroot, compilerEnvironment, file.absolutePath.view(), destFile.view(),
                           standardOutput));
    }
    return ResultPlugin(true);
}

SC::ResultPlugin SC::PluginCompiler::link(const PluginDefinition& definition, const PluginSysroot& sysroot,
                                          const PluginCompilerEnvironment& compilerEnvironment,
                                          StringSpan executablePath, Span<char>& linkerLog) const
{
    static constexpr size_t MAX_PROCESS_ARGUMENTS = 24;

    size_t numberOfStrings = 0;
    size_t stringLengths[MAX_PROCESS_ARGUMENTS];

    StringSpan::NativeWritable bufferWritable = {buffer};

    StringsArena arena = {bufferWritable,
                          numberOfStrings,
                          {stringLengths},
                          Result::Error(PluginResultCategory, PluginError::ArgumentCapacityExceeded)};
    if (not arena.appendAsSingleString({linkerPath.view()}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);

#if SC_PLATFORM_WINDOWS
    (void)(compilerEnvironment);

    if (not definition.build.contains("libc") and not definition.build.contains("libc++"))
    {
        if (not arena.appendMultipleStrings({SC_NATIVE_STR("/NODEFAULTLIB"), SC_NATIVE_STR("/ENTRY:DllMain")}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
    }
    if (not arena.appendMultipleStrings(
            {SC_NATIVE_STR("/nologo"), SC_NATIVE_STR("/DLL"), SC_NATIVE_STR("/DEBUG"), SC_NATIVE_STR("/SAFESEH:NO")}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);

    for (size_t idx = 0; idx < compilerLibraryPaths.size(); ++idx)
    {
        if (not arena.appendAsSingleString({SC_NATIVE_STR("/LIBPATH:"), compilerLibraryPaths[idx].view()}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
    }

    for (size_t idx = 0; idx < sysroot.libraryPaths.size(); ++idx)
    {
        if (not arena.appendAsSingleString({SC_NATIVE_STR("/LIBPATH:"), sysroot.libraryPaths[idx].view()}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
    }

    if (not arena.appendAsSingleString({SC_NATIVE_STR("/LIBPATH:"), PluginString::dirname(executablePath)}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);

    StringSpan exeName = PluginString::basename(executablePath, SC_NATIVE_STR(".exe"));
    if (not arena.appendAsSingleString({exeName, SC_NATIVE_STR(".lib")}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);

#else
    (void)(sysroot);
    if (not arena.appendMultipleStrings({"-fpic"}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);

    if (not sysroot.isysroot.isEmpty())
    {
        if (not arena.appendMultipleStrings({"-isysroot", sysroot.isysroot.view()}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
    }
    if (not PluginCompilerEnvironment::Internal::writeFlags(compilerEnvironment.ldFlags, arena))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerWriteArguments);

    // TODO: Figure out where to link _memcpy & co when using -nostdlib

    if (type == Type::ClangCompiler)
    {
        if (not definition.build.contains("libc++"))
        {
            if (not arena.appendMultipleStrings({"-nostdlib++"}))
                return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
        }
    }

#if SC_PLATFORM_APPLE
    if (not arena.appendMultipleStrings({"-bundle_loader", executablePath, "-bundle"}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
#else
    (void)(executablePath);
    if (not arena.appendMultipleStrings({"-shared", "-Wl,-Bsymbolic-functions"}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
#endif
#if defined(__SANITIZE_ADDRESS__)
    if (not arena.appendMultipleStrings({"-fsanitize=address,undefined"}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
#endif
#endif

    for (auto& file : definition.files)
    {
        const StringSpan outputName = PluginString::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        if (not arena.appendAsSingleString(
                {definition.directory.view(), SC_NATIVE_STR("/"), outputName, SC_NATIVE_STR(".o")}))
            return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
    }

    StringPath destFile;
    SC_TRY(definition.getDynamicLibraryAbsolutePath(destFile));
#if SC_PLATFORM_WINDOWS
    if (not arena.appendAsSingleString({SC_NATIVE_STR("/OUT:"), destFile.view()}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
#else
    if (not arena.appendMultipleStrings({"-o", destFile.view()}))
        return ResultPlugin(PluginError::ArgumentCapacityExceeded, PluginErrorDetail::LinkerBuildArguments);
#endif

    StringSpan       args[MAX_PROCESS_ARGUMENTS];
    Span<StringSpan> argsSpan = {args};
    if (not arena.writeTo(argsSpan))
        return ResultPlugin::withRequiredElements(PluginError::ArgumentCapacityExceeded,
                                                  PluginErrorDetail::LinkerWriteArguments,
                                                  static_cast<uint32_t>(numberOfStrings));
    Process process;
    if (type == Type::ClangCompiler)
    {
        SC_TRY(process.exec({args, numberOfStrings}, Process::StdOut::Inherit(), Process::StdIn::Inherit(), linkerLog));
    }
    else
    {
        SC_TRY(process.exec({args, numberOfStrings}, linkerLog));
    }
    if (process.getExitStatus() == 0)
    {
        linkerLog = {};
        return ResultPlugin(true);
    }
    else
    {
        return ResultPlugin::withExitCode(PluginError::LinkerExitedWithFailure, PluginErrorDetail::LinkerBuildArguments,
                                          process.getExitStatus());
    }
}

SC::PluginDynamicLibrary::PluginDynamicLibrary() : lastLoadTime(PluginNow()) { numReloads = 0; }
SC::ResultPlugin SC::PluginDynamicLibrary::unload(bool releaseDebuggerFiles)
{
    SC_TRY(dynamicLibrary.close());
#if SC_PLATFORM_WINDOWS
    if (releaseDebuggerFiles and Debugger::isDebuggerConnected())
    {
        StringPath pdbFile;
        SC_TRY(definition.getDynamicLibraryPDBAbsolutePath(pdbFile));
        if (PluginFileSystem::existsAndIsFileAbsolute(pdbFile.view()))
        {
            SC_TRY(Debugger::unlockFileFromAllProcesses(pdbFile.view()));
            SC_TRY(Debugger::deleteForcefullyUnlockedFile(pdbFile.view()));
        }
    }
#else
    (void)releaseDebuggerFiles;
#endif
    pluginInit  = nullptr;
    pluginClose = nullptr;

    pluginQueryInterface = nullptr;
    return ResultPlugin(true);
}

SC::ResultPlugin SC::PluginSysroot::findBestSysroot(PluginCompiler::Type compilerType, PluginSysroot& sysroot)
{
#if SC_PLATFORM_WINDOWS
    // TODO: This is clearly semi-hardcoded, and we could get the installed directory by looking at registry
    StringSpan baseDirectory = SC_NATIVE_STR("C:\\Program Files (x86)\\Windows Kits\\10");

    StringPath windowsSdkVersion;
    StringSpan searchPath = SC_NATIVE_STR("C:\\Program Files (x86)\\Windows Kits\\10\\include");

    PluginFileSystemIterator iterator;
    SC_TRY(iterator.init(searchPath));
    PluginFileSystemIterator::Entry entry;
    bool                            hasEntry = false;
    while (iterator.next(entry, hasEntry) and hasEntry)
    {
        if (entry.isDirectory and entry.name != SC_NATIVE_STR(".") and entry.name != SC_NATIVE_STR(".."))
        {
            if (not windowsSdkVersion.assign(entry.name))
                return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::SysrootFindVersion);
            break;
        }
    }

    if (windowsSdkVersion.isEmpty())
        return ResultPlugin(PluginError::SysrootNotFound, PluginErrorDetail::SysrootFindVersion);
    switch (compilerType)
    {
    case PluginCompiler::Type::MicrosoftCompiler: {
        constexpr StringSpan includeDirectories[] = {SC_NATIVE_STR("ucrt"), SC_NATIVE_STR("um"),
                                                     SC_NATIVE_STR("shared"), SC_NATIVE_STR("winrt"),
                                                     SC_NATIVE_STR("cppwinrt")};
        for (StringSpan it : includeDirectories)
        {
            StringPath str;
            SC_TRY(PluginString::assign(
                str, {baseDirectory, SC_NATIVE_STR("\\include\\"), windowsSdkVersion.view(), SC_NATIVE_STR("\\"), it}));
            if (not sysroot.includePaths.push_back(move(str)))
                return ResultPlugin::withRequiredElements(PluginError::SysrootCapacityExceeded,
                                                          PluginErrorDetail::SysrootBuildIncludePath,
                                                          static_cast<uint32_t>(sysroot.includePaths.size() + 1));
        }
        StringSpan instructionSet = "x64";
        switch (HostInstructionSet)
        {
        case InstructionSet::Intel32: instructionSet = "x86"; break;
        case InstructionSet::Intel64: instructionSet = "x64"; break;
        case InstructionSet::ARM64: instructionSet = "arm64"; break;
        }

        constexpr StringSpan libraryDirectories[] = {SC_NATIVE_STR("ucrt"), SC_NATIVE_STR("um")};
        for (StringSpan it : libraryDirectories)
        {
            StringPath str;
            SC_TRY(PluginString::assign(str, {baseDirectory, SC_NATIVE_STR("\\lib\\"), windowsSdkVersion.view(),
                                              SC_NATIVE_STR("\\"), it, SC_NATIVE_STR("\\"), instructionSet}));
            if (not sysroot.libraryPaths.push_back(move(str)))
                return ResultPlugin::withRequiredElements(PluginError::SysrootCapacityExceeded,
                                                          PluginErrorDetail::SysrootBuildLibraryPath,
                                                          static_cast<uint32_t>(sysroot.libraryPaths.size() + 1));
        }
    }
    break;
    default: break;
    }
#else
    (void)compilerType;
    (void)sysroot;
#endif
    return ResultPlugin(true);
}

SC::ResultPlugin SC::PluginDynamicLibrary::load(const PluginCompiler& compiler, const PluginSysroot& sysroot,
                                                StringSpan executablePath)
{
    if (dynamicLibrary.isValid())
        return ResultPlugin(PluginError::DynamicLibraryAlreadyLoaded);
    ProcessEnvironment        environment;
    PluginCompilerEnvironment compilerEnvironment;

    size_t     index;
    StringSpan name;
    if (environment.contains("CFLAGS", &index))
    {
        if (not environment.get(index, name, compilerEnvironment.cFlags))
            return ResultPlugin(PluginError::CompilerEnvironmentReadFailed);
    }
    if (environment.contains("LDFLAGS", &index))
    {
        if (not environment.get(index, name, compilerEnvironment.ldFlags))
            return ResultPlugin(PluginError::CompilerEnvironmentReadFailed);
    }
    lastErrorLog = {};

    Span<char> lastErrorLogCopy = {errorStorage, sizeof(errorStorage) - 1};

    auto deferWrite = MakeDeferred(
        [&]
        {
            if (lastErrorLogCopy.sizeInBytes() > 0)
            {
                const size_t last      = lastErrorLogCopy.sizeInBytes() - 1;
                lastErrorLogCopy[last] = 0;
            }
            lastErrorLog = {lastErrorLogCopy, true, StringEncoding::Ascii};
        });
    SC_TRY(compiler.compile(definition, sysroot, compilerEnvironment, lastErrorLogCopy));
#if SC_PLATFORM_WINDOWS
    ::Sleep(400); // Sometimes file is locked...
#endif
    lastErrorLogCopy = {errorStorage, sizeof(errorStorage) - 1};
    SC_TRY(compiler.link(definition, sysroot, compilerEnvironment, executablePath, lastErrorLogCopy));
    deferWrite.disarm();
    StringPath buffer;
    SC_TRY(definition.getDynamicLibraryAbsolutePath(buffer));
    SC_TRY(dynamicLibrary.load(buffer.view()));

    SC_TRY(PluginString::assign(buffer, {definition.identity.identifier.view(), "Init"}));
    SC_TRY(dynamicLibrary.getSymbol(buffer.view(), pluginInit));
    SC_TRY(PluginString::assign(buffer, {definition.identity.identifier.view(), "Close"}));
    SC_TRY(dynamicLibrary.getSymbol(buffer.view(), pluginClose));
    SC_TRY(PluginString::assign(buffer, {definition.identity.identifier.view(), "QueryInterface"}));
    (void)(dynamicLibrary.getSymbol(buffer.view(), pluginQueryInterface)); // QueryInterface is optional
    numReloads += 1;
    lastLoadTime = PluginNow();
    return ResultPlugin(true);
}

void SC::PluginRegistry::init(Span<PluginDynamicLibrary> librariesStorage)
{
    SC_PLUGIN_ASSERT_RELEASE(close());
    storage   = librariesStorage;
    libraries = {};
}

SC::ResultPlugin SC::PluginRegistry::close()
{
    ResultPlugin result(true);
    for (size_t idx = 0; idx < getNumberOfEntries(); ++idx)
    {
        // TODO: Investigate why releasing debugger PDB handles through Restart Manager can take
        // several seconds when exiting SCExample under VSCode on a Parallels shared folder.
        // Final process shutdown does not need it, but explicit unload/reload still does.
        ResultPlugin res = unloadPlugin(getIdentifierAt(idx).view(), false);
        if (not res)
        {
            // We still want to continue unload all plugins
            result = res;
        }
    }
    return result;
}

SC::ResultPlugin SC::PluginRegistry::replaceDefinitions(Span<PluginDefinition>&& definitions)
{
    FixedVector<PluginIdentifier, 16> librariesToUnload;
    // Unload libraries that have no match in the definitions
    for (PluginDynamicLibrary& item : libraries)
    {
        StringSpan libraryId = item.definition.identity.identifier.view();

        bool found = false;
        for (auto& it : definitions)
        {
            if (it.identity.identifier.view() == libraryId)
            {
                found = true;
                break;
            }
        }
        if (not found)
        {
            if (not librariesToUnload.push_back(item.definition.identity.identifier))
                return ResultPlugin::withRequiredElements(PluginError::RegistryCapacityExceeded,
                                                          PluginErrorDetail::RegistryUnloadList,
                                                          static_cast<uint32_t>(librariesToUnload.size() + 1));
        }
    }

    for (auto& identifier : librariesToUnload)
    {
        SC_TRY(unloadPlugin(identifier.view()));
        PluginDynamicLibrary* plugin = findPlugin(identifier.view());
        if (plugin)
        {
            if (libraries.empty())
            {
                *plugin = {};
            }
            else
            {
                *plugin   = move(libraries[libraries.sizeInElements() - 1]);
                libraries = {libraries.data(), libraries.sizeInElements() - 1};
            }
        }
    }

    for (PluginDefinition& definition : definitions)
    {
        PluginDynamicLibrary pdl;
        pdl.definition               = move(definition);
        PluginDynamicLibrary* plugin = findPlugin(pdl.definition.identity.identifier.view());
        if (plugin == nullptr)
        {
            if (libraries.sizeInElements() >= storage.sizeInElements())
                return ResultPlugin::withRequiredElements(PluginError::RegistryCapacityExceeded,
                                                          PluginErrorDetail::RegistryStoreDefinition,
                                                          static_cast<uint32_t>(libraries.sizeInElements() + 1));
            libraries = {storage.data(), libraries.sizeInElements() + 1};

            libraries[libraries.sizeInElements() - 1] = move(pdl);
        }
    }
    definitions = {};
    return ResultPlugin(true);
}

SC::PluginDynamicLibrary* SC::PluginRegistry::findPlugin(StringSpan identifier)
{
    for (PluginDynamicLibrary& item : libraries)
    {
        if (item.definition.identity.identifier.view() == identifier)
        {
            return &item;
        }
    }
    return nullptr;
}

void SC::PluginRegistry::getPluginsToReloadBecauseOf(StringSpan relativePath, TimeMs tolerance,
                                                     Function<void(const PluginIdentifier&)> onPlugin)
{
    const size_t numberOfPlugins = getNumberOfEntries();
    const bool   reloadAll       = relativePath.isEmpty();
    for (size_t idx = 0; idx < numberOfPlugins; ++idx)
    {
        const PluginDynamicLibrary& library      = getPluginDynamicLibraryAt(idx);
        bool                        shouldReload = false;
        for (const PluginFile& file : library.definition.files)
        {
            StringSpan filePath = file.absolutePath.view();
            if (reloadAll or PluginString::pathEndsWith(filePath, relativePath))
            {
                shouldReload = true;
                break;
            }
        }
        if (shouldReload)
        {
            const int64_t elapsed = PluginNow().milliseconds - library.lastLoadTime.milliseconds;
            if (elapsed > tolerance.milliseconds)
            {
                // Only reload if at least tolerance ms have passed, as sometimes FSEvents on
                // macOS likes to send multiple events that are difficult to filter properly
                onPlugin(getIdentifierAt(idx));
            }
        }
    }
}

SC::ResultPlugin SC::PluginRegistry::loadPlugin(StringSpan identifier, const PluginCompiler& compiler,
                                                const PluginSysroot& sysroot, StringSpan executablePath,
                                                LoadMode loadMode)
{
    PluginDynamicLibrary* res = findPlugin(identifier);
    if (res == nullptr)
        return ResultPlugin(PluginError::PluginNotFound, PluginErrorDetail::RegistryFindPlugin);
    PluginDynamicLibrary& lib = *res;
    if (loadMode == LoadMode::Reload or not lib.dynamicLibrary.isValid())
    {
        // TODO: Shield against circular dependencies
        for (const auto& dependency : lib.definition.dependencies)
        {
            SC_TRY(loadPlugin(dependency.view(), compiler, sysroot, executablePath, LoadMode::Load));
        }
        if (lib.dynamicLibrary.isValid())
        {
            SC_TRY(unloadPlugin(identifier));
        }
        SC_TRY(lib.load(compiler, sysroot, executablePath));
        if (not lib.pluginInit(lib.instance))
            return ResultPlugin(PluginError::PluginInitializationFailed);
        return ResultPlugin(true);
    }
    return ResultPlugin(true);
}

SC::ResultPlugin SC::PluginRegistry::unloadPlugin(StringSpan identifier) { return unloadPlugin(identifier, true); }

SC::ResultPlugin SC::PluginRegistry::unloadPlugin(StringSpan identifier, bool releaseDebuggerFiles)
{
    PluginDynamicLibrary* res = findPlugin(identifier);
    if (res == nullptr)
        return ResultPlugin(PluginError::PluginNotFound, PluginErrorDetail::RegistryFindPlugin);
    PluginDynamicLibrary& lib = *res;
    if (lib.dynamicLibrary.isValid())
    {
        for (const PluginDynamicLibrary& dynamicLibrary : libraries)
        {
            // TODO: Shield against circular dependencies
            if (dynamicLibrary.definition.dependencies.contains(identifier))
            {
                SC_TRY(unloadPlugin(dynamicLibrary.definition.identity.identifier.view(), releaseDebuggerFiles));
            }
        }
        auto closeResult = lib.pluginClose(lib.instance);
        lib.instance     = nullptr;
        (void)(closeResult); // TODO: Print / Return some warning
    }
    return lib.unload(releaseDebuggerFiles);
}

SC::ResultPlugin SC::PluginRegistry::removeAllBuildProducts(StringSpan identifier)
{
    PluginDynamicLibrary* res = findPlugin(identifier);
    if (res == nullptr)
        return ResultPlugin(PluginError::PluginNotFound, PluginErrorDetail::RegistryFindPlugin);
    PluginDynamicLibrary& lib = *res;
    StringPath            buffer;

#if SC_PLATFORM_WINDOWS
    SC_TRY(PluginString::assign(buffer, {lib.definition.directory.view(), SC_NATIVE_STR("/"), identifier, ".lib"}));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
    SC_TRY(PluginString::assign(buffer, {lib.definition.directory.view(), SC_NATIVE_STR("/"), identifier, ".exp"}));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
    SC_TRY(PluginString::assign(buffer, {lib.definition.directory.view(), SC_NATIVE_STR("/"), identifier, ".ilk"}));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
    SC_TRY(PluginString::assign(buffer, {lib.definition.directory.view(), SC_NATIVE_STR("/"), identifier, ".dll"}));
    int numTries = 10;
    while (not PluginFileSystem::removeFileAbsolute(buffer.view()))
    {
        ::Sleep(10); // It looks like FreeLibrary needs some time to avoid getting access denied
        numTries--;
        if (numTries < 0)
            return ResultPlugin(PluginError::FileRemoveFailed, PluginErrorDetail::WindowsFileDelete);
    }
#elif SC_PLATFORM_APPLE
    SC_TRY(PluginString::assign(buffer, {lib.definition.directory.view(), "/", identifier, ".dylib"}));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
#else
    SC_TRY(PluginString::assign(buffer, {lib.definition.directory.view(), "/", identifier, ".so"}));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
#endif
    for (auto& file : lib.definition.files)
    {
        StringSpan outputName = PluginString::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));

        StringPath destFile;
        SC_TRY(PluginString::join(destFile, {lib.definition.directory.view(), outputName}));
        if (not destFile.append(".o"))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::PathAppend);
        SC_TRY(PluginFileSystem::removeFileAbsolute(destFile.view()));
    }
    return ResultPlugin(true);
}
