// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Plugin.h"

#include "../Algorithms/AlgorithmBubbleSort.h"
#include "../FileSystem/FileSystem.h"
#include "../FileSystemIterator/FileSystemIterator.h"
#include "../Process/Internal/StringsArena.h"
#include "../Process/Process.h"
#include "../Strings/Path.h"
#include "../Strings/StringBuilder.h"

#if SC_PLATFORM_WINDOWS
#include "../Threading/Threading.h"
#include "Internal/DebuggerWindows.inl"
#include "Internal/VisualStudioPathFinder.h"
#endif
#include "Internal/DynamicLibrary.inl"

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

SC::Result SC::PluginDefinition::getDynamicLibraryAbsolutePath(String& fullDynamicPath) const
{
    SC_TRY(Path::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
    StringBuilder builder(fullDynamicPath);
#if SC_PLATFORM_WINDOWS
    SC_TRY(builder.append(".dll"));
#elif SC_PLATFORM_APPLE
    SC_TRY(builder.append(".dylib"));
#else
    SC_TRY(builder.append(".so"));
#endif
    return Result(true);
}

SC::Result SC::PluginDefinition::getDynamicLibraryPDBAbsolutePath(String& fullDynamicPath) const
{
    SC_TRY(Path::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
    StringBuilder builder(fullDynamicPath);
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
            gotFields[0]                   = true;
            pluginDefinition.identity.name = value;
        }
        else if (key == "Version")
        {
            gotFields[1]                      = true;
            pluginDefinition.identity.version = value;
        }
        else if (key == "Description")
        {
            gotFields[2]                 = true;
            pluginDefinition.description = value;
        }
        else if (key == "Category")
        {
            gotFields[3]              = true;
            pluginDefinition.category = value;
        }
        else if (key == "Dependencies") // Optional
        {
            StringViewTokenizer tokenizer = value;
            while (tokenizer.tokenizeNext(',', StringViewTokenizer::SkipEmpty))
            {
                SC_TRY(pluginDefinition.dependencies.push_back(tokenizer.component));
            }
        }
        else if (key == "Build") // Optional
        {
            StringViewTokenizer tokenizer = value;
            while (tokenizer.tokenizeNext(',', StringViewTokenizer::SkipEmpty))
            {
                SC_TRY(pluginDefinition.build.push_back(tokenizer.component));
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

SC::Result SC::PluginScanner::scanDirectory(const StringView directory, Vector<PluginDefinition>& definitions)
{
    FileSystemIterator fsIterator;
    fsIterator.options.recursive = false; // Manually recurse only first level dirs
    FileSystemIterator::FolderState recurseStack[16];
    SC_TRY(fsIterator.init(directory, recurseStack));
    FileSystem fs;
    SC_TRY(fs.init(directory));
    bool multiplePluginDefinitions = false;
    // Iterate each directory at first level and tentatively build a Plugin PluginDefinition.
    // A plugin will be valid if only a single Plugin PluginDefinition will be parsed.
    // Both no plugin definition (identity.identifier.isEmpty()) and multiple
    // contradictory plugin definitions (multiplePluginDefinitions) will prevent creation of the Plugin PluginDefinition
    String file = StringEncoding::Ascii;
    while (fsIterator.enumerateNext())
    {
        const auto& item = fsIterator.get();
        if (item.isDirectory() and item.level == 0) // only recurse first level
        {
            SC_TRY(fsIterator.recurseSubdirectory());
            const StringView pluginDirectory = item.path;
            if (definitions.isEmpty() or not definitions.back().identity.identifier.isEmpty())
            {
                SC_TRY(definitions.push_back({}));
            }
            definitions.back().files.clear();
            SC_TRY(definitions.back().directory.assign(pluginDirectory));
            multiplePluginDefinitions = false;
        }
        if (item.level == 1 and StringView(item.name).endsWith(SC_NATIVE_STR(".cpp")))
        {
            if (multiplePluginDefinitions)
            {
                continue;
            }
            PluginDefinition& pluginDefinition = definitions.back();
            {
                PluginFile pluginFile;
                SC_TRY(pluginFile.absolutePath.assign(item.path));
                SC_TRY(pluginDefinition.files.push_back(move(pluginFile)));
            }
            SC_TRY(fs.read(item.path, file));
            StringView extracted;
            if (PluginDefinition::find(file.view(), extracted))
            {
                if (PluginDefinition::parse(extracted, pluginDefinition))
                {
                    if (pluginDefinition.identity.identifier.isEmpty())
                    {
                        const StringView identifier = Path::basename(pluginDefinition.directory.view(), Path::AsNative);
                        SC_TRY(StringConverter(pluginDefinition.identity.identifier).appendNullTerminated(identifier));
                        pluginDefinition.pluginFileIndex = pluginDefinition.files.size() - 1;
                    }
                    else
                    {
                        multiplePluginDefinitions            = true;
                        pluginDefinition.identity.identifier = StringView();
                    }
                }
            }
        }
    }
    // Cleanup the last definition if case it's not valid
    if (not definitions.isEmpty())
    {
        if (definitions.back().identity.identifier.isEmpty())
        {
            SC_TRY(definitions.pop_back());
        }
    }
    Algorithms::bubbleSort(definitions.begin(), definitions.end(),
                           [](const PluginDefinition& a, const PluginDefinition& b)
                           { return a.identity.name.view() < b.identity.name.view(); });
    return fsIterator.checkErrors();
}

SC::Result SC::PluginCompiler::findBestCompiler(PluginCompiler& compiler)
{
    struct Version
    {
        unsigned char version[3] = {0};

        uint64_t value() const { return version[0] * 256 * 256 * 256 + version[1] * 256 * 256 + version[2] * 256; }

        bool operator<(const Version other) const { return value() < other.value(); }
    };
#if SC_PLATFORM_WINDOWS
    // TODO: can we use findLatest in order to avoid finding best compiler version...?
    Vector<String> rootPaths;
    SC_TRY(VisualStudioPathFinder().findAll(rootPaths))
    for (String& base : rootPaths)
        (void)Path::join(base, {base.view(), "VC", "Tools", "MSVC"});

    compiler.type = Type::MicrosoftCompiler;
    bool    found = false;
    Version version, bestVersion;

    SmallStringNative<256> bestCompiler, bestLinker;
    for (const String& basePath : rootPaths)
    {
        FileSystemIterator::FolderState recurseStack[16];
        FileSystemIterator              fsIterator;
        StringView                      base = basePath.view();
        if (not fsIterator.init(base, recurseStack))
            continue;
        while (fsIterator.enumerateNext())
        {
            if (fsIterator.get().isDirectory())
            {
                const StringView candidate = fsIterator.get().name;
                StringBuilder    compilerBuilder(bestCompiler, StringBuilder::Clear);
                SC_TRY(compilerBuilder.append(base));
                SC_TRY(compilerBuilder.append(L"/"));
                SC_TRY(compilerBuilder.append(candidate));
#if SC_PLATFORM_ARM64
                SC_TRY(compilerBuilder.append(L"/bin/Hostarm64/arm64/"));
#else
#if SC_PLATFORM_64_BIT
                SC_TRY(compilerBuilder.append(L"/bin/Hostx64/x64/"));
#else
                SC_TRY(compilerBuilder.append(L"/bin/Hostx64/x86/"));
#endif
#endif
                SC_TRY(bestLinker.assign(bestCompiler.view()));
                StringBuilder linkerBuilder(bestLinker);
                SC_TRY(linkerBuilder.append(L"link.exe"));
                SC_TRY(compilerBuilder.append(L"cl.exe"));
                FileSystem fs;
                if (fs.init(base))
                {
                    if (fs.existsAndIsFile(bestCompiler.view()) and fs.existsAndIsFile(bestLinker.view()))
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
                            version.version[idx] = static_cast<char>(number);
                            idx++;
                        }
                        if (bestVersion < version)
                        {
                            bestVersion = version;
                            SC_TRY(compiler.compilerPath.assign(bestCompiler.view()));
                            SC_TRY(compiler.linkerPath.assign(bestLinker.view()));
                            String sysrootInclude;
                            SC_TRY(StringBuilder(sysrootInclude).format("{0}/{1}/include", base, candidate));
                            SC_TRY(compiler.compilerIncludePaths.push_back(sysrootInclude));
                            String     sysrootLib;
                            StringView instructionSet = "x86_64";
                            switch (HostInstructionSet)
                            {
                            case InstructionSet::Intel32: instructionSet = "x86"; break;
                            case InstructionSet::Intel64: instructionSet = "x64"; break;
                            case InstructionSet::ARM64: instructionSet = "arm64"; break;
                            }
                            SC_TRY(
                                StringBuilder(sysrootLib).format("{0}/{1}/lib/{2}", base, candidate, instructionSet));
                            SC_TRY(compiler.compilerLibraryPaths.push_back(sysrootLib));
                        }
                        found = true;
                    }
                }
            }
        }
        if (found)
        {
            break;
        }
    }
    if (not found)
    {
        return Result::Error("Visual Studio PluginCompiler not found");
    }
#elif SC_PLATFORM_APPLE
    compiler.type         = Type::ClangCompiler;
    compiler.compilerPath = "clang"_a8;
    compiler.linkerPath   = "clang"_a8;
#elif SC_PLATFORM_LINUX
    compiler.type         = Type::GnuCompiler;
    compiler.compilerPath = "g++"_a8;
    compiler.linkerPath   = "g++"_a8;
#endif
    return Result(true);
}

SC::Result SC::PluginCompiler::compileFile(const PluginDefinition& definition, const PluginSysroot& sysroot,
                                           const PluginCompilerEnvironment& compilerEnvironment, StringView sourceFile,
                                           StringView objectFile, String& standardOutput) const
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
        StringBuilder(standardOutput, StringBuilder::Clear);
        return Result(true);
    }
    else
    {
        return Result::Error("Plugin::compileFile failed");
    }
}

