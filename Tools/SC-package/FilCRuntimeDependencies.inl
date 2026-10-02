// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#if SC_PLATFORM_LINUX
static StringView filcRuntimeDependencyVariant()
{
    return HostInstructionSet == InstructionSet::ARM64 ? "linux-arm64"_a8 : "linux-x86_64"_a8;
}

static Result probeFilCRuntimeDependencyCompiler(StringView compiler, String& version)
{
    Process process;
    SC_TRY(process.exec({compiler, "--version"}, version));
    if (process.getExitStatus() != 0 or not StringView(version.view()).containsString("Fil-C"))
        return Result::Error(PackageResultCategory, PackageError::ToolProbeFailed);
    return Result(true);
}

static Result filcRuntimeDependencyReceiptMatches(StringView packageRoot, StringView receiptName, StringView version,
                                                  StringView variant, StringView source, StringView sourceHash,
                                                  Span<const PackageReceiptExport> exports, bool& matches)
{
    matches            = false;
    String receiptPath = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(packageRoot, receiptPath));
    String receipt = StringEncoding::Utf8;
    if (not readFileIntoString(receiptPath.view(), receipt))
        return Result(true);

    PackageReceiptJSON receiptJSON;
    if (not readPackageReceiptJSON(receipt.view(), receiptJSON) or not validatePackageReceiptHeader(receiptJSON))
        return Result(true);
    if (receiptJSON.name.view() != receiptName or receiptJSON.version.view() != version or
        receiptJSON.recipeVersion.view() != "1"_a8 or receiptJSON.hostPlatform.view() != "linux"_a8 or
        receiptJSON.variant.view() != variant or receiptJSON.source.view() != source or
        receiptJSON.sourceHash.view() != sourceHash or receiptJSON.installRoot.view() != packageRoot or
        receiptJSON.validation.view() != "passed"_a8 or receiptJSON.exports.size() != exports.sizeInElements())
    {
        return Result(true);
    }
    for (size_t idx = 0; idx < exports.sizeInElements(); ++idx)
    {
        const PackageReceiptExportJSON& actual = receiptJSON.exports[idx];
        const PackageReceiptExport&     wanted = exports[idx];
        if (actual.kind.view() != wanted.kind or actual.name.view() != wanted.name or
            actual.path.view() != wanted.relativePath)
        {
            return Result(true);
        }
    }
    matches = true;
    return Result(true);
}

static Result runOpenSSLFilCSmoke(StringView compiler, StringView openSSLRoot, StringView zlibRoot,
                                  StringView buildRoot)
{
    String source = StringEncoding::Utf8;
    String binary = StringEncoding::Utf8;
    SC_TRY(resolveToolSupportPath("OpenSSLFilCSmoke.c", source));
    SC_TRY(Path::join(binary, {buildRoot, "openssl-filc-smoke"}));

    String  includeDirectory = format("-I{}/include", openSSLRoot);
    String  openSSLLibrary   = format("-L{}/lib", openSSLRoot);
    String  zlibLibrary      = format("-L{}/lib", zlibRoot);
    Process compile;
    SC_TRY(compile.exec({compiler, "-Wall", "-Wextra", "-Werror", source.view(), includeDirectory.view(),
                         openSSLLibrary.view(), zlibLibrary.view(), "-lssl", "-lcrypto", "-lz", "-o", binary.view()}));
    if (compile.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::ToolchainCompileFailed);

    String  openSSLLibraryDirectory = format("{}/lib", openSSLRoot);
    String  zlibLibraryDirectory    = format("{}/lib", zlibRoot);
    String  runtimeLibraryPath      = format("{}:{}", openSSLLibraryDirectory.view(), zlibLibraryDirectory.view());
    Process run;
    SC_TRY(run.setEnvironment("LD_LIBRARY_PATH", runtimeLibraryPath.view()));
    SC_TRY(run.exec({binary.view()}));
    if (run.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::ToolchainRunFailed);
    return Result(true);
}

