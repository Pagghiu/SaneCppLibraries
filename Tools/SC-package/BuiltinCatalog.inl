// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

static Result installDoxygenEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installDoxygen(cache, install, package);
}

static Result installDoxygenAwesomeCssEntry(StringView cache, StringView install, Package& package,
                                            Span<const StringView>)
{
    return installDoxygenAwesomeCss(cache, install, package);
}

static Result installClangEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installClangBinaries(cache, install, package);
}

static Result installLLVMEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installLLVMToolchain(cache, install, package);
}

static Result installQEMUEntry(StringView cache, StringView install, Package& package, Span<const StringView> arguments)
{
    QEMUPackageInstallOptions options;
    SC_TRY(parseQEMUPackageInstallOptions(arguments, options));
    return installQEMURunner(cache, install, package, options.importDirectory);
}

static Result installFilCEntry(StringView cache, StringView install, Package& package, Span<const StringView> arguments)
{
    FilCPackageInstallOptions options;
    SC_TRY(parseFilCPackageInstallOptions(arguments, options));
    return installFilCToolchain(cache, install, package, options.importDirectory);
}

static Result installLLVMMingwEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installLLVMMingwToolchain(cache, install, package);
}

static Result installLinuxSysrootGlibcX64Entry(StringView cache, StringView install, Package& package,
                                               Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Glibc;
    spec.architecture = InstructionSet::Intel64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installLinuxSysrootGlibcArm64Entry(StringView cache, StringView install, Package& package,
                                                 Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Glibc;
    spec.architecture = InstructionSet::ARM64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installLinuxSysrootMuslX64Entry(StringView cache, StringView install, Package& package,
                                              Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Musl;
    spec.architecture = InstructionSet::Intel64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installLinuxSysrootMuslArm64Entry(StringView cache, StringView install, Package& package,
                                                Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Musl;
    spec.architecture = InstructionSet::ARM64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installWineEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installWineStableRunner(cache, install, package);
}

static Result installMSVCEntry(StringView cache, StringView install, Package& package, Span<const StringView> arguments)
{
    MSVCPackageInstallOptions options;
    SC_TRY(parseMSVCPackageInstallOptions(arguments, options));
    return installMSVCToolchain(cache, install, package, options.importDirectory, options.wineExecutable);
}

static constexpr PackageRegistryExport DoxygenExports[] = {
    {PackageExportKind::Tool, "doxygen"},
};
static constexpr StringView DoxygenPhases[] = {
    "resolveDoxygenRelease",
    "extractDoxygenExecutable",
    "validateDoxygenVersion",
    "writeReceipt",
};
static constexpr PackageRegistryExport DoxygenAwesomeExports[] = {
    {PackageExportKind::Asset, "doxygen-awesome-css"},
};
static constexpr StringView DoxygenAwesomePhases[] = {
    "fetchGitRevision",
    "validateGitCommit",
    "writeReceipt",
};
static constexpr PackageRegistryExport ClangExports[] = {
    {PackageExportKind::Tool, "clang-format"},
};
static constexpr StringView ClangPhases[] = {
    "resolveHostLLVMArchive",
    "extractClangFormat",
    "validateClangFormatVersion",
    "writeReceipt",
};
static constexpr PackageRegistryExport LLVMExports[] = {
    {PackageExportKind::Tool, PackageExport::Clang},
    {PackageExportKind::Tool, PackageExport::ClangXX},
    {PackageExportKind::Tool, PackageExport::LLVMAr},
    {PackageExportKind::Tool, PackageExport::LLVMLinker},
    {PackageExportKind::Capability, PackageCapability::ToolCCompiler},
    {PackageExportKind::Capability, PackageCapability::ToolCXXCompiler},
    {PackageExportKind::Capability, PackageCapability::ToolArchiver},
    {PackageExportKind::Capability, PackageCapability::ToolLinker},
};
static constexpr StringView LLVMPhases[] = {
    "resolveHostLLVMArchive",
    "extractLLVMToolchain",
    "validateLLVMToolchain",
    "writeReceipt",
};
static constexpr PackageRegistryExport QEMUExports[] = {
    {PackageExportKind::Runner, PackageExport::RunnerQEMU},
    {PackageExportKind::Capability, PackageCapability::RunnerQEMUX86_64},
    {PackageExportKind::Capability, PackageCapability::RunnerQEMUArm64},
};
static constexpr StringView QEMUPhases[] = {
    "resolveImportedQEMU",
    "validateQEMUTargets",
    "writeReceipt",
};
static constexpr PackageRegistryExport FilCExports[] = {
    {PackageExportKind::Tool, PackageExport::Clang},
    {PackageExportKind::Tool, PackageExport::ClangXX},
    {PackageExportKind::Capability, PackageCapability::ToolchainFilCX86_64},
};
static constexpr StringView FilCPhases[] = {
    "resolveFilCSource",
    "prepareFilCCompilerLaunchers",
    "validateFilCToolchain",
    "writeReceipt",
};
static constexpr PackageRegistryExport LLVMMingwExports[] = {
    {PackageExportKind::Tool, PackageExport::LLVMMinGWClang_X86_64},
    {PackageExportKind::Tool, PackageExport::LLVMMinGWClangXX_X86_64},
    {PackageExportKind::Tool, PackageExport::LLVMMinGWClangArm64},
    {PackageExportKind::Tool, PackageExport::LLVMMinGWClangXXArm64},
    {PackageExportKind::Tool, PackageExport::LLVMAr},
    {PackageExportKind::Capability, PackageCapability::ToolchainWindowsGNUX86_64},
    {PackageExportKind::Capability, PackageCapability::ToolchainWindowsGNUArm64},
};
static constexpr StringView LLVMMingwPhases[] = {
    "resolveLLVMMingwArchive",
    "extractLLVMMingwToolchain",
    "validateLLVMMingwToolchain",
    "writeReceipt",
};
static constexpr PackageRegistryExport LinuxSysrootExports[] = {
    {PackageExportKind::Sysroot, PackageExport::Sysroot},
    {PackageExportKind::IncludeDir, "sysroot.include"},
    {PackageExportKind::LibraryDir, "sysroot.lib"},
    {PackageExportKind::Capability, "sysroot.linux.<environment>.<architecture>"},
};
static constexpr StringView LinuxSysrootPhases[] = {
    "resolveLinuxSysrootPackages",
    "extractLinuxSysrootPackages",
    "repairLinuxSysrootLayout",
    "validateLinuxSysroot",
    "writeReceipt",
};
static constexpr PackageRegistryExport WineExports[] = {
    {PackageExportKind::Runner, PackageExport::RunnerWine},
    {PackageExportKind::Capability, PackageCapability::RunnerWine},
};
static constexpr StringView WinePhases[] = {
    "resolveWinePackages",
    "repairLinuxWineRunner",
    "validateWineRunner",
    "writeReceipt",
};
static constexpr PackageRegistryExport MSVCExports[] = {
    {PackageExportKind::Tool, PackageExport::MSVCClX64},
    {PackageExportKind::Tool, PackageExport::MSVCLinkX64},
    {PackageExportKind::Tool, PackageExport::MSVCLibX64},
    {PackageExportKind::Tool, PackageExport::MSVCClArm64},
    {PackageExportKind::Tool, PackageExport::MSVCLinkArm64},
    {PackageExportKind::Tool, PackageExport::MSVCLibArm64},
    {PackageExportKind::Capability, PackageCapability::ToolchainWindowsMSVCX64},
    {PackageExportKind::Capability, PackageCapability::ToolchainWindowsMSVCArm64},
};
static constexpr StringView MSVCPhases[] = {
    "fetchPortableMSVC", "repairMSVCLayout", "prepareMSVCWinePrefix", "validateMSVCLayout", "writeReceipt",
};

static constexpr PackageRegistryEntry BuiltinPackageRegistryEntries[] = {
    {"doxygen", "doxygen", "tool", "Documentation generator binary", "host", "GitHub release archive", false,
     DoxygenExports, DoxygenPhases, installDoxygenEntry},
    {"doxygen-awesome-css", "doxygen-awesome-css", "asset", "Doxygen Awesome CSS theme checkout", "host",
     "Pinned Git revision", false, DoxygenAwesomeExports, DoxygenAwesomePhases, installDoxygenAwesomeCssEntry},
    {"clang", "clang-binaries", "tool", "Pinned host clang-format binary", "host", "Official LLVM release archive",
     false, ClangExports, ClangPhases, installClangEntry},
    {"llvm", "llvm", "toolchain", "Pinned host LLVM toolchain", "host", "Official LLVM release archive", false,
     LLVMExports, LLVMPhases, installLLVMEntry},
    {"qemu", "qemu", "runner", "Imported QEMU user-mode runner registration", "host", "Imported directory or PATH",
     true, QEMUExports, QEMUPhases, installQEMUEntry},
    {"filc", "filc", "toolchain", "Experimental Fil-C compiler toolchain", "linux-x86_64", "Pinned archive or import",
     true, FilCExports, FilCPhases, installFilCEntry},
    {"llvm-mingw", "llvm-mingw", "toolchain", "LLVM MinGW Windows GNU toolchain", "host", "llvm-mingw release archive",
     false, LLVMMingwExports, LLVMMingwPhases, installLLVMMingwEntry},
    {"linux-sysroot-glibc-x86_64", "linux-sysroot-glibc-x86_64", "sysroot", "Linux glibc x86_64 sysroot",
     "linux-glibc-x86_64", "Ubuntu Jammy package index", false, LinuxSysrootExports, LinuxSysrootPhases,
     installLinuxSysrootGlibcX64Entry},
    {"linux-sysroot-glibc-arm64", "linux-sysroot-glibc-arm64", "sysroot", "Linux glibc arm64 sysroot",
     "linux-glibc-arm64", "Ubuntu Jammy package index", false, LinuxSysrootExports, LinuxSysrootPhases,
     installLinuxSysrootGlibcArm64Entry},
    {"linux-sysroot-musl-x86_64", "linux-sysroot-musl-x86_64", "sysroot", "Linux musl x86_64 sysroot",
     "linux-musl-x86_64", "Alpine APK index", false, LinuxSysrootExports, LinuxSysrootPhases,
     installLinuxSysrootMuslX64Entry},
    {"linux-sysroot-musl-arm64", "linux-sysroot-musl-arm64", "sysroot", "Linux musl arm64 sysroot", "linux-musl-arm64",
     "Alpine APK index", false, LinuxSysrootExports, LinuxSysrootPhases, installLinuxSysrootMuslArm64Entry},
    {"wine", "wine-stable", "runner", "Wine runner for Windows targets", "host", "Wine release archive or Linux DEBs",
     false, WineExports, WinePhases, installWineEntry},
    {"msvc", "msvc", "toolchain", "Portable MSVC and Windows SDK toolchain", "windows-msvc-x64/windows-msvc-arm64",
     "Portable MSVC vendor fetch or import", true, MSVCExports, MSVCPhases, installMSVCEntry},
};

PackageRegistry builtinPackageRegistry() { return {BuiltinPackageRegistryEntries}; }

Result addBuiltinPackages(PackageRegistryBuilder& registry) { return registry.add(builtinPackageRegistry()); }