SC::Result SC::PluginCompiler::compile(const PluginDefinition& plugin, const PluginSysroot& sysroot,
                                       const PluginCompilerEnvironment& compilerEnvironment,
                                       String&                          standardOutput) const
{
    // TODO: Spawn parallel tasks
    SmallStringNative<256> destFile = StringEncoding::Native;
    for (auto& file : plugin.files)
    {
        StringView dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        StringView outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SC_TRY(Path::join(destFile, {dirname, outputName}));
        StringBuilder builder(destFile);
        SC_TRY(builder.append(SC_NATIVE_STR(".o")));
        SC_TRY(compileFile(plugin, sysroot, compilerEnvironment, file.absolutePath.view(), destFile.view(),
                           standardOutput));
    }
    return Result(true);
}

SC::Result SC::PluginCompiler::link(const PluginDefinition& definition, const PluginSysroot& sysroot,
                                    const PluginCompilerEnvironment& compilerEnvironment, StringView executablePath,
                                    String& linkerLog) const
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

    String destFile = StringEncoding::Native;
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
        SC_TRY_MSG(process.exec({args, numberOfStrings}, linkerLog), "Process lin exec failed");
    }
    if (process.getExitStatus() == 0)
    {
        StringBuilder(linkerLog, StringBuilder::Clear);
        return Result(true);
    }
    else
    {
        return Result::Error("Plugin::link failed");
    }
}