Result installOpenSSLFilC(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    if (HostInstructionSet != InstructionSet::ARM64 and HostInstructionSet != InstructionSet::Intel64)
        return Result::Error(PackageResultCategory, PackageError::InstallerArchitectureUnsupported);

    static constexpr StringView version = "3.6.5";
    static constexpr StringView url =
        "https://github.com/openssl/openssl/releases/download/openssl-3.6.5/openssl-3.6.5.tar.gz";
    static constexpr StringView hash        = "a2157c2830efdec3788939b00c9b0638306d3f0bbb76dc4832ee503bb397df98";
    static constexpr StringView patchHash   = "982c4231436064d8c79d94258b1fcdd6e82dea77e6da9794bb283bcc1d3af583";
    static constexpr StringView patchName   = "FilCOpenSSL365.patch";
    static constexpr StringView packageName = "openssl_filc";

    const StringView variant = filcRuntimeDependencyVariant();
    package.packageFullName  = "openssl-filc-3.6.5";
    package.packageBaseName  = "openssl-3.6.5.tar.gz";
    package.packageLocalFile = format("{}/openssl-filc/{}", packagesCacheDirectory, package.packageBaseName.view());
    package.packageLocalDirectory = format("{}/openssl-filc/source-{}", packagesCacheDirectory, version);
    package.packageLocalTxt       = format("{}/openssl-filc/{}.txt", packagesCacheDirectory, variant);
    package.installDirectoryLink  = format("{}/openssl_filc", packagesInstallDirectory);

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));
    SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalFile.view(), Path::AsNative)));

    String patchPath = StringEncoding::Utf8;
    SC_TRY(resolveToolSupportPath(patchName, patchPath));
    SC_TRY(checkFileHash(patchPath.view(), Hashing::TypeSHA256, patchHash));

    Package filc;
    SC_TRY(installFilCToolchain(packagesCacheDirectory, packagesInstallDirectory, filc));
    String compiler = StringEncoding::Utf8;
    SC_TRY(resolveFilCRawCompilerPath(filc.installDirectoryLink.view(), "clang", compiler));
    String compilerVersion = StringEncoding::Utf8;
    SC_TRY(probeFilCRuntimeDependencyCompiler(compiler.view(), compilerVersion));

    Package zlib;
    SC_TRY(installZLibFilC(packagesCacheDirectory, packagesInstallDirectory, zlib));
    String zlibIdentity = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(zlib.packageLocalTxt.view(), zlibIdentity));

    String buildRoot   = format("{}-build", package.installDirectoryLink.view());
    String archiveHash = format("sha256:{}", hash);
    String identity = format("{}\nsha256:{}\npatch={}\nsha256:{}\n{}\nprofile=shared-zlib-no-tests-no-apps\nCFLAGS=-g "
                             "-O2 -yolo-assembler\n{}\n{}\n",
                             url, hash, patchName, patchHash, variant, compilerVersion.view(), zlibIdentity.view());
    String cryptoLibrary = format("{}/lib/libcrypto.so.3", package.installDirectoryLink.view());
    String sslLibrary    = format("{}/lib/libssl.so.3", package.installDirectoryLink.view());
    String sslHeader     = format("{}/include/openssl/ssl.h", package.installDirectoryLink.view());
    String evpHeader     = format("{}/include/openssl/evp.h", package.installDirectoryLink.view());
    String receiptPath   = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(package.installDirectoryLink.view(), receiptPath));
    const PackageReceiptExport exports[] = {
        {PackageExportKind::Library, PackageExport::OpenSSLCryptoShared, "lib/libcrypto.so.3"},
        {PackageExportKind::Library, PackageExport::OpenSSLShared, "lib/libssl.so.3"},
        {PackageExportKind::LibraryDir, PackageExport::OpenSSLLibraryDir, "lib"},
        {PackageExportKind::IncludeDir, PackageExport::OpenSSLIncludeDir, "include"},
        {PackageExportKind::Capability,
         HostInstructionSet == InstructionSet::ARM64 ? PackageCapability::LibraryOpenSSLFilCArm64
                                                     : PackageCapability::LibraryOpenSSLFilCX86_64,
         "lib/libssl.so.3"},
    };

    String previous = StringEncoding::Utf8;
    if (fs.existsAndIsFile(cryptoLibrary.view()) and fs.existsAndIsFile(sslLibrary.view()) and
        fs.existsAndIsFile(sslHeader.view()) and fs.existsAndIsFile(evpHeader.view()) and
        fs.existsAndIsFile(receiptPath.view()) and readFileIntoString(package.packageLocalTxt.view(), previous) and
        previous.view() == identity.view())
    {
        bool receiptMatches = false;
        SC_TRY(filcRuntimeDependencyReceiptMatches(package.installDirectoryLink.view(), packageName, version, variant,
                                                   url, archiveHash.view(), exports, receiptMatches));
        if (receiptMatches)
        {
            SC_TRY(fs.makeDirectoryRecursive(buildRoot.view()));
            return runOpenSSLFilCSmoke(compiler.view(), package.installDirectoryLink.view(),
                                       zlib.installDirectoryLink.view(), buildRoot.view());
        }
    }

    SC_TRY(downloadFileHash(url, package.packageLocalFile.view(), Hashing::TypeSHA256, hash));
    SC_TRY(extractTarArchiveFlatteningRoot(package.packageLocalFile.view(), package.packageLocalDirectory.view()));
    if (fs.existsAndIsDirectory(buildRoot.view()))
        SC_TRY(fs.removeDirectoriesRecursive(buildRoot.view()));
    SC_TRY(removePackageInstallLink(fs, package));
    SC_TRY(fs.makeDirectoryRecursive(buildRoot.view()));

    Process applyPatch;
    SC_TRY(applyPatch.exec({"patch", "--batch", "--forward", "--fuzz=0", "-p2", "-d",
                            package.packageLocalDirectory.view(), "-i", patchPath.view()}));
    if (applyPatch.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::PackageBuildFailed);

    String           configure = format("{}/Configure", package.packageLocalDirectory.view());
    const StringView target    = HostInstructionSet == InstructionSet::ARM64 ? "linux-aarch64"_a8 : "linux-x86_64"_a8;
    String           prefix    = format("--prefix={}", package.installDirectoryLink.view());
    String           opensslDirectory = format("--openssldir={}/ssl", package.installDirectoryLink.view());
    String           zlibInclude      = format("--with-zlib-include={}/include", zlib.installDirectoryLink.view());
    String           zlibLibrary      = format("--with-zlib-lib={}/lib", zlib.installDirectoryLink.view());
    Process          config;
    SC_TRY(config.setWorkingDirectory(buildRoot.view()));
    SC_TRY(config.setEnvironment("CC", compiler.view()));
    SC_TRY(config.setEnvironment("CFLAGS", "-g -O2 -yolo-assembler"));
    SC_TRY(config.exec({configure.view(), target, "shared", "zlib", "no-tests", "no-apps", prefix.view(),
                        "--libdir=lib", opensslDirectory.view(), zlibInclude.view(), zlibLibrary.view()}));
    if (config.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::PackageBuildFailed);

    Process make;
    SC_TRY(make.setWorkingDirectory(buildRoot.view()));
    SC_TRY(make.exec({"make", "-j2"}));
    if (make.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::PackageBuildFailed);
    Process install;
    SC_TRY(install.setWorkingDirectory(buildRoot.view()));
    SC_TRY(install.exec({"make", "install_sw", "install_ssldirs"}));
    if (install.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::PackageBuildFailed);

    if (not fs.existsAndIsFile(cryptoLibrary.view()) or not fs.existsAndIsFile(sslLibrary.view()) or
        not fs.existsAndIsFile(sslHeader.view()) or not fs.existsAndIsFile(evpHeader.view()))
    {
        return Result::Error(PackageResultCategory, PackageError::PackageBuildArtifactMissing);
    }
    SC_TRY(runOpenSSLFilCSmoke(compiler.view(), package.installDirectoryLink.view(), zlib.installDirectoryLink.view(),
                               buildRoot.view()));
    static constexpr StringView phases[] = {
        "resolveOpenSSLSource",   "applyFilCOpenSSLPort", "buildOpenSSLWithFilC",
        "validateOpenSSLRuntime", "writeReceipt",
    };
    SC_TRY(writeManualPackageReceipt(package, packageName, version, variant, url, archiveHash.view(), exports, phases));
    SC_TRY(fs.writeString(package.packageLocalTxt.view(), identity.view()));
    return Result(true);
}

