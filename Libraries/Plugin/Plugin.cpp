// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Plugin.h"

#include "../Foundation/Deferred.h"
#include "../Process/Internal/StringsArena.h"
#include "../Process/Process.h"
#include "../Strings/Path.h"
#include "../Strings/StringBuilder.h"

#include "Internal/PluginFileSystem.h" // This must be included before VisualStudioPathFinder.h for the unity build
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
    [[nodiscard]] static bool writeFlags(const StringView flags, StringsArena& arena)
    {
        StringViewTokenizer tokenizer(flags);
        while (tokenizer.tokenizeNext({' '}))
        {
            SC_TRY(arena.appendAsSingleString(tokenizer.component));
        }
        return true;
    }
};

bool SC::PluginDefinition::find(const StringView text, StringView& extracted)
{
    auto       it          = text.getIterator<StringIteratorASCII>();
    const auto beginPlugin = ("SC_BEGIN_PLUGIN"_a8).getIterator<StringIteratorASCII>();
    SC_TRY(it.advanceAfterFinding(beginPlugin));
    SC_TRY(it.advanceUntilMatches('\n'));
    SC_TRY(it.stepForward());
    auto       start     = it;
    const auto endPlugin = ("SC_END_PLUGIN"_a8).getIterator<StringIteratorASCII>();
    SC_TRY(it.advanceAfterFinding(endPlugin));
    SC_TRY(it.reverseAdvanceUntilMatches('\n'));
    auto end  = it;
    extracted = StringView::fromIterators(start, end);
    return true;
}

SC::Result SC::PluginDefinition::getDynamicLibraryAbsolutePath(StringPath& fullDynamicPath) const
{
    SC_TRY(Path::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
    auto builder = StringBuilder::createForAppendingTo(fullDynamicPath);
#if SC_PLATFORM_WINDOWS
    SC_TRY(builder.append(".dll"));
#elif SC_PLATFORM_APPLE
    SC_TRY(builder.append(".dylib"));
#else
    SC_TRY(builder.append(".so"));
#endif
    return Result(true);
}

SC::Result SC::PluginDefinition::getDynamicLibraryPDBAbsolutePath(StringPath& fullDynamicPath) const
{
    SC_TRY(Path::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
    auto builder = StringBuilder::createForAppendingTo(fullDynamicPath);
#if SC_PLATFORM_WINDOWS
    SC_TRY(builder.append(".pdb"));
#elif SC_PLATFORM_APPLE
    SC_TRY(builder.append(".dSYM"));
#else
    SC_TRY(builder.append(".sym"));
#endif
    return Result(true);
}

bool SC::PluginDefinition::parse(StringView text, PluginDefinition& pluginDefinition)
{
    auto       it = text.getIterator<StringIteratorASCII>();
    StringView key, value;
    bool       gotFields[4] = {false};
    while (parseLine(it, key, value))
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
            StringViewTokenizer tokenizer = value;
            while (tokenizer.tokenizeNext(',', StringViewTokenizer::SkipEmpty))
            {
                PluginIdentifier identifier;
                SC_TRY(identifier.assign(tokenizer.component));
                SC_TRY(pluginDefinition.dependencies.push_back(move(identifier)));
            }
        }
        else if (key == "Build") // Optional
        {
            StringViewTokenizer tokenizer = value;
            while (tokenizer.tokenizeNext(',', StringViewTokenizer::SkipEmpty))
            {
                PluginBuildOption option;
                SC_TRY_MSG(option.assign(tokenizer.component), "Build option exceeds fixed size");
                SC_TRY(pluginDefinition.build.push_back(option));
            }
        }
    }
    for (size_t i = 0; i < sizeof(gotFields) / sizeof(bool); ++i)
    {
        if (!gotFields[i])
            return false;
    }
    return true;
}

