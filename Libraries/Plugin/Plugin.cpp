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
            SC_TRY(arena.appendAsSingleString(tokenizer.component));
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

SC::Result SC::PluginDefinition::getDynamicLibraryAbsolutePath(StringPath& fullDynamicPath) const
{
    SC_TRY(PluginString::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
#if SC_PLATFORM_WINDOWS
    SC_TRY(fullDynamicPath.append(".dll"));
#elif SC_PLATFORM_APPLE
    SC_TRY(fullDynamicPath.append(".dylib"));
#else
    SC_TRY(fullDynamicPath.append(".so"));
#endif
    return Result(true);
}

SC::Result SC::PluginDefinition::getDynamicLibraryPDBAbsolutePath(StringPath& fullDynamicPath) const
{
    SC_TRY(PluginString::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
#if SC_PLATFORM_WINDOWS
    SC_TRY(fullDynamicPath.append(".pdb"));
#elif SC_PLATFORM_APPLE
    SC_TRY(fullDynamicPath.append(".dSYM"));
#else
    SC_TRY(fullDynamicPath.append(".sym"));
#endif
    return Result(true);
}

bool SC::PluginDefinition::parse(StringSpan text, PluginDefinition& pluginDefinition)
{
    struct Cursor
    {
        StringSpan text;
        size_t     offset = 0;

        bool parseLine(StringSpan& key, StringSpan& value)
        {
            if (text.getEncoding() == StringEncoding::Utf16)
                return false;
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
            SC_TRY_MSG(pluginDefinition.identity.name.assign(value), "Name exceeds fixed size");
        }
        else if (key == "Version")
        {
            gotFields[1] = true;
            SC_TRY_MSG(pluginDefinition.identity.version.assign(value), "Version exceeds fixed size");
        }
        else if (key == "Description")
        {
            gotFields[2] = true;
            SC_TRY_MSG(pluginDefinition.description.assign(value), "Description exceeds fixed size");
        }
        else if (key == "Category")
        {
            gotFields[3] = true;
            SC_TRY_MSG(pluginDefinition.category.assign(value), "Category exceeds fixed size");
        }
        else if (key == "Dependencies") // Optional
        {
            PluginString::Tokenizer tokenizer(value);
            while (tokenizer.next(','))
            {
                PluginIdentifier identifier;
                SC_TRY(identifier.assign(tokenizer.component));
                SC_TRY(pluginDefinition.dependencies.push_back(move(identifier)));
            }
        }
        else if (key == "Build") // Optional
        {
            PluginString::Tokenizer tokenizer(value);
            while (tokenizer.next(','))
            {
                PluginBuildOption option;
                SC_TRY_MSG(option.assign(tokenizer.component), "Build option exceeds fixed size");
                SC_TRY(pluginDefinition.build.push_back(option));
            }
        }
    }
    for (size_t i = 0; i < sizeof(gotFields) / sizeof(bool); ++i)
    {
        if (not gotFields[i])
            return false;
    }
    return true;
}

struct SC::PluginScanner::ScannerState
{
    Span<PluginDefinition> definitions;

    size_t numDefinitions      = 0;
    bool   multipleDefinitions = false;

    Result storeTentativePluginFolder(StringSpan pluginDirectory)
    {
        if (numDefinitions == 0 or not definitions[numDefinitions - 1].identity.identifier.isEmpty())
        {
            if (numDefinitions >= definitions.sizeInElements())
                return Result::Error("Insufficient size of PluginDefinitions span");
            numDefinitions++;
            definitions[numDefinitions - 1] = {};
        }
        PluginDefinition& pluginDefinition = definitions[numDefinitions - 1];
        pluginDefinition.files.clear();
        SC_TRY(pluginDefinition.directory.assign(pluginDirectory));
        multipleDefinitions = false;
        return Result(true);
    }

    Result tryParseCandidate(StringSpan candidate, IGrowableBuffer&& tempFileBuffer)
    {
        PluginDefinition& pluginDefinition = definitions[numDefinitions - 1];
        {
            PluginFile pluginFile;
            SC_TRY(pluginFile.absolutePath.assign(candidate));
            SC_TRY(pluginDefinition.files.push_back(move(pluginFile)));
        }
        SC_TRY(PluginFileSystem::readAbsoluteFile(candidate, move(tempFileBuffer)));
        StringSpan extracted;
        StringSpan fileView = {{tempFileBuffer.data(), tempFileBuffer.size()}, false, StringEncoding::Utf8};
        if (PluginDefinition::find(fileView, extracted))
        {
            if (PluginDefinition::parse(extracted, pluginDefinition))
            {
                if (pluginDefinition.identity.identifier.isEmpty())
                {
                    const StringSpan identifier = PluginString::basename(pluginDefinition.directory.view());
                    SC_TRY(pluginDefinition.identity.identifier.assign(identifier));
                    pluginDefinition.pluginFileIndex = pluginDefinition.files.size() - 1;
                }
                else
                {
                    multipleDefinitions                  = true;
                    pluginDefinition.identity.identifier = {};
                }
            }
        }
        return Result(true);
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

SC::Result SC::PluginScanner::scanDirectory(StringSpan directory, Span<PluginDefinition> definitions,
                                            IGrowableBuffer&& tempFileBuffer, Span<PluginDefinition>& foundDefinitions)
{
    ScannerState scannerState = {definitions};

    StringPath pathBuffer;
    SC_TRY(pathBuffer.assign(directory));

    PluginFileSystemIterator iterator;
    SC_TRY(iterator.init(directory));
    PluginFileSystemIterator::Entry entry;
    while (iterator.next(entry))
    {
        if (entry.name == SC_NATIVE_STR(".") or entry.name == SC_NATIVE_STR(".."))
        {
            continue; // skip . and ..
        }
        StringPath fullPath = pathBuffer;
        SC_TRY(fullPath.append(iterator.pathSeparator));
        SC_TRY(fullPath.append(entry.name));
        if (entry.isDirectory)
        {
            // Immediately recurse to find candidates
            SC_TRY(scannerState.storeTentativePluginFolder(fullPath.view()));
            // Scan subdirectory for .cpp files
            PluginFileSystemIterator subIterator;
            SC_TRY(subIterator.init(fullPath.view()));
            PluginFileSystemIterator::Entry subEntry;
            while (subIterator.next(subEntry))
            {
                if (subEntry.name == SC_NATIVE_STR(".") or subEntry.name == SC_NATIVE_STR(".."))
                {
                    continue; // skip . and ..
                }
                StringPath subFullPath = fullPath;
                SC_TRY(subFullPath.append(subIterator.pathSeparator));
                SC_TRY(subFullPath.append(subEntry.name));
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
    return Result(true);
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

    Result tryFindCompiler(StringSpan base, StringSpan candidate, PluginCompiler& compiler)
    {
        SC_TRY(PluginString::assign(bestCompiler, {base, SC_NATIVE_STR("/"), candidate}));
#if SC_PLATFORM_ARM64
        SC_TRY(bestCompiler.append(SC_NATIVE_STR("/bin/Hostarm64/arm64/")));
#else
#if SC_PLATFORM_64_BIT
        SC_TRY(bestCompiler.append(SC_NATIVE_STR("/bin/Hostx64/x64/")));
#else
        SC_TRY(bestCompiler.append(SC_NATIVE_STR("/bin/Hostx64/x86/")));
#endif
#endif

        SC_TRY(bestLinker.assign(bestCompiler.view()));
        SC_TRY(bestLinker.append(SC_NATIVE_STR("link.exe")));
        SC_TRY(bestCompiler.append(SC_NATIVE_STR("cl.exe")));
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
                    SC_TRY(compiler.compilerPath.assign(bestCompiler.view()));
                    SC_TRY(compiler.linkerPath.assign(bestLinker.view()));
                    StringPath sysrootInclude;
                    SC_TRY(PluginString::assign(sysrootInclude,
                                                {base, SC_NATIVE_STR("/"), candidate, SC_NATIVE_STR("/include")}));
                    SC_TRY(compiler.compilerIncludePaths.push_back(sysrootInclude));
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
                    SC_TRY(compiler.compilerLibraryPaths.push_back(sysrootLib));
                }
                found = true;
            }
        }
        return Result(true);
    }
};
#endif
SC::Result SC::PluginCompiler::findBestCompiler(PluginCompiler& compiler)
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
        while (iterator.next(entry))
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
    {
        return Result::Error("Visual Studio PluginCompiler not found");
    }
#elif SC_PLATFORM_APPLE
    compiler.type = Type::ClangCompiler;
    SC_TRY(compiler.compilerPath.assign("clang"));
    SC_TRY(compiler.linkerPath.assign("clang"));
#elif SC_PLATFORM_LINUX
    compiler.type = Type::GnuCompiler;
    SC_TRY(compiler.compilerPath.assign("g++"));
    SC_TRY(compiler.linkerPath.assign("g++"));
#endif
    return Result(true);
}

SC::Result SC::PluginCompiler::compileFile(const PluginDefinition& definition, const PluginSysroot& sysroot,
                                           const PluginCompilerEnvironment& compilerEnvironment, StringSpan sourceFile,
                                           StringSpan objectFile, Span<char>& standardOutput) const
{
    static constexpr size_t MAX_PROCESS_ARGUMENTS = 32;

    size_t argumentsLengths[MAX_PROCESS_ARGUMENTS];
    size_t numberOfArguments = 0;

    StringSpan::NativeWritable bufferWritable = {buffer};

    StringsArena argumentsArena{bufferWritable, numberOfArguments, {argumentsLengths}};
    SC_TRY(argumentsArena.appendAsSingleString(compilerPath.view()));
#if SC_PLATFORM_WINDOWS
    for (size_t idx = 0; idx < includePaths.size(); ++idx)
    {
        SC_TRY(argumentsArena.appendAsSingleString({L"/I\"", includePaths[idx].view(), L"\""}));
    }

    if (definition.build.contains("libc"))
    {
        for (size_t idx = 0; idx < compilerIncludePaths.size(); ++idx)
        {
            SC_TRY(argumentsArena.appendAsSingleString({L"/I\"", compilerIncludePaths[idx].view(), L"\""}));
        }

        for (size_t idx = 0; idx < sysroot.includePaths.size(); ++idx)
        {
            SC_TRY(argumentsArena.appendAsSingleString({L"/I\"", sysroot.includePaths[idx].view(), L"\""}));
        }
    }

    SC_TRY(argumentsArena.appendAsSingleString({L"/Fo:", objectFile}));
    if (not definition.build.contains("libc"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({L"/DSC_INCLUDE_STD_CPP=0"}));
    }
    if (not definition.build.contains("libc++"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({L"/DSC_PROVIDE_CPP_RUNTIME_SHIMS=1"}));
    }
    SC_TRY(argumentsArena.appendMultipleStrings({L"/std:c++17", L"/GR-", L"/WX", L"/W4", L"/permissive-", L"/GS-",
                                                 L"/Zi", L"/DSC_PLUGIN_LIBRARY=1", L"/D_HAS_EXCEPTIONS=0", L"/nologo",
                                                 L"/c", sourceFile}));
#else
    (void)sysroot;
    for (size_t idx = 0; idx < includePaths.size(); ++idx)
    {
        SC_TRY(argumentsArena.appendAsSingleString({"-I", includePaths[idx].view()}));
    }
    if (not definition.build.contains("libc"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-nostdinc", "-nostdinc++", "-fno-stack-protector"}));
        SC_TRY(argumentsArena.appendMultipleStrings({"-DSC_INCLUDE_STD_CPP=0"}));
    }
    if (not definition.build.contains("libc++"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-DSC_PROVIDE_CPP_RUNTIME_SHIMS=1"}));
    }
    if (not definition.build.contains("exceptions"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-fno-exceptions"}));
    }
    if (not definition.build.contains("rtti"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-fno-rtti"}));
    }

#if defined(__SANITIZE_ADDRESS__)
    SC_TRY(argumentsArena.appendMultipleStrings({"-fsanitize=address,undefined"}));
#if SC_PLATFORM_APPLE
    SC_TRY(argumentsArena.appendMultipleStrings({"-fno-sanitize=enum,return,float-divide-by-zero,function,vptr"}));
#endif
#endif

    // This is really important on macOS because otherwise symbols exported on some plugin .dylib that
    // match the signature and assembly content, will be re-used by other plugin.dylib making the first
    // plugin .dylib not re-loadable until the other .dylibs having references to it are unloaded too...
    SC_TRY(argumentsArena.appendMultipleStrings({"-fvisibility=hidden", "-fvisibility-inlines-hidden"}));

    SC_TRY(argumentsArena.appendMultipleStrings(
        {"-DSC_PLUGIN_LIBRARY=1", "-std=c++14", "-g", "-c", "-fpic", sourceFile, "-o", objectFile}));
#endif
    if (not sysroot.isysroot.isEmpty())
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-isysroot", sysroot.isysroot.view()}));
    }
    SC_TRY_MSG(PluginCompilerEnvironment::Internal::writeFlags(compilerEnvironment.cFlags, argumentsArena),
               "writeFlags");
    StringSpan arguments[MAX_PROCESS_ARGUMENTS];
    SC_TRY_MSG(argumentsArena.writeTo(arguments), "arguments MAX_PROCESS_ARGUMENTS exceeded");
    Process process;
    if (type == Type::ClangCompiler)
    {
        SC_TRY_MSG(process.exec({arguments, numberOfArguments}, Process::StdOut::Inherit(), Process::StdIn::Inherit(),
                                standardOutput),
                   "Process exec failed (clang)");
    }
    else
    {
        SC_TRY_MSG(process.exec({arguments, numberOfArguments}, standardOutput), "Process exec failed");
    }
    if (process.getExitStatus() == 0)
    {
        standardOutput = {};
        return Result(true);
    }
    else
    {
        return Result::Error("Plugin::compileFile failed");
    }
}

SC::Result SC::PluginCompiler::compile(const PluginDefinition& plugin, const PluginSysroot& sysroot,
                                       const PluginCompilerEnvironment& compilerEnvironment,
                                       Span<char>&                      standardOutput) const
{
    // TODO: Spawn parallel tasks
    StringPath destFile;
    for (auto& file : plugin.files)
    {
        StringSpan outputName = PluginString::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SC_TRY(PluginString::join(destFile, {plugin.directory.view(), outputName}));
        SC_TRY(destFile.append(SC_NATIVE_STR(".o")));
        SC_TRY(compileFile(plugin, sysroot, compilerEnvironment, file.absolutePath.view(), destFile.view(),
                           standardOutput));
    }
    return Result(true);
}

SC::Result SC::PluginCompiler::link(const PluginDefinition& definition, const PluginSysroot& sysroot,
                                    const PluginCompilerEnvironment& compilerEnvironment, StringSpan executablePath,
                                    Span<char>& linkerLog) const
{
    static constexpr size_t MAX_PROCESS_ARGUMENTS = 24;

    size_t numberOfStrings = 0;
    size_t stringLengths[MAX_PROCESS_ARGUMENTS];

    StringSpan::NativeWritable bufferWritable = {buffer};

    StringsArena arena = {bufferWritable, numberOfStrings, {stringLengths}};
    SC_TRY_MSG(arena.appendAsSingleString({linkerPath.view()}), "link buffer full");

#if SC_PLATFORM_WINDOWS
    (void)(compilerEnvironment);

    if (not definition.build.contains("libc") and not definition.build.contains("libc++"))
    {
        SC_TRY(arena.appendMultipleStrings({SC_NATIVE_STR("/NODEFAULTLIB"), SC_NATIVE_STR("/ENTRY:DllMain")}));
    }
    SC_TRY(arena.appendMultipleStrings(
        {SC_NATIVE_STR("/nologo"), SC_NATIVE_STR("/DLL"), SC_NATIVE_STR("/DEBUG"), SC_NATIVE_STR("/SAFESEH:NO")}));

    for (size_t idx = 0; idx < compilerLibraryPaths.size(); ++idx)
    {
        SC_TRY(arena.appendAsSingleString({SC_NATIVE_STR("/LIBPATH:"), compilerLibraryPaths[idx].view()}));
    }

    for (size_t idx = 0; idx < sysroot.libraryPaths.size(); ++idx)
    {
        SC_TRY(arena.appendAsSingleString({SC_NATIVE_STR("/LIBPATH:"), sysroot.libraryPaths[idx].view()}));
    }

    SC_TRY(arena.appendAsSingleString({SC_NATIVE_STR("/LIBPATH:"), PluginString::dirname(executablePath)}));

    StringSpan exeName = PluginString::basename(executablePath, SC_NATIVE_STR(".exe"));
    SC_TRY(arena.appendAsSingleString({exeName, SC_NATIVE_STR(".lib")}));

#else
    (void)(sysroot);
    SC_TRY(arena.appendMultipleStrings({"-fpic"}));

    if (not sysroot.isysroot.isEmpty())
    {
        SC_TRY(arena.appendMultipleStrings({"-isysroot", sysroot.isysroot.view()}));
    }
    SC_TRY(PluginCompilerEnvironment::Internal::writeFlags(compilerEnvironment.ldFlags, arena));

    // TODO: Figure out where to link _memcpy & co when using -nostdlib

    if (type == Type::ClangCompiler)
    {
        if (not definition.build.contains("libc++"))
        {
            SC_TRY(arena.appendMultipleStrings({"-nostdlib++"}));
        }
    }

#if SC_PLATFORM_APPLE
    SC_TRY(arena.appendMultipleStrings({"-bundle_loader", executablePath, "-bundle"}));
#else
    (void)(executablePath);
    SC_TRY(arena.appendMultipleStrings({"-shared", "-Wl,-Bsymbolic-functions"}));
#endif
#if defined(__SANITIZE_ADDRESS__)
    SC_TRY(arena.appendMultipleStrings({"-fsanitize=address,undefined"}));
#endif
#endif

    for (auto& file : definition.files)
    {
        const StringSpan outputName = PluginString::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SC_TRY(arena.appendAsSingleString(
            {definition.directory.view(), SC_NATIVE_STR("/"), outputName, SC_NATIVE_STR(".o")}));
    }

    StringPath destFile;
    SC_TRY(definition.getDynamicLibraryAbsolutePath(destFile));
#if SC_PLATFORM_WINDOWS
    SC_TRY(arena.appendAsSingleString({SC_NATIVE_STR("/OUT:"), destFile.view()}));
#else
    SC_TRY(arena.appendMultipleStrings({"-o", destFile.view()}));
#endif

    StringSpan       args[MAX_PROCESS_ARGUMENTS];
    Span<StringSpan> argsSpan = {args};
    SC_TRY_MSG(arena.writeTo(argsSpan), "Excessive number of arguments");
    Process process;
    if (type == Type::ClangCompiler)
    {
        SC_TRY_MSG(
            process.exec({args, numberOfStrings}, Process::StdOut::Inherit(), Process::StdIn::Inherit(), linkerLog),
            "Process link exec failed (clang)");
    }
    else
    {
        SC_TRY_MSG(process.exec({args, numberOfStrings}, linkerLog), "Process link exec failed");
    }
    if (process.getExitStatus() == 0)
    {
        linkerLog = {};
        return Result(true);
    }
    else
    {
        return Result::Error("Plugin::link failed");
    }
}

SC::PluginDynamicLibrary::PluginDynamicLibrary() : lastLoadTime(PluginNow()) { numReloads = 0; }
SC::Result SC::PluginDynamicLibrary::unload(bool releaseDebuggerFiles)
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
    return Result(true);
}

SC::Result SC::PluginSysroot::findBestSysroot(PluginCompiler::Type compilerType, PluginSysroot& sysroot)
{
#if SC_PLATFORM_WINDOWS
    // TODO: This is clearly semi-hardcoded, and we could get the installed directory by looking at registry
    StringSpan baseDirectory = SC_NATIVE_STR("C:\\Program Files (x86)\\Windows Kits\\10");

    StringPath windowsSdkVersion;
    StringSpan searchPath = SC_NATIVE_STR("C:\\Program Files (x86)\\Windows Kits\\10\\include");

    PluginFileSystemIterator iterator;
    SC_TRY(iterator.init(searchPath));
    PluginFileSystemIterator::Entry entry;
    while (iterator.next(entry))
    {
        if (entry.isDirectory and entry.name != SC_NATIVE_STR(".") and entry.name != SC_NATIVE_STR(".."))
        {
            SC_TRY(windowsSdkVersion.assign(entry.name));
            break;
        }
    }

    SC_TRY_MSG(not windowsSdkVersion.isEmpty(), "Cannot find Windows Kits 10 include directory")
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
            SC_TRY(sysroot.includePaths.push_back(move(str)));
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
            SC_TRY(sysroot.libraryPaths.push_back(move(str)));
        }
    }
    break;
    default: break;
    }
#else
    (void)compilerType;
    (void)sysroot;
#endif
    return Result(true);
}

SC::Result SC::PluginDynamicLibrary::load(const PluginCompiler& compiler, const PluginSysroot& sysroot,
                                          StringSpan executablePath)
{
    SC_TRY_MSG(not dynamicLibrary.isValid(), "Dynamic Library must be unloaded first");
    ProcessEnvironment        environment;
    PluginCompilerEnvironment compilerEnvironment;

    size_t     index;
    StringSpan name;
    if (environment.contains("CFLAGS", &index))
    {
        SC_TRY(environment.get(index, name, compilerEnvironment.cFlags));
    }
    if (environment.contains("LDFLAGS", &index))
    {
        SC_TRY(environment.get(index, name, compilerEnvironment.ldFlags));
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
    SC_TRY_MSG(compiler.compile(definition, sysroot, compilerEnvironment, lastErrorLogCopy), "Compile failed");
#if SC_PLATFORM_WINDOWS
    ::Sleep(400); // Sometimes file is locked...
#endif
    lastErrorLogCopy = {errorStorage, sizeof(errorStorage) - 1};
    SC_TRY_MSG(compiler.link(definition, sysroot, compilerEnvironment, executablePath, lastErrorLogCopy), "Link fails");
    deferWrite.disarm();
    StringPath buffer;
    SC_TRY(definition.getDynamicLibraryAbsolutePath(buffer));
    SC_TRY(dynamicLibrary.load(buffer.view()));

    SC_TRY(PluginString::assign(buffer, {definition.identity.identifier.view(), "Init"}));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginInit), "Missing #PluginName#Init");
    SC_TRY(PluginString::assign(buffer, {definition.identity.identifier.view(), "Close"}));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginClose), "Missing #PluginName#Close");
    SC_TRY(PluginString::assign(buffer, {definition.identity.identifier.view(), "QueryInterface"}));
    (void)(dynamicLibrary.getSymbol(buffer.view(), pluginQueryInterface)); // QueryInterface is optional
    numReloads += 1;
    lastLoadTime = PluginNow();
    return Result(true);
}

