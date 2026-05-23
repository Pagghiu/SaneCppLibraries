// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Tools.h"

#include "../../Libraries/File/File.h"
#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Foundation/Function.h"
#include "../../Libraries/Hashing/Hashing.h"
#include "../../Libraries/Process/Process.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringBuilder.h"

namespace SC
{
namespace Tools
{
struct Download
{
    SmallString<255> packagesCacheDirectory;
    SmallString<255> packagesInstallDirectory;

    SmallString<255> packageName;
    SmallString<255> packageVersion;
    SmallString<255> shallowClone;
    SmallString<255> packagePlatform;
    SmallString<255> url;
    SmallString<255> expectedHash;

    Hashing::Type hashType = Hashing::TypeMD5;

    bool createLink = true;
    bool isGitClone = false;

    Download()
    {
        switch (HostPlatform)
        {
        case Platform::Apple: //
            packagePlatform = "macos";
            break;
        case Platform::Linux: //
            packagePlatform = "linux";
            break;
        case Platform::Windows: //
            packagePlatform = "windows";
            break;
        case Platform::Emscripten: packagePlatform = "emscripten"; break;
        }
    }
};

struct Package
{
    SmallString<255> packageFullName;
    SmallString<255> packageLocalDirectory;
    SmallString<255> packageLocalFile;
    SmallString<255> packageLocalTxt;
    SmallString<255> packageBaseName;

    SmallString<255> installDirectoryLink;
};

namespace PackageExport
{
constexpr StringView Clang                   = "clang";
constexpr StringView ClangXX                 = "clang++";
constexpr StringView ZLibShared              = "libz.so.1";
constexpr StringView ZLibSharedLink          = "libz.so";
constexpr StringView ZLibLibraryDir          = "zlib.lib";
constexpr StringView ZLibIncludeDir          = "zlib.include";
constexpr StringView LLVMAr                  = "llvm-ar";
constexpr StringView LLVMLinker              = "ld.lld";
constexpr StringView Sysroot                 = "sysroot";
constexpr StringView RunnerQEMU              = "qemu";
constexpr StringView RunnerWine              = "wine";
constexpr StringView MSVCClX64               = "cl.x64";
constexpr StringView MSVCClArm64             = "cl.arm64";
constexpr StringView MSVCLinkX64             = "link.x64";
constexpr StringView MSVCLinkArm64           = "link.arm64";
constexpr StringView MSVCLibX64              = "lib.x64";
constexpr StringView MSVCLibArm64            = "lib.arm64";
constexpr StringView LLVMMinGWClang_X86_64   = "x86_64-w64-mingw32-clang";
constexpr StringView LLVMMinGWClangArm64     = "aarch64-w64-mingw32-clang";
constexpr StringView LLVMMinGWClangXX_X86_64 = "x86_64-w64-mingw32-clang++";
constexpr StringView LLVMMinGWClangXXArm64   = "aarch64-w64-mingw32-clang++";
} // namespace PackageExport

namespace PackageExportKind
{
constexpr StringView Asset      = "asset";
constexpr StringView Capability = "capability";
constexpr StringView IncludeDir = "include-dir";
constexpr StringView Library    = "library";
constexpr StringView LibraryDir = "library-dir";
constexpr StringView Runner     = "runner";
constexpr StringView Sysroot    = "sysroot";
constexpr StringView Tool       = "tool";
} // namespace PackageExportKind

namespace PackageKind
{
constexpr StringView Asset     = "asset";
constexpr StringView Library   = "library";
constexpr StringView Runner    = "runner";
constexpr StringView Sysroot   = "sysroot";
constexpr StringView Tool      = "tool";
constexpr StringView Toolchain = "toolchain";
} // namespace PackageKind

namespace PackageCapability
{
constexpr StringView ToolCCompiler             = "tool.c-compiler";
constexpr StringView ToolCXXCompiler           = "tool.cxx-compiler";
constexpr StringView ToolArchiver              = "tool.archiver";
constexpr StringView ToolLinker                = "tool.linker";
constexpr StringView RunnerWine                = "runner.wine";
constexpr StringView RunnerQEMUX86_64          = "runner.qemu.x86_64";
constexpr StringView RunnerQEMUArm64           = "runner.qemu.arm64";
constexpr StringView LibraryZLibFilCX86_64     = "library.zlib.filc.x86_64";
constexpr StringView ToolchainFilCX86_64       = "toolchain.filc.x86_64";
constexpr StringView ToolchainWindowsGNUX86_64 = "toolchain.windows-gnu.x86_64";
constexpr StringView ToolchainWindowsGNUArm64  = "toolchain.windows-gnu.arm64";
constexpr StringView ToolchainWindowsMSVCX64   = "toolchain.windows-msvc.x64";
constexpr StringView ToolchainWindowsMSVCArm64 = "toolchain.windows-msvc.arm64";

[[nodiscard]] constexpr StringView qemuRunner(InstructionSet architecture)
{
    return architecture == InstructionSet::Intel64 ? RunnerQEMUX86_64 : RunnerQEMUArm64;
}
} // namespace PackageCapability

struct PackageReceiptExport
{
    StringView kind;
    StringView name;
    StringView relativePath;
};

struct PackageRegistryExport
{
    StringView kind;
    StringView name;
};

struct PackageReceiptInfo
{
    StringView             packageName;
    StringView             packageVersion;
    StringView             recipeVersion;
    StringView             hostPlatform;
    StringView             packageVariant;
    StringView             source;
    StringView             sourceHash;
    StringView             validation;
    Span<const StringView> phases;
};

using PackageInstallHandler = Result (*)(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                         Package& package, Span<const StringView> packageArguments);

struct PackageRecipe;
using PackagePhaseHandler = Result (*)(const PackageRecipe& recipe, Package& package);

struct PackagePhaseRegistryEntry
{
    StringView          name;
    PackagePhaseHandler handler = nullptr;
};

struct PackagePhaseRegistry
{
    Span<const PackagePhaseRegistryEntry> entries;

