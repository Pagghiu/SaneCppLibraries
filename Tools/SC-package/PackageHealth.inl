// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

static Result findInstalledPackageReceipt(StringView packagesInstallDirectory, StringView installedName,
                                          String& receiptPath, String& packageRoot, bool& found)
{
    found = false;
    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsDirectory(packagesInstallDirectory))
    {
        return Result(true);
    }

    FileSystemIterator::FolderState entries[8];
    FileSystemIterator              iterator;
    SC_TRY(iterator.init(packagesInstallDirectory, entries));

    auto checkReceipt = [&](StringView candidateReceiptPath, StringView candidatePackageRoot) -> Result
    {
        if (not fs.existsAndIsFile(candidateReceiptPath))
        {
            return Result(true);
        }
        String receipt = StringEncoding::Utf8;
        SC_TRY(readFileIntoString(candidateReceiptPath, receipt));
        PackageReceiptJSON receiptJSON;
        SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));
        if (receiptJSON.name.view() == installedName)
        {
            SC_TRY(receiptPath.assign(candidateReceiptPath));
            SC_TRY(packageRoot.assign(candidatePackageRoot));
            found = true;
        }
        return Result(true);
    };

    while (iterator.enumerateNext())
    {
        const FileSystemIterator::Entry entry            = iterator.get();
        String                          candidateReceipt = StringEncoding::Utf8;
        SC_TRY(Path::join(candidateReceipt, {entry.path, PackageReceiptFileName}));
        SC_TRY(checkReceipt(candidateReceipt.view(), entry.path));
        if (found)
        {
            return Result(true);
        }

        if (entry.isDirectory() or StringView(entry.name) != PackageReceiptFileName)
        {
            continue;
        }
        SC_TRY(checkReceipt(entry.path, Path::dirname(entry.path, Path::AsNative)));
        if (found)
        {
            return Result(true);
        }
    }
    SC_TRY(iterator.checkErrors());
    return Result(true);
}

static Result expectedPackageRepairRoot(StringView packagesInstallDirectory, const PackageRegistryEntry& entry,
                                        String& packageRoot)
{
    if (entry.name == "qemu"_a8)
    {
        const StringView installLeaf = qemuRunnerInstallLeafName();
        SC_TRY_MSG(not installLeaf.isEmpty(), "QEMU package repair is not supported on this host");
        SC_TRY(StringBuilder::format(packageRoot, "{}/qemu_{}", packagesInstallDirectory, installLeaf));
        return Result(true);
    }
    SC_TRY(Path::join(packageRoot, {packagesInstallDirectory, entry.installedName}));
    return Result(true);
}

static Result findPackageRepairRoot(StringView packagesInstallDirectory, const PackageRegistryEntry& entry,
                                    String& packageRoot)
{
    String receiptPath = StringEncoding::Utf8;
    bool   found       = false;
    SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot, found));
    if (found)
    {
        return Result(true);
    }

    SC_TRY(expectedPackageRepairRoot(packagesInstallDirectory, entry, packageRoot));
    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY_MSG(fs.existsAndIsDirectory(packageRoot.view()), "Package repair root not found");
    return Result(true);
}

static Result readRepairReceiptIfPresent(StringView packageRoot, PackageReceiptJSON& receiptJSON, bool& found)
{
    found = false;
    String receiptPath = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(packageRoot, receiptPath));

    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsFile(receiptPath.view()))
    {
        return Result(true);
    }

    String receipt = StringEncoding::Utf8;
    if (not readFileIntoString(receiptPath.view(), receipt))
    {
        return Result(true);
    }
    if (not readPackageReceiptJSON(receipt.view(), receiptJSON))
    {
        return Result(true);
    }
    found = true;
    return Result(true);
}