bool SC::PluginDefinition::parseLine(StringIteratorASCII& iterator, StringView& key, StringView& value)
{
    constexpr StringIteratorSkipTable skipTable({'\t', '\n', '\r', ' ', '/', ':'});

    StringIteratorASCII::CodePoint current = 0;
    while (iterator.advanceRead(current))
    {
        if (not skipTable.matches[current])
        {
            (void)iterator.stepBackward();
            break;
        }
    }
    auto identifierStart = iterator;
    while (iterator.advanceRead(current))
    {
        if (skipTable.matches[current])
        {
            (void)iterator.stepBackward();
            break;
        }
    }
    auto identifierEnd = iterator;

    key = StringView::fromIterators(identifierStart, identifierEnd);
    if (not iterator.advanceIfMatches(':'))
    {
        return false;
    }
    while (iterator.advanceRead(current))
    {
        if (not skipTable.matches[current])
        {
            (void)iterator.stepBackward();
            break;
        }
    }
    auto valueStart = iterator;
    while (iterator.advanceRead(current))
    {
        if (current == '\n' or current == '\r')
        {
            (void)iterator.stepBackward();
            value = StringView::fromIterators(valueStart, iterator);
            (void)iterator.stepForward();
            return true;
        }
    }
    value = StringView::fromIterators(valueStart, iterator);
    return value.sizeInBytes() > 0;
}

struct SC::PluginScanner::ScannerState
{
    Span<PluginDefinition> definitions;

    size_t numDefinitions      = 0;
    bool   multipleDefinitions = false;

    Result storeTentativePluginFolder(StringView pluginDirectory)
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

