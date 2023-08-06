// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Plugin.h"

#include "../FileSystem/FileSystem.h"
#include "../FileSystem/FileSystemWalker.h"
#include "../FileSystem/Path.h"
#include "../Foundation/StringBuilder.h"
#include "../Process/Process.h"
#include "../Threading/Threading.h"

bool SC::PluginDefinition::find(const StringView text, StringView& extracted)
{
    auto       it           = text.getIterator<StringIteratorASCII>();
    const auto beginPluging = ("SC_BEGIN_PLUGIN"_a8).getIterator<StringIteratorASCII>();
    SC_TRY_IF(it.advanceAfterFinding(beginPluging));
    SC_TRY_IF(it.advanceUntilMatches('\n'));
    SC_TRY_IF(it.stepForward());
    auto       start     = it;
    const auto endPlugin = ("SC_END_PLUGIN"_a8).getIterator<StringIteratorASCII>();
    SC_TRY_IF(it.advanceAfterFinding(endPlugin));
    SC_TRY_IF(it.reverseAdvanceUntilMatches('\n'));
    auto end  = it;
    extracted = StringView::fromIterators(start, end);
    return true;
}

SC::ReturnCode SC::PluginDefinition::getDynamicLibraryAbsolutePath(String& fullDynamicPath) const
{
    SC_TRY_IF(Path::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
    StringBuilder builder(fullDynamicPath);
#if SC_PLATFORM_WINDOWS
    SC_TRY_IF(builder.append(".dll"));
#else
    SC_TRY_IF(builder.append(".dylib"));
#endif
    return true;
}

SC::ReturnCode SC::PluginDefinition::getDynamicLibraryPDBAbsolutePath(String& fullDynamicPath) const
{
    SC_TRY_IF(Path::join(fullDynamicPath, {directory.view(), identity.identifier.view()}));
    StringBuilder builder(fullDynamicPath);
#if SC_PLATFORM_WINDOWS
    SC_TRY_IF(builder.append(".pdb"));
#else
    SC_TRY_IF(builder.append(".dSYM"));
#endif
    return true;
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
                SC_TRY_IF(pluginDefinition.dependencies.push_back(tokenizer.component));
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
        if (current == '\n')
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

SC::ReturnCode SC::PluginScanner::scanDirectory(const StringView directory, Vector<PluginDefinition>& definitions)
{
    FileSystemWalker walker;
    walker.options.recursive = false; // Manually recurse only first level dirs
    SC_TRY_IF(walker.init(directory));
    FileSystem fs;
    SC_TRY_IF(fs.init(directory));
    bool multiplePluginDefinitions = false;
    // Iterate each directory at first level and tentatively build a Plugin Definition.
    // A plugin will be valid if only a single Plugin Definition will be parsed.
    // Both no plugin definition (identity.identifier.isEmpty()) and multiple
    // contradictory plugin definitions (multiplePluginDefinitions) will prevent creation of the Plugin Definition
    String file;
    while (walker.enumerateNext())
    {
        const auto& item = walker.get();
        if (item.isDirectory() and item.level == 0) // only recurse first level
        {
            SC_TRY_IF(walker.recurseSubdirectory());
            const StringView pluginDirectory = item.path;
            if (definitions.isEmpty() or not definitions.back().identity.identifier.isEmpty())
            {
                SC_TRY_IF(definitions.resize(definitions.size() + 1));
            }
            definitions.back().files.clear();
            SC_TRY_IF(definitions.back().directory.assign(pluginDirectory));
            multiplePluginDefinitions = false;
        }
        if (item.level == 1 and item.name.endsWith(SC_STR_NATIVE(".cpp")))
        {
            if (multiplePluginDefinitions)
            {
                continue;
            }
            PluginDefinition& pluginDefinition = definitions.back();
            {
                PluginFile pluginFile;
                SC_TRY_IF(pluginFile.absolutePath.assign(item.path));
                SC_TRY_IF(pluginDefinition.files.push_back(move(pluginFile)));
            }
            file.data.clear();
            SC_TRY_IF(fs.read(item.path, file.data));
            StringView extracted;
            if (PluginDefinition::find(file.view(), extracted))
            {
                if (PluginDefinition::parse(extracted, pluginDefinition))
                {
                    if (pluginDefinition.identity.identifier.isEmpty())
                    {
                        const StringView identifier          = Path::basename(pluginDefinition.directory.view());
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
            SC_TRY_IF(definitions.pop_back());
        }
    }
    return walker.checkErrors();
}

SC::ReturnCode SC::PluginCompiler::findBestCompiler(PluginCompiler& compiler)
{
#if SC_PLATFORM_WINDOWS
    compiler.type               = Type::MicrosoftCompiler;
    constexpr StringView root[] = {L"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC",
                                   L"C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC",
                                   L"C:/Program Files/Microsoft Visual Studio/2022/Preview/VC/Tools/MSVC"};

    bool found = false;
    for (const StringView& base : root)
    {
        FileSystemWalker fswalker;
        SC_TRY_IF(fswalker.init(base));
        while (fswalker.enumerateNext())
        {
            if (fswalker.get().isDirectory())
            {
                const StringView candidate = fswalker.get().name;
                compiler.compilerPath.data.clear();
                compiler.linkerPath.data.clear();
                StringBuilder compilerBuilder(compiler.compilerPath);
                SC_TRY_IF(compilerBuilder.append(base));
                SC_TRY_IF(compilerBuilder.append(L"/"));
                SC_TRY_IF(compilerBuilder.append(candidate));
#if SC_PLATFORM_ARM64
                SC_TRY_IF(compilerBuilder.append(L"/bin/Hostarm64/arm64/"));
#else
                SC_TRY_IF(compilerBuilder.append(L"/bin/Hostx64/x64/"));
#endif
                compiler.linkerPath = compiler.compilerPath;
                StringBuilder linkerBuilder(compiler.linkerPath);
                SC_TRY_IF(linkerBuilder.append(L"link.exe"));
                SC_TRY_IF(compilerBuilder.append(L"cl.exe"));
                FileSystem fs;
                if (fs.init(base))
                {
                    if (fs.existsAndIsFile(compiler.compilerPath.view()) and
                        fs.existsAndIsFile(compiler.linkerPath.view()))
                    {
                        // TODO: Improve vstudio detection, finding latest
                        found = true;
                        break;
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
        return "Visual Studio Compiler not found"_a8;
    }
#else
    compiler.type         = Type::ClangCompiler;
    compiler.compilerPath = "clang"_a8;
    compiler.linkerPath   = "clang"_a8;
#endif
    return true;
}

SC::ReturnCode SC::PluginCompiler::compileFile(StringView sourceFile, StringView objectFile) const
{
    Process    process;
    StringView includePath = StringView(__FILE__);
    for (int i = 0; i < 4; ++i)
    {
        includePath = Path::dirname(includePath);
    }
    StringNative<256> includes = StringEncoding::Native;
    StringBuilder     includeBuilder(includes);
#if SC_PLATFORM_WINDOWS
    StringNative<256> destFile = StringEncoding::Native;
    StringBuilder     destBuilder(destFile);
    SC_TRY_IF(destBuilder.append(L"/Fo:"));
    SC_TRY_IF(destBuilder.append(objectFile));
    SC_TRY_IF(includeBuilder.append(L"/I\""));
#if SC_MSVC
    if (includePath.startsWithChar('\\'))
    {
        // For some reason on MSVC __FILE__ returns paths on network drives with a single starting slash
        SC_TRY_IF(includeBuilder.append(L"\\"));
    }
#endif
    SC_TRY_IF(includeBuilder.append(includePath));
    SC_TRY_IF(includeBuilder.append(L"\""));
    SC_TRY_IF(process.launch(compilerPath.view(), includes.view(), destFile.view(), L"/std:c++14",
                             L"/DSC_DISABLE_CONFIG=1", L"/GR-", L"/WX", L"/W4", L"/permissive-", L"/sdl-", L"/GS-",
                             L"/Zi", L"/DSC_PLUGIN_LIBRARY=1", L"/EHsc-", L"/c", sourceFile));
#else
    SC_TRY_IF(includeBuilder.append("-I"));
    SC_TRY_IF(includeBuilder.append(includePath));
    SC_TRY_IF(process.launch("clang", "-DSC_DISABLE_CONFIG=1", "-DSC_PLUGIN_LIBRARY=1", "-nostdinc++", "-nostdinc",
                             "-fno-stack-protector", "-std=c++14", includes.view(), "-fno-exceptions", "-fno-rtti",
                             "-g", "-c", "-fpic", sourceFile, "-o", objectFile));
#endif
    SC_TRY_IF(process.waitForExitSync());
    auto res = process.exitStatus.status.get();
    return res and *res == 0;
}

SC::ReturnCode SC::PluginCompiler::compile(const PluginDefinition& plugin) const
{
    // TODO: Spawn parallel tasks
    for (auto& file : plugin.files)
    {
        StringView        dirname    = Path::dirname(file.absolutePath.view());
        StringView        outputName = Path::basename(file.absolutePath.view(), SC_STR_NATIVE(".cpp"));
        StringNative<256> destFile   = StringEncoding::Native;
        SC_TRY_IF(Path::join(destFile, {dirname, outputName}));
        StringBuilder builder(destFile);
        SC_TRY_IF(builder.append(".o"));
        SC_TRY_IF(compileFile(file.absolutePath.view(), destFile.view()));
    }
    return true;
}

SC::ReturnCode SC::PluginCompiler::link(const PluginDefinition& definition, StringView executablePath) const
{
    Process process;
    String  destFile = StringEncoding::Native;
    SC_TRY_IF(definition.getDynamicLibraryAbsolutePath(destFile));
    Vector<StringNative<256>> objectFiles;
    SC_TRY_IF(objectFiles.reserve(definition.files.size()));
    for (auto& file : definition.files)
    {
        StringNative<256> objectPath = StringEncoding::Native;
        StringBuilder     b(objectPath);
        StringView        dirname    = Path::dirname(file.absolutePath.view());
        StringView        outputName = Path::basename(file.absolutePath.view(), SC_STR_NATIVE(".cpp"));
        SC_TRY_IF(b.append(dirname));
        SC_TRY_IF(b.append("/"));
        SC_TRY_IF(b.append(outputName));
        SC_TRY_IF(b.append(".o"));
        SC_TRY_IF(objectFiles.push_back(move(objectPath)));
    }
    SmallVector<StringView, 256> args;
#if SC_PLATFORM_WINDOWS
    StringNative<256> outFile = StringEncoding::Native;
    StringBuilder     outFileBuilder(outFile);
    SC_TRY_IF(outFileBuilder.append(L"/OUT:"));
    SC_TRY_IF(outFileBuilder.append(destFile.view()));

    StringNative<256> libPath = StringEncoding::Native;
    StringBuilder     libPathBuilder(libPath);
    SC_TRY_IF(libPathBuilder.append(L"/LIBPATH:"));
    SC_TRY_IF(libPathBuilder.append(Path::dirname(executablePath)));
    StringView exeName = Path::basename(executablePath, ".exe"_u8);

    StringNative<256> libName = StringEncoding::Native;
    StringBuilder     libNameBuilder(libName);
    SC_TRY_IF(libNameBuilder.append(exeName));
    SC_TRY_IF(libNameBuilder.append(L".lib"));

    SC_TRY_IF(args.push_back(
        {linkerPath.view(), L"/DLL", L"/DEBUG", L"/NODEFAULTLIB", L"/ENTRY:DllMain", libPath.view(), libName.view()}));
    for (auto& obj : objectFiles)
    {
        SC_TRY_IF(args.push_back(obj.view()));
    }
    SC_TRY_IF(args.push_back(outFile.view()));
    SC_UNUSED(executablePath);
#else
    SC_TRY_IF(
        args.push_back({"clang", "-bundle_loader", executablePath, "-bundle", "-fpic", "-nostdlib++", "-nostdlib"}));
    SC_TRY_IF(objectFiles.reserve(definition.files.size()));
    for (auto& obj : objectFiles)
    {
        SC_TRY_IF(args.push_back(obj.view()));
    }
    SC_TRY_IF(args.push_back({"-o", destFile.view()}));
#endif
    SC_TRY_IF(process.formatArguments(args.toSpanConst()));
    SC_TRY_IF(process.launch());
    SC_TRY_IF(process.waitForExitSync());
    auto res = process.exitStatus.status.get();
    return res and *res == 0;
}

SC::ReturnCode SC::PluginDynamicLibrary::unload()
{
    SC_TRY_IF(dynamicLibrary.close());
#if SC_PLATFORM_WINDOWS
    if (SystemDebug::isDebuggerConnected())
    {
        SmallString<256> pdbFile = StringEncoding::Native;
        SC_TRY_IF(definition.getDynamicLibraryPDBAbsolutePath(pdbFile));
        FileSystem fs;
        if (fs.existsAndIsFile(pdbFile.view()))
        {
            SC_TRY_IF(SystemDebug::unlockFileFromAllProcesses(pdbFile.view()));
            SC_TRY_IF(SystemDebug::deleteForcefullyUnlockedFile(pdbFile.view()))
        }
    }
#endif
    return true;
}

SC::ReturnCode SC::PluginDynamicLibrary::load(const PluginCompiler& compiler, StringView executablePath)
{
    SC_TRY_MSG(not dynamicLibrary.isValid(), "Dynamic Library must be unloaded first"_a8);
    SC_TRY_IF(compiler.compile(definition));
#if SC_PLATFORM_WINDOWS
    Thread::Sleep(400); // Sometimes file is locked...
    SC_TRY_IF(compiler.link(definition, executablePath));
#else
    SC_TRY_IF(compiler.link(definition, executablePath));
#endif

    StringNative<256> buffer;
    SC_TRY_IF(definition.getDynamicLibraryAbsolutePath(buffer));
    SC_TRY_IF(dynamicLibrary.load(buffer.view()));

    SC_TRY_IF(StringBuilder(buffer).format("{}Init", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginInit), "Missing #PluginName#Init"_a8);
    SC_TRY_IF(StringBuilder(buffer).format("{}Close", definition.identity.identifier.view()));
    SC_TRY_MSG(dynamicLibrary.getSymbol(buffer.view(), pluginClose), "Missing #PluginName#Close"_a8);
    return true;
}

SC::ReturnCode SC::PluginRegistry::init(Vector<PluginDefinition>&& definitions)
{
    for (auto& definition : definitions)
    {
        PluginDynamicLibrary pdl;
        pdl.definition = move(definition);
        SC_TRY_IF(libraries.insertIfNotExists({pdl.definition.identity.identifier, move(pdl)}));
    }
    definitions.clear();
    return true;
}

const SC::PluginDynamicLibrary* SC::PluginRegistry::findPlugin(const StringView identifier)
{
    auto result = libraries.get(identifier);
    return result ? result : nullptr;
}

SC::ReturnCode SC::PluginRegistry::loadPlugin(const StringView identifier, const PluginCompiler& compiler,
                                              StringView executablePath, LoadMode loadMode)
{
    PluginDynamicLibrary* res = libraries.get(identifier);
    SC_TRY_IF(res != nullptr);
    PluginDynamicLibrary& lib = *res;
    if (loadMode == LoadMode::Reload or not lib.dynamicLibrary.isValid())
    {
        // TODO: Shield against circular dependencies
        for (const auto& dependency : lib.definition.dependencies)
        {
            SC_TRY_IF(loadPlugin(dependency.view(), compiler, executablePath, LoadMode::Default));
        }
        if (lib.dynamicLibrary.isValid())
        {
            SC_TRY_IF(unloadPlugin(identifier));
        }
        SC_TRY_IF(lib.load(compiler, executablePath));
        SC_TRY_MSG(lib.pluginInit(lib.instance), "PluginInit failed"_a8); // TODO: Return actual failure strings
        return true;
    }
    return true;
}

SC::ReturnCode SC::PluginRegistry::unloadPlugin(const StringView identifier)
{
    PluginDynamicLibrary* res = libraries.get(identifier);
    SC_TRY_IF(res != nullptr);
    PluginDynamicLibrary& lib = *res;
    if (lib.dynamicLibrary.isValid())
    {
        for (const auto& kv : libraries.getItems())
        {
            // TODO: Shield against circular dependencies
            if (kv.value.definition.dependencies.contains(identifier))
            {
                SC_TRY_IF(unloadPlugin(kv.key.view()));
            }
        }
        auto closeResult = lib.pluginClose(lib.instance);
        lib.instance     = nullptr;
        SC_UNUSED(closeResult); // TODO: Print / Return some warning
    }
    return lib.unload();
}

SC::ReturnCode SC::PluginRegistry::removeAllBuildProducts(const StringView identifier)
{
    PluginDynamicLibrary* res = libraries.get(identifier);
    SC_TRY_IF(res != nullptr);
    PluginDynamicLibrary& lib = *res;
    FileSystem            fs;
    SC_TRY_IF(fs.init(lib.definition.directory.view()));
    StringNative<255> buffer;
    StringBuilder     fmt(buffer);
#if SC_PLATFORM_WINDOWS
    SC_TRY_IF(fmt.format("{}.lib", identifier));
    SC_TRY_IF(fs.removeFile(buffer.view()));
    SC_TRY_IF(fmt.format("{}.exp", identifier));
    SC_TRY_IF(fs.removeFile(buffer.view()));
    SC_TRY_IF(fmt.format("{}.ilk", identifier));
    SC_TRY_IF(fs.removeFile(buffer.view()));
    SC_TRY_IF(fmt.format("{}.dll", identifier));
    int numTries = 10;
    while (not fs.removeFile(buffer.view()))
    {
        Thread::Sleep(10); // It looks like FreeLibrary needs some time to avoid getting access denied
        numTries--;
        SC_TRY_MSG(numTries >= 0, "PluginRegistry: Cannot remove dll"_a8);
    }
#else
    SC_TRY_IF(fmt.format("{}.dylib", identifier));
    SC_TRY_IF(fs.removeFile(buffer.view()));
#endif
    for (auto& file : lib.definition.files)
    {
        StringView        dirname    = Path::dirname(file.absolutePath.view());
        StringView        outputName = Path::basename(file.absolutePath.view(), SC_STR_NATIVE(".cpp"));
        StringNative<256> destFile   = StringEncoding::Native;
        SC_TRY_IF(Path::join(destFile, {dirname, outputName}));
        StringBuilder builder(destFile);
        SC_TRY_IF(builder.append(".o"));
        SC_TRY_IF(fs.removeFile(destFile.view()));
    }
    return true;
}