static Result writeRepairedPackageReceipt(StringView packageRoot, StringView packageName,
                                          Span<const PackageReceiptExport> exports,
                                          Span<const StringView> phases)
{
    PackageReceiptJSON existingReceipt;
    bool               hasExistingReceipt = false;
    SC_TRY(readRepairReceiptIfPresent(packageRoot, existingReceipt, hasExistingReceipt));

    String version    = StringEncoding::Utf8;
    String variant    = StringEncoding::Utf8;
    String source     = StringEncoding::Utf8;
    String sourceHash = StringEncoding::Utf8;
    SC_TRY(version.assign(hasExistingReceipt and not existingReceipt.version.isEmpty() ? existingReceipt.version.view()
                                                                                       : "repaired"_a8));
    SC_TRY(variant.assign(hasExistingReceipt and not existingReceipt.variant.isEmpty() ? existingReceipt.variant.view()
                                                                                       : hostPackagePlatformName()));
    SC_TRY(source.assign(hasExistingReceipt and not existingReceipt.source.isEmpty() ? existingReceipt.source.view()
                                                                                    : "repair"_a8));
    if (hasExistingReceipt and validatePackageReceiptSourceHash(existingReceipt.sourceHash.view()))
    {
        SC_TRY(sourceHash.assign(existingReceipt.sourceHash.view()));
    }

    Package package;
    SC_TRY(package.installDirectoryLink.assign(packageRoot));
    return writeManualPackageReceipt(package, packageName, version.view(), variant.view(), source.view(),
                                     sourceHash.view(), exports, phases);
}

static Result validateLLVMMingwPackageRoot(StringView packageRoot, Span<const PackageReceiptExport> exports)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    for (const PackageReceiptExport& packageExport : exports)
    {
        if (packageExport.relativePath == "."_a8)
        {
            continue;
        }
        String path = StringEncoding::Utf8;
        SC_TRY(Path::join(path, {packageRoot, packageExport.relativePath}));
        SC_TRY_MSG(fs.exists(path.view()), "llvm-mingw package is incomplete");
    }
    return Result(true);
}

static Result repairQEMUPackageReceipt(Console& console, StringView packagesInstallDirectory,
                                       const PackageRegistryEntry& entry)
{
    String packageRoot = StringEncoding::Utf8;
    SC_TRY(findPackageRepairRoot(packagesInstallDirectory, entry, packageRoot));
    SC_TRY(testQEMUPackageRoot(packageRoot.view()));

    String qemuX86_64Export = StringEncoding::Utf8;
    String qemuArm64Export  = StringEncoding::Utf8;
    SC_TRY(resolveQEMURunnerExecutableExport(packageRoot.view(), InstructionSet::Intel64, qemuX86_64Export));
    SC_TRY(resolveQEMURunnerExecutableExport(packageRoot.view(), InstructionSet::ARM64, qemuArm64Export));
    const PackageReceiptExport exports[] = {
        {PackageExportKind::Runner, PackageExport::RunnerQEMU, "."},
        {PackageExportKind::Capability, PackageCapability::RunnerQEMUX86_64, qemuX86_64Export.view()},
        {PackageExportKind::Capability, PackageCapability::RunnerQEMUArm64, qemuArm64Export.view()},
    };
    SC_TRY(writeRepairedPackageReceipt(packageRoot.view(), "qemu", exports, QEMUPhases));
    console.print("repaired: ");
    console.print(entry.name);
    console.print(" at ");
    console.printLine(packageRoot.view());
    return Result(true);
}