void SC::PluginRegistry::init(Span<PluginDynamicLibrary> librariesStorage)
{
    SC_PLUGIN_ASSERT_RELEASE(close());
    storage   = librariesStorage;
    libraries = {};
}

SC::Result SC::PluginRegistry::close()
{
    Result result(true);
    for (size_t idx = 0; idx < getNumberOfEntries(); ++idx)
    {
        // TODO: Investigate why releasing debugger PDB handles through Restart Manager can take
        // several seconds when exiting SCExample under VSCode on a Parallels shared folder.
        // Final process shutdown does not need it, but explicit unload/reload still does.
        Result res = unloadPlugin(getIdentifierAt(idx).view(), false);
        if (not res)
        {
            // We still want to continue unload all plugins
            result = res;
        }
    }
    return result;
}

SC::Result SC::PluginRegistry::replaceDefinitions(Span<PluginDefinition>&& definitions)
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
            SC_TRY(librariesToUnload.push_back(item.definition.identity.identifier));
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
            SC_TRY_MSG(libraries.sizeInElements() < storage.sizeInElements(),
                       "Exceeded number of Plugins storage space");
            libraries = {storage.data(), libraries.sizeInElements() + 1};

            libraries[libraries.sizeInElements() - 1] = move(pdl);
        }
    }
    definitions = {};
    return Result(true);
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