    Result tryParseCandidate(StringView candidate, IGrowableBuffer&& tempFileBuffer)
    {
        PluginDefinition& pluginDefinition = definitions[numDefinitions - 1];
        {
            PluginFile pluginFile;
            SC_TRY(pluginFile.absolutePath.assign(candidate));
            SC_TRY(pluginDefinition.files.push_back(move(pluginFile)));
        }
        SC_TRY(PluginFileSystem::readAbsoluteFile(candidate, move(tempFileBuffer)));
        StringView extracted;
        StringView fileView = {{tempFileBuffer.data(), tempFileBuffer.size()}, false, StringEncoding::Utf8};
        if (PluginDefinition::find(fileView, extracted))
        {
            if (PluginDefinition::parse(extracted, pluginDefinition))
            {
                if (pluginDefinition.identity.identifier.isEmpty())
                {
                    const StringView identifier = Path::basename(pluginDefinition.directory.view(), Path::AsNative);
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

SC::Result SC::PluginScanner::scanDirectory(const StringView directory, Span<PluginDefinition> definitions,
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
                if (!subEntry.isDirectory and subEntry.name.endsWith(SC_NATIVE_STR(".cpp")))
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

    Result tryFindCompiler(StringView base, StringView candidate, PluginCompiler& compiler)
    {

        auto compilerBuilder = StringBuilder::create(bestCompiler);
        SC_TRY(compilerBuilder.append(base));
        SC_TRY(compilerBuilder.append(SC_NATIVE_STR("/")));
        SC_TRY(compilerBuilder.append(candidate));
#if SC_PLATFORM_ARM64
        SC_TRY(compilerBuilder.append(SC_NATIVE_STR("/bin/Hostarm64/arm64/")));
#else
#if SC_PLATFORM_64_BIT
        SC_TRY(compilerBuilder.append(SC_NATIVE_STR("/bin/Hostx64/x64/")));
#else
        SC_TRY(compilerBuilder.append(SC_NATIVE_STR("/bin/Hostx64/x86/")));
#endif
#endif
        compilerBuilder.finalize();

        SC_TRY(bestLinker.assign(bestCompiler.view()));
        auto linkerBuilder = StringBuilder::createForAppendingTo(bestLinker);
        SC_TRY(linkerBuilder.append(SC_NATIVE_STR("link.exe")));
        linkerBuilder.finalize();
        auto compilerBuilder2 = StringBuilder::createForAppendingTo(bestCompiler);
        SC_TRY(compilerBuilder2.append(SC_NATIVE_STR("cl.exe")));
        compilerBuilder2.finalize();
        {
            if (PluginFileSystem::existsAndIsFileAbsolute(bestCompiler.view()) and
                PluginFileSystem::existsAndIsFileAbsolute(bestLinker.view()))
            {
                StringViewTokenizer tokenizer(candidate);
                int                 idx = 0;
                while (tokenizer.tokenizeNext('.', StringViewTokenizer::SkipEmpty))
                {
                    int number;
                    if (not tokenizer.component.parseInt32(number) or number < 0 or number > 255 or idx > 2)
                    {
                        continue;
                    }
                    version.version[idx] = static_cast<unsigned char>(number);
                    idx++;
                }
                if (bestVersion < version)
                {
                    bestVersion = version;
                    SC_TRY(compiler.compilerPath.assign(bestCompiler.view()));
                    SC_TRY(compiler.linkerPath.assign(bestLinker.view()));
                    StringPath sysrootInclude;
                    SC_TRY(StringBuilder::format(sysrootInclude, "{0}/{1}/include", base, candidate));
                    SC_TRY(compiler.compilerIncludePaths.push_back(sysrootInclude));
                    StringPath sysrootLib;
                    StringView instructionSet = "x86_64";
                    switch (HostInstructionSet)
                    {
                    case InstructionSet::Intel32: instructionSet = "x86"; break;
                    case InstructionSet::Intel64: instructionSet = "x64"; break;
                    case InstructionSet::ARM64: instructionSet = "arm64"; break;
                    }
                    SC_TRY(StringBuilder::format(sysrootLib, "{0}/{1}/lib/{2}", base, candidate, instructionSet));
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
        (void)Path::join(base, {base.view(), "VC", "Tools", "MSVC"});

    compiler.type = Type::MicrosoftCompiler;
    CompilerFinder compilerFinder;
    for (const auto& basePath : rootPaths)
    {
        StringView               base = basePath.view();
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
                                           const PluginCompilerEnvironment& compilerEnvironment, StringView sourceFile,
                                           StringView objectFile, Span<char>& standardOutput) const
{
    static constexpr size_t MAX_PROCESS_ARGUMENTS = 24;

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
    }
    else if (not definition.build.contains("libc++"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-nostdinc++"}));
    }
    if (not definition.build.contains("exceptions"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-fno-exceptions"}));
    }
    if (not definition.build.contains("rtti"))
    {
        SC_TRY(argumentsArena.appendMultipleStrings({"-fno-rtti"}));
    }

#if SC_COMPILER_ASAN
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
        StringView dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        StringView outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SC_TRY(Path::join(destFile, {dirname, outputName}));
        auto builder = StringBuilder::createForAppendingTo(destFile);
        SC_TRY(builder.append(SC_NATIVE_STR(".o")));
        SC_TRY(compileFile(plugin, sysroot, compilerEnvironment, file.absolutePath.view(), builder.finalize(),
                           standardOutput));
    }
    return Result(true);
}

SC::Result SC::PluginCompiler::link(const PluginDefinition& definition, const PluginSysroot& sysroot,
                                    const PluginCompilerEnvironment& compilerEnvironment, StringView executablePath,
                                    Span<char>& linkerLog) const
{
    static constexpr size_t MAX_PROCESS_ARGUMENTS = 24;

    size_t numberOfStrings = 0;
    size_t stringLengths[MAX_PROCESS_ARGUMENTS];

    StringSpan::NativeWritable bufferWritable = {buffer};

    StringsArena arena = {bufferWritable, numberOfStrings, {stringLengths}};
    SC_TRY_MSG(arena.appendAsSingleString({linkerPath.view()}), "link buffer full");

#if SC_PLATFORM_WINDOWS
    SC_COMPILER_UNUSED(compilerEnvironment);

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

    SC_TRY(arena.appendAsSingleString({SC_NATIVE_STR("/LIBPATH:"), Path::dirname(executablePath, Path::AsNative)}));

    StringView exeName = Path::basename(executablePath, SC_NATIVE_STR(".exe"));
    SC_TRY(arena.appendAsSingleString({exeName, SC_NATIVE_STR(".lib")}));

#else
    SC_COMPILER_UNUSED(sysroot);
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
    SC_COMPILER_UNUSED(executablePath);
    SC_TRY(arena.appendMultipleStrings({"-shared"}));
#endif
#if SC_COMPILER_ASAN
    SC_TRY(arena.appendMultipleStrings({"-fsanitize=address,undefined"}));
#endif
#endif

    for (auto& file : definition.files)
    {
        const StringView dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        const StringView outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SC_TRY(arena.appendAsSingleString({dirname, SC_NATIVE_STR("/"), outputName, SC_NATIVE_STR(".o")}));
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
SC::Result SC::PluginDynamicLibrary::unload()
{
    SC_TRY(dynamicLibrary.close());
#if SC_PLATFORM_WINDOWS
    if (Debugger::isDebuggerConnected())
    {
        StringPath pdbFile;
        SC_TRY(definition.getDynamicLibraryPDBAbsolutePath(pdbFile));
        if (PluginFileSystem::existsAndIsFileAbsolute(pdbFile.view()))
        {
            SC_TRY(Debugger::unlockFileFromAllProcesses(pdbFile.view()));
            SC_TRY(Debugger::deleteForcefullyUnlockedFile(pdbFile.view()));
        }
    }
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
    StringView baseDirectory = SC_NATIVE_STR("C:\\Program Files (x86)\\Windows Kits\\10");
    StringView baseInclude   = SC_NATIVE_STR("C:\\Program Files (x86)\\Windows Kits\\10\\include");

    StringPath windowsSdkVersion;
    StringView searchPath = SC_NATIVE_STR("C:\\Program Files (x86)\\Windows Kits\\10\\include");

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
        for (auto it : {SC_NATIVE_STR("ucrt"), SC_NATIVE_STR("um"), SC_NATIVE_STR("shared"), SC_NATIVE_STR("winrt"),
                        SC_NATIVE_STR("cppwinrt")})
        {
            StringPath str;
            SC_TRY(StringBuilder::format(str, "{0}\\include\\{1}\\{2}", baseDirectory, windowsSdkVersion, it));
            SC_TRY(sysroot.includePaths.push_back(move(str)));
        }
        StringView instructionSet = "x64";
        switch (HostInstructionSet)
        {
        case InstructionSet::Intel32: instructionSet = "x86"; break;
        case InstructionSet::Intel64: instructionSet = "x64"; break;
        case InstructionSet::ARM64: instructionSet = "arm64"; break;
        }

        for (auto it : {SC_NATIVE_STR("ucrt"), SC_NATIVE_STR("um")})
        {
            StringPath str;
            SC_TRY(StringBuilder::format(str, "{0}\\lib\\{1}\\{2}\\{3}", baseDirectory, windowsSdkVersion, it,
                                         instructionSet));
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
                                          StringView executablePath)
{
    SC_TRY_MSG(not dynamicLibrary.isValid(), "Dynamic Library must be unloaded first");
    ProcessEnvironment        environment;
    PluginCompilerEnvironment compilerEnvironment;

    size_t     index;
    StringView name;
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

    SC_TRY(StringBuilder::format(buffer, "{}Init", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginInit), "Missing #PluginName#Init");
    SC_TRY(StringBuilder::format(buffer, "{}Close", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginClose), "Missing #PluginName#Close");
    SC_TRY(StringBuilder::format(buffer, "{}QueryInterface", definition.identity.identifier.view()));
    SC_COMPILER_UNUSED(dynamicLibrary.getSymbol(buffer.view(), pluginQueryInterface)); // QueryInterface is optional
    numReloads += 1;
    lastLoadTime = PluginNow();
    return Result(true);
}

void SC::PluginRegistry::init(Span<PluginDynamicLibrary> librariesStorage)
{
    SC_ASSERT_RELEASE(close());
    storage   = librariesStorage;
    libraries = {};
}

SC::Result SC::PluginRegistry::close()
{
    Result result(true);
    for (size_t idx = 0; idx < getNumberOfEntries(); ++idx)
    {
        Result res = unloadPlugin(getIdentifierAt(idx).view());
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
        StringView libraryId = item.definition.identity.identifier.view();

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

SC::PluginDynamicLibrary* SC::PluginRegistry::findPlugin(const StringView identifier)
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

void SC::PluginRegistry::getPluginsToReloadBecauseOf(StringView relativePath, TimeMs tolerance,
                                                     Function<void(const PluginIdentifier&)> onPlugin)
{
    const size_t numberOfPlugins = getNumberOfEntries();
    for (size_t idx = 0; idx < numberOfPlugins; ++idx)
    {
        const PluginDynamicLibrary& library = getPluginDynamicLibraryAt(idx);
        for (const PluginFile& file : library.definition.files)
        {
            StringView filePath = file.absolutePath.view();
            if (filePath.endsWith(relativePath))
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
}

SC::Result SC::PluginRegistry::loadPlugin(const StringView identifier, const PluginCompiler& compiler,
                                          const PluginSysroot& sysroot, StringView executablePath, LoadMode loadMode)
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

SC::Result SC::PluginRegistry::unloadPlugin(const StringView identifier)
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
                SC_TRY(unloadPlugin(dynamicLibrary.definition.identity.identifier.view()));
            }
        }
        auto closeResult = lib.pluginClose(lib.instance);
        lib.instance     = nullptr;
        SC_COMPILER_UNUSED(closeResult); // TODO: Print / Return some warning
    }
    return lib.unload();
}

SC::Result SC::PluginRegistry::removeAllBuildProducts(const StringView identifier)
{
    PluginDynamicLibrary* res = findPlugin(identifier);
    SC_TRY(res != nullptr);
    PluginDynamicLibrary& lib = *res;
    StringPath            buffer;

#if SC_PLATFORM_WINDOWS
    SC_TRY(StringBuilder::format(buffer, "{}/{}{}", lib.definition.directory.view(), identifier, ".lib"));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
    SC_TRY(StringBuilder::format(buffer, "{}/{}{}", lib.definition.directory.view(), identifier, ".exp"));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
    SC_TRY(StringBuilder::format(buffer, "{}/{}{}", lib.definition.directory.view(), identifier, ".ilk"));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
    SC_TRY(StringBuilder::format(buffer, "{}/{}{}", lib.definition.directory.view(), identifier, ".dll"));
    int numTries = 10;
    while (not PluginFileSystem::removeFileAbsolute(buffer.view()))
    {
        ::Sleep(10); // It looks like FreeLibrary needs some time to avoid getting access denied
        numTries--;
        SC_TRY_MSG(numTries >= 0, "PluginRegistry: Cannot remove dll");
    }
#elif SC_PLATFORM_APPLE
    SC_TRY(StringBuilder::format(buffer, "{}/{}{}", lib.definition.directory.view(), identifier, ".dylib"));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
#else
    SC_TRY(StringBuilder::format(buffer, "{}/{}{}", lib.definition.directory.view(), identifier, ".so"));
    SC_TRY(PluginFileSystem::removeFileAbsolute(buffer.view()));
#endif
    for (auto& file : lib.definition.files)
    {
        StringView dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        StringView outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));

        StringPath destFile;
        SC_TRY(Path::join(destFile, {dirname, outputName}));
        auto builder = StringBuilder::createForAppendingTo(destFile);
        SC_TRY(builder.append(".o"));
        SC_TRY(PluginFileSystem::removeFileAbsolute(builder.finalize()));
    }
    return Result(true);
}