static Result repairLLVMMingwPackageReceipt(Console& console, StringView packagesInstallDirectory,
                                            const PackageRegistryEntry& entry)
{
    String packageRoot    = StringEncoding::Utf8;
    String x64Compiler    = StringEncoding::Utf8;
    String x64CompilerCpp = StringEncoding::Utf8;
    String arm64Compiler  = StringEncoding::Utf8;
    String arm64Cpp       = StringEncoding::Utf8;
    String archiver       = StringEncoding::Utf8;
    SC_TRY(findPackageRepairRoot(packagesInstallDirectory, entry, packageRoot));
    SC_TRY(Path::join(x64Compiler, {"bin", "x86_64-w64-mingw32-clang"}));
    SC_TRY(Path::join(x64CompilerCpp, {"bin", "x86_64-w64-mingw32-clang++"}));
    SC_TRY(Path::join(arm64Compiler, {"bin", "aarch64-w64-mingw32-clang"}));
    SC_TRY(Path::join(arm64Cpp, {"bin", "aarch64-w64-mingw32-clang++"}));
    SC_TRY(Path::join(archiver, {"bin", "llvm-ar"}));

    const PackageReceiptExport exports[] = {
        {PackageExportKind::Tool, PackageExport::LLVMMinGWClang_X86_64, x64Compiler.view()},
        {PackageExportKind::Tool, PackageExport::LLVMMinGWClangXX_X86_64, x64CompilerCpp.view()},
        {PackageExportKind::Tool, PackageExport::LLVMMinGWClangArm64, arm64Compiler.view()},
        {PackageExportKind::Tool, PackageExport::LLVMMinGWClangXXArm64, arm64Cpp.view()},
        {PackageExportKind::Tool, PackageExport::LLVMAr, archiver.view()},
        {PackageExportKind::Capability, PackageCapability::ToolchainWindowsGNUX86_64, x64Compiler.view()},
        {PackageExportKind::Capability, PackageCapability::ToolchainWindowsGNUArm64, arm64Compiler.view()},
    };
    SC_TRY(validateLLVMMingwPackageRoot(packageRoot.view(), exports));
    SC_TRY(writeRepairedPackageReceipt(packageRoot.view(), "llvm-mingw", exports, LLVMMingwPhases));
    console.print("repaired: ");
    console.print(entry.name);
    console.print(" at ");
    console.printLine(packageRoot.view());
    return Result(true);
}

static Result repairPackageReceipt(Console& console, StringView packagesInstallDirectory,
                                   const PackageRegistryEntry& entry)
{
    if (entry.name == "qemu"_a8)
    {
        return repairQEMUPackageReceipt(console, packagesInstallDirectory, entry);
    }
    if (entry.name == "llvm-mingw"_a8)
    {
        return repairLLVMMingwPackageReceipt(console, packagesInstallDirectory, entry);
    }
    return Result::Error("Package repair is not implemented for this package");
}

static Result verifyPackageReceipt(StringView receiptPath, StringView packageRoot)
{
    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath, receipt));

    PackageReceiptJSON receiptJSON;
    SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));

    SC_TRY(validatePackageReceiptHeader(receiptJSON));
    SC_TRY_MSG(not receiptJSON.version.isEmpty(), "Package receipt is missing package version");
    SC_TRY_MSG(not receiptJSON.source.isEmpty(), "Package receipt is missing source");
    SC_TRY_MSG(not receiptJSON.installRoot.isEmpty(), "Package receipt is missing install root");
    SC_TRY_MSG(receiptJSON.validation.view() == "passed"_a8, "Package receipt validation did not pass");
    SC_TRY(validatePackageReceiptSourceHash(receiptJSON.sourceHash.view()));

    FileSystem fs;
    SC_TRY(fs.init("."));
    for (PackageReceiptExportJSON& exportView : receiptJSON.exports)
    {
        SC_TRY_MSG(not exportView.kind.isEmpty(), "Package receipt export is missing kind");
        SC_TRY_MSG(not exportView.name.isEmpty(), "Package receipt export is missing name");
        SC_TRY(validatePackageReceiptExportPath(exportView.path.view()));
        if (exportView.path.view() == "."_a8)
        {
            continue;
        }
        String exportedPath = StringEncoding::Utf8;
        SC_TRY(Path::join(exportedPath, {packageRoot, exportView.path.view()}));
        SC_TRY_MSG(fs.exists(exportedPath.view()), "Package receipt export is missing");
    }
    return Result(true);
}