static Result runNGHTTP2FilCSmoke(StringView compiler, StringView packageRoot, StringView buildRoot)
{
    String source = StringEncoding::Utf8;
    String binary = StringEncoding::Utf8;
    SC_TRY(resolveToolSupportPath("NGHTTP2FilCSmoke.c", source));
    SC_TRY(Path::join(binary, {buildRoot, "nghttp2-filc-smoke"}));

    String  includeDirectory = format("-I{}/include", packageRoot);
    String  libraryDirectory = format("-L{}/lib", packageRoot);
    Process compile;
    SC_TRY(compile.exec({compiler, "-Wall", "-Wextra", "-Werror", source.view(), includeDirectory.view(),
                         libraryDirectory.view(), "-lnghttp2", "-o", binary.view()}));
    if (compile.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::ToolchainCompileFailed);

    String  runtimeLibraryPath = format("{}/lib", packageRoot);
    Process run;
    SC_TRY(run.setEnvironment("LD_LIBRARY_PATH", runtimeLibraryPath.view()));
    SC_TRY(run.exec({binary.view()}));
    if (run.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::ToolchainRunFailed);
    return Result(true);
}

Result installNGHTTP2FilC(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    if (HostInstructionSet != InstructionSet::ARM64 and HostInstructionSet != InstructionSet::Intel64)
        return Result::Error(PackageResultCategory, PackageError::InstallerArchitectureUnsupported);

    static constexpr StringView version = "1.70.0";
    static constexpr StringView url =
        "https://github.com/nghttp2/nghttp2/releases/download/v1.70.0/nghttp2-1.70.0.tar.gz";
    static constexpr StringView hash        = "aa317e2cf9dca6afa0aed68f8fad6ff303ec6982e25a78c75c0b65e2b9b3ded5";
    static constexpr StringView packageName = "nghttp2_filc";

    const StringView variant = filcRuntimeDependencyVariant();
    package.packageFullName  = "nghttp2-filc-1.70.0";
    package.packageBaseName  = "nghttp2-1.70.0.tar.gz";
    package.packageLocalFile = format("{}/nghttp2-filc/{}", packagesCacheDirectory, package.packageBaseName.view());
    package.packageLocalDirectory = format("{}/nghttp2-filc/source-{}", packagesCacheDirectory, version);
    package.packageLocalTxt       = format("{}/nghttp2-filc/{}.txt", packagesCacheDirectory, variant);
    package.installDirectoryLink  = format("{}/nghttp2_filc", packagesInstallDirectory);

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));
    SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalFile.view(), Path::AsNative)));

    Package filc;
    SC_TRY(installFilCToolchain(packagesCacheDirectory, packagesInstallDirectory, filc));
    String compiler   = StringEncoding::Utf8;
    String compilerXX = StringEncoding::Utf8;
    SC_TRY(resolveFilCRawCompilerPath(filc.installDirectoryLink.view(), "clang", compiler));
    SC_TRY(resolveFilCRawCompilerPath(filc.installDirectoryLink.view(), "clang++", compilerXX));
    String compilerVersion   = StringEncoding::Utf8;
    String compilerXXVersion = StringEncoding::Utf8;
    SC_TRY(probeFilCRuntimeDependencyCompiler(compiler.view(), compilerVersion));
    SC_TRY(probeFilCRuntimeDependencyCompiler(compilerXX.view(), compilerXXVersion));

    String buildRoot   = format("{}-build", package.installDirectoryLink.view());
    String archiveHash = format("sha256:{}", hash);
    String identity    = format("{}\nsha256:{}\n{}\n{}\nprofile=lib-only-shared\nwithout-openssl\nwithout-zlib\n", url,
                                hash, variant, compilerVersion.view());
    SC_TRY(StringBuilder::createForAppendingTo(identity).append("{}\n", compilerXXVersion.view()));
    String library       = format("{}/lib/libnghttp2.so.14", package.installDirectoryLink.view());
    String header        = format("{}/include/nghttp2/nghttp2.h", package.installDirectoryLink.view());
    String versionHeader = format("{}/include/nghttp2/nghttp2ver.h", package.installDirectoryLink.view());
    String receiptPath   = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(package.installDirectoryLink.view(), receiptPath));
    const PackageReceiptExport exports[] = {
        {PackageExportKind::Library, PackageExport::NGHTTP2Shared, "lib/libnghttp2.so.14"},
        {PackageExportKind::LibraryDir, PackageExport::NGHTTP2LibraryDir, "lib"},
        {PackageExportKind::IncludeDir, PackageExport::NGHTTP2IncludeDir, "include"},
        {PackageExportKind::Capability,
         HostInstructionSet == InstructionSet::ARM64 ? PackageCapability::LibraryNGHTTP2FilCArm64
                                                     : PackageCapability::LibraryNGHTTP2FilCX86_64,
         "lib/libnghttp2.so.14"},
    };

    String previous = StringEncoding::Utf8;
    if (fs.existsAndIsFile(library.view()) and fs.existsAndIsFile(header.view()) and
        fs.existsAndIsFile(versionHeader.view()) and fs.existsAndIsFile(receiptPath.view()) and
        readFileIntoString(package.packageLocalTxt.view(), previous) and previous.view() == identity.view())
    {
        bool receiptMatches = false;
        SC_TRY(filcRuntimeDependencyReceiptMatches(package.installDirectoryLink.view(), packageName, version, variant,
                                                   url, archiveHash.view(), exports, receiptMatches));
        if (receiptMatches)
        {
            SC_TRY(fs.makeDirectoryRecursive(buildRoot.view()));
            return runNGHTTP2FilCSmoke(compiler.view(), package.installDirectoryLink.view(), buildRoot.view());
        }
    }

    SC_TRY(downloadFileHash(url, package.packageLocalFile.view(), Hashing::TypeSHA256, hash));
    SC_TRY(extractTarArchiveFlatteningRoot(package.packageLocalFile.view(), package.packageLocalDirectory.view()));
    if (fs.existsAndIsDirectory(buildRoot.view()))
        SC_TRY(fs.removeDirectoriesRecursive(buildRoot.view()));
    SC_TRY(removePackageInstallLink(fs, package));
    SC_TRY(fs.makeDirectoryRecursive(buildRoot.view()));

    String  configure = format("{}/configure", package.packageLocalDirectory.view());
    String  prefix    = format("--prefix={}", package.installDirectoryLink.view());
    Process config;
    SC_TRY(config.setWorkingDirectory(buildRoot.view()));
    SC_TRY(config.setEnvironment("CC", compiler.view()));
    SC_TRY(config.setEnvironment("CXX", compilerXX.view()));
    SC_TRY(config.setEnvironment("PKG_CONFIG", "false"));
    SC_TRY(config.exec({"sh", configure.view(), prefix.view(), "--enable-lib-only", "--enable-shared",
                        "--disable-static", "--without-openssl", "--without-zlib"}));
    if (config.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::PackageBuildFailed);

    Process make;
    SC_TRY(make.setWorkingDirectory(buildRoot.view()));
    SC_TRY(make.exec({"make", "-j2"}));
    if (make.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::PackageBuildFailed);
    Process install;
    SC_TRY(install.setWorkingDirectory(buildRoot.view()));
    SC_TRY(install.exec({"make", "install"}));
    if (install.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::PackageBuildFailed);

    if (not fs.existsAndIsFile(library.view()) or not fs.existsAndIsFile(header.view()) or
        not fs.existsAndIsFile(versionHeader.view()))
    {
        return Result::Error(PackageResultCategory, PackageError::PackageBuildArtifactMissing);
    }
    SC_TRY(runNGHTTP2FilCSmoke(compiler.view(), package.installDirectoryLink.view(), buildRoot.view()));
    static constexpr StringView phases[] = {
        "resolveNGHTTP2Source",
        "buildNGHTTP2WithFilC",
        "validateNGHTTP2Runtime",
        "writeReceipt",
    };
    SC_TRY(writeManualPackageReceipt(package, packageName, version, variant, url, archiveHash.view(), exports, phases));
    SC_TRY(fs.writeString(package.packageLocalTxt.view(), identity.view()));
    return Result(true);
}
#else
Result installOpenSSLFilC(StringView, StringView, Package&)
{
    return Result::Error(PackageResultCategory, PackageError::InstallerHostUnsupported);
}

Result installNGHTTP2FilC(StringView, StringView, Package&)
{
    return Result::Error(PackageResultCategory, PackageError::InstallerHostUnsupported);
}
#endif