SC::Result SC::PluginDynamicLibrary::unload()
{
    SC_TRY(dynamicLibrary.close());
#if SC_PLATFORM_WINDOWS
    if (Debugger::isDebuggerConnected())
    {
        SmallString<256> pdbFile = StringEncoding::Native;
        SC_TRY(definition.getDynamicLibraryPDBAbsolutePath(pdbFile));
        FileSystem fs;
        if (fs.existsAndIsFile(pdbFile.view()))
        {
            SC_TRY(Debugger::unlockFileFromAllProcesses(pdbFile.view()));
            SC_TRY(Debugger::deleteForcefullyUnlockedFile(pdbFile.view()))
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

    FileSystemIterator::FolderState recurseStack[16];

    FileSystemIterator fsIterator;
    SC_TRY_MSG(fsIterator.init(baseInclude, recurseStack), "Missing Windows Kits 10 directory");
    String windowsSdkVersion;
    while (fsIterator.enumerateNext())
    {
        if (fsIterator.get().isDirectory())
        {
            SC_TRY(windowsSdkVersion.assign(fsIterator.get().name));
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
            String str;
            SC_TRY(StringBuilder(str).format("{0}\\include\\{1}\\{2}", baseDirectory, windowsSdkVersion, it));
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
            String str;
            SC_TRY(StringBuilder(str).format("{0}\\lib\\{1}\\{2}\\{3}", baseDirectory, windowsSdkVersion, it,
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
    size_t                    index;
    StringView                name;
    PluginCompilerEnvironment compilerEnvironment;
    if (environment.contains("CFLAGS", &index))
    {
        SC_TRY(environment.get(index, name, compilerEnvironment.cFlags));
    }
    if (environment.contains("LDFLAGS", &index))
    {
        SC_TRY(environment.get(index, name, compilerEnvironment.ldFlags));
    }
    StringBuilder(lastErrorLog, StringBuilder::Clear);
    SC_TRY_MSG(compiler.compile(definition, sysroot, compilerEnvironment, lastErrorLog), "Compile failed");
#if SC_PLATFORM_WINDOWS
    Thread::Sleep(400); // Sometimes file is locked...
#endif
    SC_TRY_MSG(compiler.link(definition, sysroot, compilerEnvironment, executablePath, lastErrorLog), "Link failes");

    SmallStringNative<256> buffer;
    SC_TRY(definition.getDynamicLibraryAbsolutePath(buffer));
    SC_TRY(dynamicLibrary.load(buffer.view()));

    SC_TRY(StringBuilder(buffer).format("{}Init", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginInit), "Missing #PluginName#Init");
    SC_TRY(StringBuilder(buffer).format("{}Close", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginClose), "Missing #PluginName#Close");
    SC_TRY(StringBuilder(buffer).format("{}QueryInterface", definition.identity.identifier.view()));
    SC_COMPILER_UNUSED(dynamicLibrary.getSymbol(buffer.view(), pluginQueryInterface)); // QueryInterface is optional
    numReloads += 1;
    lastLoadTime = Time::Realtime::now();
    return Result(true);
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

SC::Result SC::PluginRegistry::replaceDefinitions(Vector<PluginDefinition>&& definitions)
{
    SmallVector<String, 16> librariesToUnload;
    // Unload libraries that have no match in the definitions
    for (auto& item : libraries.items)
    {
        StringView libraryId = item.key.view();
        if (not definitions.find([&](const PluginDefinition& it)
                                 { return it.identity.identifier.view() == libraryId; }))
        {
            SC_TRY(librariesToUnload.push_back(libraryId));
        }
    }

    for (String& identifier : librariesToUnload)
    {
        SC_TRY(unloadPlugin(identifier.view()));
        SC_TRY(libraries.remove(identifier));
    }

    for (PluginDefinition& definition : definitions)
    {
        PluginDynamicLibrary pdl;
        pdl.definition = move(definition);
        // If the plugin already exists, it's fine, there's no need to return error
        (void)libraries.insertIfNotExists({pdl.definition.identity.identifier, move(pdl)});
    }
    definitions.clear();
    return Result(true);
}

const SC::PluginDynamicLibrary* SC::PluginRegistry::findPlugin(const StringView identifier)
{
    return libraries.get(identifier);
}

void SC::PluginRegistry::getPluginsToReloadBecauseOf(StringView relativePath, Time::Milliseconds tolerance,
                                                     Function<void(const PluginIdentifier&)> onPlugin)
{
    const size_t numberOfPlugins = getNumberOfEntries();
    for (size_t idx = 0; idx < numberOfPlugins; ++idx)
    {
        const PluginDynamicLibrary& library = getPluginDynamicLibraryAt(idx);
        for (const PluginFile& file : library.definition.files)
        {
            if (file.absolutePath.view().endsWith(relativePath))
            {
                const Time::Milliseconds elapsed = Time::Realtime::now().subtractExact(library.lastLoadTime);
                if (elapsed > tolerance)
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
    PluginDynamicLibrary* res = libraries.get(identifier);
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
    PluginDynamicLibrary* res = libraries.get(identifier);
    SC_TRY(res != nullptr);
    PluginDynamicLibrary& lib = *res;
    if (lib.dynamicLibrary.isValid())
    {
        for (const auto& kv : libraries)
        {
            // TODO: Shield against circular dependencies
            if (kv.value.definition.dependencies.contains(identifier))
            {
                SC_TRY(unloadPlugin(kv.key.view()));
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
    PluginDynamicLibrary* res = libraries.get(identifier);
    SC_TRY(res != nullptr);
    PluginDynamicLibrary& lib = *res;
    FileSystem            fs;
    SC_TRY(fs.init(lib.definition.directory.view()));
    SmallStringNative<255> buffer;
    StringBuilder          fmt(buffer);
#if SC_PLATFORM_WINDOWS
    SC_TRY(fmt.format("{}.lib", identifier));
    SC_TRY(fs.removeFile(buffer.view()));
    SC_TRY(fmt.format("{}.exp", identifier));
    SC_TRY(fs.removeFile(buffer.view()));
    SC_TRY(fmt.format("{}.ilk", identifier));
    SC_TRY(fs.removeFile(buffer.view()));
    SC_TRY(fmt.format("{}.dll", identifier));
    int numTries = 10;
    while (not fs.removeFile(buffer.view()))
    {
        Thread::Sleep(10); // It looks like FreeLibrary needs some time to avoid getting access denied
        numTries--;
        SC_TRY_MSG(numTries >= 0, "PluginRegistry: Cannot remove dll");
    }
#elif SC_PLATFORM_APPLE
    SC_TRY(fmt.format("{}.dylib", identifier));
    SC_TRY(fs.removeFile(buffer.view()));
#else
    SC_TRY(fmt.format("{}.so", identifier));
    SC_TRY(fs.removeFile(buffer.view()));
#endif
    for (auto& file : lib.definition.files)
    {
        StringView             dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        StringView             outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SmallStringNative<256> destFile   = StringEncoding::Native;
        SC_TRY(Path::join(destFile, {dirname, outputName}));
        StringBuilder builder(destFile);
        SC_TRY(builder.append(".o"));
        SC_TRY(fs.removeFile(destFile.view()));
    }
    return Result(true);
}