    [[nodiscard]] const PackagePhaseRegistryEntry* find(StringView phaseName) const
    {
        for (const PackagePhaseRegistryEntry& entry : entries)
        {
            if (entry.name == phaseName)
            {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct PackageRegistryEntry
{
    StringView                        name;
    StringView                        installedName;
    StringView                        kind;
    StringView                        description;
    StringView                        variants;
    StringView                        source;
    bool                              supportsImport = false;
    Span<const PackageRegistryExport> exports;
    Span<const StringView>            phases;
    PackageInstallHandler             install = nullptr;
    const PackageRecipe*              recipe  = nullptr;
};

struct PackageRegistry
{
    Span<const PackageRegistryEntry> entries;

    [[nodiscard]] const PackageRegistryEntry* find(StringView packageName) const
    {
        for (const PackageRegistryEntry& entry : entries)
        {
            if (entry.name == packageName)
            {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct PackageRegistryBuilder
{
    Span<PackageRegistryEntry> entries;
    size_t                     size = 0;

    [[nodiscard]] PackageRegistry registry() const { return {{entries.data(), size}}; }

    [[nodiscard]] Result add(const PackageRegistryEntry& entry)
    {
        SC_TRY_MSG(registry().find(entry.name) == nullptr, "Duplicate package registry entry");
        SC_TRY_MSG(size < entries.sizeInElements(), "Package registry storage exhausted");
        entries[size] = entry;
        size += 1;
        return Result(true);
    }

    [[nodiscard]] Result add(PackageRegistry registryToAppend)
    {
        for (const PackageRegistryEntry& entry : registryToAppend.entries)
        {
            SC_TRY(add(entry));
        }
        return Result(true);
    }

    [[nodiscard]] operator PackageRegistry() const { return registry(); }
};

struct CustomFunctions
{
    Function<Result(const Download&, const Package&)> testFunction;
    Function<Result(StringView, StringView)>          extractFunction;
    bool                                              keepDownloadedArchive = true;
};

enum class PackageRecipeKind
{
    Download,
    CopyDirectory,
};

struct PackageRecipe
{
    PackageRecipeKind                kind = PackageRecipeKind::Download;
    StringView                       copySourceDirectory;
    Download                         download;
    Package                          package;
    CustomFunctions                  functions;
    Span<const PackageReceiptExport> exports;
    Span<const StringView>           phases;
    PackagePhaseRegistry             phaseRegistry;
};

constexpr StringView PackagesCacheDirectory   = "_PackagesCache";
constexpr StringView PackagesInstallDirectory = "_Packages";
constexpr StringView PackageReceiptFileName   = "sc-package-receipt.json";

Result               writePackageReceipt(const Package& package, const PackageReceiptInfo& info,
                                         Span<const PackageReceiptExport> exports = {});
Result               installPackageRecipe(const PackageRecipe& recipe, Package& package);
PackagePhaseRegistry builtinPackagePhaseRegistry();
Result               resolvePackageExportPath(StringView packageRoot, StringView exportName, String& output);
Result               resolvePackageCapabilityPath(StringView packageRoot, StringView capabilityName, String& output);
Result runPackageTool(Tool::Arguments& arguments, PackageRegistry registry, Tools::Package* package = nullptr);
Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package);

} // namespace Tools
} // namespace SC
