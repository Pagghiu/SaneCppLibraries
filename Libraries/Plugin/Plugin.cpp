// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Plugin.h"

#include "../FileSystem/FileSystem.h"
#include "../FileSystem/Path.h"
#include "../FileSystemIterator/FileSystemIterator.h"
#include "../Process/Process.h"
#include "../Strings/StringBuilder.h"
#include "../Threading/Threading.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/DebuggerWindows.inl"
#endif
#include "Internal/DynamicLibrary.inl"

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
    SC_TRY(fsIterator.init(directory));
    FileSystem fs;
    SC_TRY(fs.init(directory));
    bool multiplePluginDefinitions = false;
    // Iterate each directory at first level and tentatively build a Plugin PluginDefinition.
    // A plugin will be valid if only a single Plugin PluginDefinition will be parsed.
    // Both no plugin definition (identity.identifier.isEmpty()) and multiple
    // contradictory plugin definitions (multiplePluginDefinitions) will prevent creation of the Plugin PluginDefinition
    String file;
    while (fsIterator.enumerateNext())
    {
        const auto& item = fsIterator.get();
        if (item.isDirectory() and item.level == 0) // only recurse first level
        {
            SC_TRY(fsIterator.recurseSubdirectory());
            const StringView pluginDirectory = item.path;
            if (definitions.isEmpty() or not definitions.back().identity.identifier.isEmpty())
            {
                SC_TRY(definitions.resize(definitions.size() + 1));
            }
            definitions.back().files.clear();
            SC_TRY(definitions.back().directory.assign(pluginDirectory));
            multiplePluginDefinitions = false;
        }
        if (item.level == 1 and item.name.endsWith(SC_NATIVE_STR(".cpp")))
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
            SC_TRY(fs.read(item.path, file, StringEncoding::Ascii));
            StringView extracted;
            if (PluginDefinition::find(file.view(), extracted))
            {
                if (PluginDefinition::parse(extracted, pluginDefinition))
                {
                    if (pluginDefinition.identity.identifier.isEmpty())
                    {
                        const StringView identifier = Path::basename(pluginDefinition.directory.view(), Path::AsNative);
                        pluginDefinition.identity.identifier = identifier;
                        pluginDefinition.pluginFileIndex     = pluginDefinition.files.size() - 1;
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
    compiler.type               = Type::MicrosoftCompiler;
    constexpr StringView root[] = {L"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC",
                                   L"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC",
                                   L"C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC",
                                   L"C:/Program Files/Microsoft Visual Studio/2022/Preview/VC/Tools/MSVC",
                                   L"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC",
                                   L"C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Tools/MSVC",
                                   L"C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Tools/MSVC",
                                   L"C:/Program Files (x86)/Microsoft Visual Studio/2019/Preview/VC/Tools/MSVC"};

    bool              found = false;
    Version           version, bestVersion;
    StringNative<256> bestCompiler, bestLinker;
    for (const StringView& base : root)
    {
        FileSystemIterator fsIterator;
        if (not fsIterator.init(base))
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

SC::Result SC::PluginCompiler::compileFile(StringView sourceFile, StringView objectFile) const
{
    Process process;

    StringNative<256> includes = StringEncoding::Native;
    StringBuilder     includeBuilder(includes);
#if SC_PLATFORM_WINDOWS
    StringNative<256> destFile = StringEncoding::Native;
    StringBuilder     destBuilder(destFile);
    SC_TRY(destBuilder.append(L"/Fo:"));
    SC_TRY(destBuilder.append(objectFile));
    SC_TRY(includeBuilder.append(L"/I\""));
    SC_TRY(includeBuilder.append(includePath.view()));
    SC_TRY(includeBuilder.append(L"\""));
    SC_TRY(process.launch({compilerPath.view(), includes.view(), destFile.view(), L"/std:c++17",
                           L"/DSC_DISABLE_CONFIG=1", L"/GR-", L"/WX", L"/W4", L"/permissive-", L"/GS-", L"/Zi",
                           L"/DSC_PLUGIN_LIBRARY=1", L"/EHsc-", L"/c", sourceFile}));
#else
    SC_TRY(includeBuilder.append("-I"));
    SC_TRY(includeBuilder.append(includePath.view()));
    SC_TRY(process.launch({compilerPath.view(), "-DSC_DISABLE_CONFIG=1", "-DSC_PLUGIN_LIBRARY=1", "-nostdinc++",
                           "-nostdinc", "-fno-stack-protector", "-std=c++14", includes.view(), "-fno-exceptions",
                           "-fno-rtti", "-g", "-c", "-fpic", sourceFile, "-o", objectFile}));
#endif
    SC_TRY(process.waitForExitSync());
    return Result(process.getExitStatus() == 0);
}

SC::Result SC::PluginCompiler::compile(const PluginDefinition& plugin) const
{
    // TODO: Spawn parallel tasks
    for (auto& file : plugin.files)
    {
        StringView        dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        StringView        outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        StringNative<256> destFile   = StringEncoding::Native;
        SC_TRY(Path::join(destFile, {dirname, outputName}));
        StringBuilder builder(destFile);
        SC_TRY(builder.append(".o"));
        SC_TRY(compileFile(file.absolutePath.view(), destFile.view()));
    }
    return Result(true);
}

SC::Result SC::PluginCompiler::link(const PluginDefinition& definition, StringView executablePath) const
{
    Process process;
    String  destFile = StringEncoding::Native;
    SC_TRY(definition.getDynamicLibraryAbsolutePath(destFile));
    Vector<StringNative<256>> objectFiles;
    SC_TRY(objectFiles.reserve(definition.files.size()));
    for (auto& file : definition.files)
    {
        StringNative<256> objectPath = StringEncoding::Native;
        StringBuilder     b(objectPath);
        StringView        dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        StringView        outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        SC_TRY(b.append(dirname));
        SC_TRY(b.append("/"));
        SC_TRY(b.append(outputName));
        SC_TRY(b.append(".o"));
        SC_TRY(objectFiles.push_back(move(objectPath)));
    }
    SmallVector<StringView, 256> args;
#if SC_PLATFORM_WINDOWS
    StringNative<256> outFile = StringEncoding::Native;
    StringBuilder     outFileBuilder(outFile);
    SC_TRY(outFileBuilder.append(L"/OUT:"));
    SC_TRY(outFileBuilder.append(destFile.view()));

    StringNative<256> libPath = StringEncoding::Native;
    StringBuilder     libPathBuilder(libPath);
    SC_TRY(libPathBuilder.append(L"/LIBPATH:"));
    SC_TRY(libPathBuilder.append(Path::dirname(executablePath, Path::AsNative)));
    StringView exeName = Path::basename(executablePath, ".exe"_u8);

    StringNative<256> libName = StringEncoding::Native;
    StringBuilder     libNameBuilder(libName);
    SC_TRY(libNameBuilder.append(exeName));
    SC_TRY(libNameBuilder.append(L".lib"));

    SC_TRY(args.append({linkerPath.view(), L"/DLL", L"/DEBUG", L"/NODEFAULTLIB", L"/ENTRY:DllMain", "/SAFESEH:NO",
                        libPath.view(), libName.view()}));
    for (auto& obj : objectFiles)
    {
        SC_TRY(args.push_back(obj.view()));
    }
    SC_TRY(args.push_back(outFile.view()));
    SC_COMPILER_UNUSED(executablePath);
#else
#if SC_PLATFORM_APPLE
    SC_TRY(args.append(
        {linkerPath.view(), "-bundle_loader", executablePath, "-bundle", "-fpic", "-nostdlib++", "-nostdlib"}));
#else
    SC_COMPILER_UNUSED(executablePath);
    if (type == Type::ClangCompiler)
    {
        SC_TRY(args.append({linkerPath.view(), "-shared", "-fpic", "-nostdlib++", "-nostdlib"}));
    }
    else
    {
        SC_TRY(args.append({linkerPath.view(), "-shared", "-fpic", "-nostdlib"}));
    }
#endif
    SC_TRY(objectFiles.reserve(definition.files.size()));
    for (auto& obj : objectFiles)
    {
        SC_TRY(args.push_back(obj.view()));
    }
    SC_TRY(args.append({"-o", destFile.view()}));
#endif
    SC_TRY(process.launch(args.toSpanConst()));
    SC_TRY(process.waitForExitSync());
    return Result(process.getExitStatus() == 0);
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
    return Result(true);
}

SC::Result SC::PluginDynamicLibrary::load(const PluginCompiler& compiler, StringView executablePath)
{
    SC_TRY_MSG(not dynamicLibrary.isValid(), "Dynamic Library must be unloaded first");
    SC_TRY(compiler.compile(definition));
#if SC_PLATFORM_WINDOWS
    Thread::Sleep(400); // Sometimes file is locked...
    SC_TRY(compiler.link(definition, executablePath));
#else
    SC_TRY(compiler.link(definition, executablePath));
#endif

    StringNative<256> buffer;
    SC_TRY(definition.getDynamicLibraryAbsolutePath(buffer));
    SC_TRY(dynamicLibrary.load(buffer.view()));

    SC_TRY(StringBuilder(buffer).format("{}Init", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginInit), "Missing #PluginName#Init");
    SC_TRY(StringBuilder(buffer).format("{}Close", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginClose), "Missing #PluginName#Close");
    return Result(true);
}

SC::Result SC::PluginRegistry::init(Vector<PluginDefinition>&& definitions)
{
    for (auto& definition : definitions)
    {
        PluginDynamicLibrary pdl;
        pdl.definition = move(definition);
        SC_TRY(libraries.insertIfNotExists({pdl.definition.identity.identifier, move(pdl)}));
    }
    definitions.clear();
    return Result(true);
}

const SC::PluginDynamicLibrary* SC::PluginRegistry::findPlugin(const StringView identifier)
{
    auto result = libraries.get(identifier);
    return result ? result : nullptr;
}

SC::Result SC::PluginRegistry::loadPlugin(const StringView identifier, const PluginCompiler& compiler,
                                          StringView executablePath, LoadMode loadMode)
{
    PluginDynamicLibrary* res = libraries.get(identifier);
    SC_TRY(res != nullptr);
    PluginDynamicLibrary& lib = *res;
    if (loadMode == LoadMode::Reload or not lib.dynamicLibrary.isValid())
    {
        // TODO: Shield against circular dependencies
        for (const auto& dependency : lib.definition.dependencies)
        {
            SC_TRY(loadPlugin(dependency.view(), compiler, executablePath, LoadMode::Load));
        }
        if (lib.dynamicLibrary.isValid())
        {
            SC_TRY(unloadPlugin(identifier));
        }
        SC_TRY(lib.load(compiler, executablePath));
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
    StringNative<255> buffer;
    StringBuilder     fmt(buffer);
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
        StringView        dirname    = Path::dirname(file.absolutePath.view(), Path::AsNative);
        StringView        outputName = Path::basename(file.absolutePath.view(), SC_NATIVE_STR(".cpp"));
        StringNative<256> destFile   = StringEncoding::Native;
        SC_TRY(Path::join(destFile, {dirname, outputName}));
        StringBuilder builder(destFile);
        SC_TRY(builder.append(".o"));
        SC_TRY(fs.removeFile(destFile.view()));
    }
    return Result(true);
}