static Result verifyPackageReceiptMatchesRegistryExports(const PackageRegistryEntry& entry,
                                                         const PackageReceiptJSON&   receiptJSON)
{
    for (const PackageRegistryExport& expectedExport : entry.exports)
    {
        if (expectedExport.name.containsString("<"))
        {
            continue;
        }

        bool found = false;
        for (const PackageReceiptExportJSON& exportView : receiptJSON.exports)
        {
            if (exportView.kind.view() == expectedExport.kind and exportView.name.view() == expectedExport.name)
            {
                found = true;
                break;
            }
        }
        SC_TRY_MSG(found, "Package receipt is missing registry export");
    }
    return Result(true);
}

static Result verifyPackageReceiptForEntry(const PackageRegistryEntry& entry, StringView receiptPath,
                                           StringView packageRoot)
{
    SC_TRY(verifyPackageReceipt(receiptPath, packageRoot));
    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath, receipt));
    PackageReceiptJSON receiptJSON;
    SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));
    SC_TRY_MSG(receiptJSON.name.view() == entry.installedName,
               "Package receipt identity does not match registry entry");
    SC_TRY(verifyPackageReceiptMatchesRegistryExports(entry, receiptJSON));
    return Result(true);
}

static Result printAllPackageStatuses(Console& console, PackageRegistry registry, StringView packagesInstallDirectory,
                                      bool verify)
{
    size_t verifiedCount = 0;
    for (const PackageRegistryEntry& entry : registry.entries)
    {
        String receiptPath = StringEncoding::Utf8;
        String packageRoot = StringEncoding::Utf8;
        bool   found       = false;
        SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot,
                                           found));
        if (not found)
        {
            if (not verify)
            {
                console.print("not installed: ");
                console.printLine(entry.name);
            }
            continue;
        }
        if (verify)
        {
            SC_TRY(verifyPackageReceiptForEntry(entry, receiptPath.view(), packageRoot.view()));
            console.print("verified: ");
            verifiedCount += 1;
        }
        else
        {
            console.print("installed: ");
        }
        console.print(entry.name);
        console.print(" at ");
        console.print(packageRoot.view());
        if (not verify)
        {
            const Result validation = verifyPackageReceiptForEntry(entry, receiptPath.view(), packageRoot.view());
            console.print(validation ? " (receipt valid)"_a8 : " (receipt invalid)"_a8);
        }
        console.printLine(""_a8);
    }
    if (verify and verifiedCount == 0)
    {
        console.printLine("no installed package receipts found");
    }
    return Result(true);
}

static Result printPackageDoctorForEntry(Console& console, StringView packagesInstallDirectory,
                                         const PackageRegistryEntry& entry, bool& hasIssues)
{
    String receiptPath = StringEncoding::Utf8;
    String packageRoot = StringEncoding::Utf8;
    bool   found       = false;
    SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot, found));
    if (not found)
    {
        hasIssues = true;
        console.print("missing: ");
        console.printLine(entry.name);
        console.print("  expected receipt for installedName: ");
        console.printLine(entry.installedName);
        console.print("  suggested action: ./SC.sh package install ");
        console.printLine(entry.name);
        return Result(true);
    }

    const Result validation = verifyPackageReceiptForEntry(entry, receiptPath.view(), packageRoot.view());
    if (validation)
    {
        console.print("healthy: ");
        console.print(entry.name);
        console.print(" at ");
        console.printLine(packageRoot.view());
        return Result(true);
    }

    hasIssues = true;
    console.print("problem: ");
    console.print(entry.name);
    console.print(" at ");
    console.printLine(packageRoot.view());
    console.print("  receipt: ");
    console.printLine(receiptPath.view());
    console.print("  reason: ");
    console.printLine(StringView::fromNullTerminated(validation.message, StringEncoding::Ascii));
    console.printLine("  suggested action: re-run install or remove the stale package directory before reinstalling");
    return Result(true);
}