SC::Result SC::PluginRegistry::loadPlugin(StringSpan identifier, const PluginCompiler& compiler,
                                          const PluginSysroot& sysroot, StringSpan executablePath, LoadMode loadMode)
{
    PluginDynamicLibrary* res = findPlugin(identifier);
    SC_TRY_MSG(res != nullptr, "loadplugin res == nullptr");
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
            SC_TRY_MSG(unloadPlugin(identifier), "unload plugin");
        }
        SC_TRY(lib.load(compiler, sysroot, executablePath));
        SC_TRY_MSG(lib.pluginInit(lib.instance), "PluginInit failed"); // TODO: Return actual failure strings
        return Result(true);
    }
    return Result(true);
}

SC::Result SC::PluginRegistry::unloadPlugin(StringSpan identifier) { return unloadPlugin(identifier, true); }

SC::Result SC::PluginRegistry::unloadPlugin(StringSpan identifier, bool releaseDebuggerFiles)
{
    PluginDynamicLibrary* res = findPlugin(identifier);
    SC_TRY(res != nullptr);
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

SC::Result SC::PluginRegistry::removeAllBuildProducts(StringSpan identifier)
{
    PluginDynamicLibrary* res = findPlugin(identifier);
    SC_TRY(res != nullptr);
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
        SC_TRY_MSG(numTries >= 0, "PluginRegistry: Cannot remove dll");
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
        SC_TRY(destFile.append(".o"));
        SC_TRY(PluginFileSystem::removeFileAbsolute(destFile.view()));
    }
    return Result(true);
}
