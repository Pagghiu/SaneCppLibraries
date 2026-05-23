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

static constexpr StringView DoxygenExports[] = {
    "tool:doxygen",
};
static constexpr StringView DoxygenPhases[] = {
    "resolveDoxygenRelease",
    "extractDoxygenExecutable",
    "validateDoxygenVersion",
    "writeReceipt",
};
static constexpr StringView DoxygenAwesomeExports[] = {
    "asset:doxygen-awesome-css",
};
static constexpr StringView DoxygenAwesomePhases[] = {
    "fetchGitRevision",
    "validateGitCommit",
    "writeReceipt",
};
static constexpr StringView ClangExports[] = {
    "tool:clang-format",
};
static constexpr StringView ClangPhases[] = {
    "resolveHostLLVMArchive",
    "extractClangFormat",
    "validateClangFormatVersion",
    "writeReceipt",
};
static constexpr StringView LLVMExports[] = {
    "tool:clang",
    "tool:clang++",
    "tool:llvm-ar",
    "tool:ld.lld",
    "capability:tool.c-compiler",
    "capability:tool.cxx-compiler",
    "capability:tool.archiver",
    "capability:tool.linker",
};
static constexpr StringView LLVMPhases[] = {
    "resolveHostLLVMArchive",
    "extractLLVMToolchain",
    "validateLLVMToolchain",
    "writeReceipt",
};
static constexpr StringView QEMUExports[] = {
    "runner:qemu",
    "capability:runner.qemu.x86_64",
    "capability:runner.qemu.arm64",
};
static constexpr StringView QEMUPhases[] = {
    "resolveImportedQEMU",
    "validateQEMUTargets",
    "writeReceipt",
};
static constexpr StringView FilCExports[] = {
    "tool:clang",
    "tool:clang++",
    "capability:toolchain.filc.x86_64",
};
static constexpr StringView FilCPhases[] = {
    "resolveFilCSource",
    "prepareFilCCompilerLaunchers",
    "validateFilCToolchain",
    "writeReceipt",
};
static constexpr StringView LLVMMingwExports[] = {
    "tool:x86_64-w64-mingw32-clang",
    "tool:x86_64-w64-mingw32-clang++",
    "tool:aarch64-w64-mingw32-clang",
    "tool:aarch64-w64-mingw32-clang++",
    "tool:llvm-ar",
    "capability:toolchain.windows-gnu.x86_64",
    "capability:toolchain.windows-gnu.arm64",
};
static constexpr StringView LLVMMingwPhases[] = {
    "resolveLLVMMingwArchive",
    "extractLLVMMingwToolchain",
    "validateLLVMMingwToolchain",
    "writeReceipt",
};
static constexpr StringView LinuxSysrootExports[] = {
    "sysroot:sysroot",
    "include-dir:sysroot.include",
    "library-dir:sysroot.lib",
    "capability:sysroot.linux.<environment>.<architecture>",
};
static constexpr StringView LinuxSysrootPhases[] = {
    "resolveLinuxSysrootPackages",
    "extractLinuxSysrootPackages",
    "repairLinuxSysrootLayout",
    "validateLinuxSysroot",
    "writeReceipt",
};
static constexpr StringView WineExports[] = {
    "runner:wine",
    "capability:runner.wine",
};
static constexpr StringView WinePhases[] = {
    "resolveWinePackages",
    "repairLinuxWineRunner",
    "validateWineRunner",
    "writeReceipt",
};
static constexpr StringView MSVCExports[] = {
    "tool:cl.x64",
    "tool:link.x64",
    "tool:lib.x64",
    "tool:cl.arm64",
    "tool:link.arm64",
    "tool:lib.arm64",
    "capability:toolchain.windows-msvc.x64",
    "capability:toolchain.windows-msvc.arm64",
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