static Result printLegacyPackageSidecars(Console& console, StringView packagesInstallDirectory, bool& hasIssues)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsDirectory(packagesInstallDirectory))
    {
        return Result(true);
    }

    FileSystemIterator::FolderState entries[8];
    FileSystemIterator              iterator;
    SC_TRY(iterator.init(packagesInstallDirectory, entries));
    while (iterator.enumerateNext())
    {
        const FileSystemIterator::Entry entry = iterator.get();
        if (not entry.isDirectory() and StringView(entry.name).endsWith(".txt"))
        {
            hasIssues = true;
            console.print("legacy sidecar: ");
            console.printLine(entry.path);
            console.printLine("  suggested action: reinstall the package so a structured receipt is written");
        }
    }
    SC_TRY(iterator.checkErrors());
    return Result(true);
}

static Result printPackageDoctor(Console& console, PackageRegistry registry, StringView packagesInstallDirectory,
                                 const PackageRegistryEntry* singleEntry)
{
    bool hasIssues = false;
    if (singleEntry != nullptr)
    {
        SC_TRY(printPackageDoctorForEntry(console, packagesInstallDirectory, *singleEntry, hasIssues));
    }
    else
    {
        for (const PackageRegistryEntry& entry : registry.entries)
        {
            SC_TRY(printPackageDoctorForEntry(console, packagesInstallDirectory, entry, hasIssues));
        }
        SC_TRY(printLegacyPackageSidecars(console, packagesInstallDirectory, hasIssues));
    }

    console.printLine(hasIssues ? "doctor: issues found"_a8 : "doctor: ok"_a8);
    return Result(true);
}

static Result printPackageReceipt(Console& console, StringView packagesInstallDirectory,
                                  const PackageRegistryEntry& entry)
{
    String receiptPath = StringEncoding::Utf8;
    String packageRoot = StringEncoding::Utf8;
    bool   found       = false;
    SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot, found));
    if (not found)
    {
        console.print("not installed: ");
        console.printLine(entry.name);
        return Result::Error("Package receipt not found");
    }

    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath.view(), receipt));

    console.print("packageRoot     = ");
    console.printLine(packageRoot.view());
    console.print("receipt         = ");
    console.printLine(receiptPath.view());
    console.printLine(receipt.view());
    return Result(true);
}

static Result printPackageExports(Console& console, StringView packagesInstallDirectory,
                                  const PackageRegistryEntry& entry)
{
    String receiptPath = StringEncoding::Utf8;
    String packageRoot = StringEncoding::Utf8;
    bool   found       = false;
    SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot, found));
    if (not found)
    {
        console.print("not installed: ");
        console.printLine(entry.name);
        return Result::Error("Package receipt not found");
    }

    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath.view(), receipt));
    console.print("packageRoot     = ");
    console.printLine(packageRoot.view());
    console.print("receipt         = ");
    console.printLine(receiptPath.view());

    bool hasExports = false;
    SC_TRY(forEachReceiptExport(receipt.view(),
                                [&](const PackageReceiptExportJSON& exportView) -> Result
                                {
                                    String exportedPath = StringEncoding::Utf8;
                                    SC_TRY(Path::join(exportedPath, {packageRoot.view(), exportView.path.view()}));
                                    console.print(exportView.kind.view());
                                    console.print(":");
                                    console.print(exportView.name.view());
                                    console.print(" = ");
                                    console.printLine(exportedPath.view());
                                    hasExports = true;
                                    return Result(true);
                                }));
    if (not hasExports)
    {
        console.printLine("no exports");
    }
    return Result(true);
}

static bool isSmaller(StringView left, StringView right)
{
    return left.compare(right) == StringView::Comparison::Smaller;
}

static bool isPackageLockExportSmaller(const PackageReceiptExportJSON& left, const PackageReceiptExportJSON& right)
{
    if (left.kind.view() != right.kind.view())
    {
        return isSmaller(left.kind.view(), right.kind.view());
    }
    if (left.name.view() != right.name.view())
    {
        return isSmaller(left.name.view(), right.name.view());
    }
    return isSmaller(left.path.view(), right.path.view());
}

static bool isPackageLockEntrySmaller(const PackageLockEntryJSON& left, const PackageLockEntryJSON& right)
{
    if (left.name.view() != right.name.view())
    {
        return isSmaller(left.name.view(), right.name.view());
    }
    if (left.variant.view() != right.variant.view())
    {
        return isSmaller(left.variant.view(), right.variant.view());
    }
    return isSmaller(left.installRoot.view(), right.installRoot.view());
}

static void sortPackageLock(PackageLockJSON& lockJSON)
{
    for (PackageLockEntryJSON& entry : lockJSON.packages)
    {
        Algorithms::bubbleSort(entry.exports.begin(), entry.exports.end(), isPackageLockExportSmaller);
    }
    Algorithms::bubbleSort(lockJSON.packages.begin(), lockJSON.packages.end(), isPackageLockEntrySmaller);
}

static Result lockInstalledPackages(StringView packagesInstallDirectory, StringView lockPath)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    PackageLockJSON lockJSON;
    SC_TRY(assignJSONField(lockJSON.tool, "SC-package"));
    SC_TRY(assignJSONField(lockJSON.toolVersion, "1"));
    SC_TRY(StringBuilder::format(lockJSON.generatedAt, "{}", Time::Realtime::now().milliseconds));
    SC_TRY(assignJSONField(lockJSON.hostPlatform, hostPlatformName()));
    SC_TRY(assignJSONField(lockJSON.hostArch, hostInstructionSetName()));
    auto appendReceiptObject = [&](StringView receiptPath) -> Result
    {
        if (not fs.existsAndIsFile(receiptPath))
        {
            return Result(true);
        }
        String receipt = StringEncoding::Utf8;
        SC_TRY(readFileIntoString(receiptPath, receipt));
        SC_TRY(verifyPackageReceipt(receiptPath, Path::dirname(receiptPath, Path::AsNative)));

        PackageReceiptJSON receiptJSON;
        SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));

        PackageLockEntryJSON entry;
        SC_TRY(assignJSONField(entry.name, receiptJSON.name.view()));
        SC_TRY(assignJSONField(entry.version, receiptJSON.version.view()));
        SC_TRY(assignJSONField(entry.recipeVersion, receiptJSON.recipeVersion.view()));
        SC_TRY(assignJSONField(entry.hostPlatform, receiptJSON.hostPlatform.view()));
        SC_TRY(assignJSONField(entry.variant, receiptJSON.variant.view()));
        SC_TRY(assignJSONField(entry.source, receiptJSON.source.view()));
        SC_TRY(assignJSONField(entry.sourceHash, receiptJSON.sourceHash.view()));
        SC_TRY(assignJSONField(entry.installRoot, receiptJSON.installRoot.view()));
        SC_TRY(assignJSONField(entry.receipt, receiptPath));
        for (PackageReceiptExportJSON& exportView : receiptJSON.exports)
        {
            SC_TRY(appendJSONExport(entry.exports, exportView.kind.view(), exportView.name.view(),
                                    exportView.path.view()));
        }
        SC_TRY(lockJSON.packages.push_back(move(entry)));
        lockJSON.packageCount = static_cast<int>(lockJSON.packages.size());
        return Result(true);
    };

    if (fs.existsAndIsDirectory(packagesInstallDirectory))
    {
        FileSystemIterator::FolderState entries[8];
        FileSystemIterator              iterator;
        SC_TRY(iterator.init(packagesInstallDirectory, entries));
        while (iterator.enumerateNext())
        {
            const FileSystemIterator::Entry entry            = iterator.get();
            String                          candidateReceipt = StringEncoding::Utf8;
            SC_TRY(Path::join(candidateReceipt, {entry.path, PackageReceiptFileName}));
            SC_TRY(appendReceiptObject(candidateReceipt.view()));

            if (entry.isDirectory() or StringView(entry.name) != PackageReceiptFileName)
            {
                continue;
            }
            SC_TRY(appendReceiptObject(entry.path));
        }
        SC_TRY(iterator.checkErrors());
    }

    sortPackageLock(lockJSON);
    String lock = StringEncoding::Utf8;
    SC_TRY_MSG(SerializationJson::write(lockJSON, lock), "Failed writing package lock JSON");
    return fs.writeString(lockPath, lock.view());
}
