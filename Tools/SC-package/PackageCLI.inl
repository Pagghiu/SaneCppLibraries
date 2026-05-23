// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

static Result printKnownPackages(Console& console, PackageRegistry registry)
{
    for (const PackageRegistryEntry& entry : registry.entries)
    {
        console.print(entry.name);
        console.print(" [");
        console.print(entry.kind);
        console.print("] ");
        console.printLine(entry.description);
    }
    return Result(true);
}

static Result printPackageHelp(Console& console)
{
    console.printLine("Usage: ./SC.sh package <action> [package] [options]");
    console.printLine("");
    console.printLine("Actions:");
    console.printLine("  install [package]       Install a package (default package: clang)");
    console.printLine("  list                    List known packages");
    console.printLine("  info <package>          Show package registry metadata");
    console.printLine("  status [package]        Show installed receipt status");
    console.printLine("  verify [package]        Verify installed package receipts and exports");
    console.printLine("  doctor [package]        Explain package receipt and export health");
    console.printLine("  repair <package>        Rebuild package receipts for valid existing layouts");
    console.printLine("  receipt <package>       Print the installed receipt JSON");
    console.printLine("  exports <package>       Print resolved receipt exports");
    console.printLine("  lock                    Write _Build/SC-package.lock");
    console.printLine("");
    console.printLine("Import-capable packages:");
    console.printLine("  qemu --import-directory <path>");
    console.printLine("  filc --import-directory <path>");
    console.printLine("  msvc --import-directory <path> --wine <path>");
    return Result(true);
}

static Result printStringViewList(Console& console, StringView label, Span<const StringView> values)
{
    console.print(label);
    console.print(" = ");
    if (values.empty())
    {
        console.printLine("-");
        return Result(true);
    }
    for (size_t idx = 0; idx < values.sizeInElements(); ++idx)
    {
        if (idx > 0)
        {
            console.print(", ");
        }
        console.print(values[idx]);
    }
    console.printLine(""_a8);
    return Result(true);
}

static Result printUnknownPackageError(Console& console, PackageRegistry registry, StringView packageName)
{
    console.print("Unknown package: ");
    console.printLine(packageName);
    console.print("Known packages: ");
    for (size_t idx = 0; idx < registry.entries.sizeInElements(); ++idx)
    {
        if (idx > 0)
        {
            console.print(", ");
        }
        console.print(registry.entries[idx].name);
    }
    console.printLine(""_a8);
    return Result::Error("Invalid package name");
}

Result runPackageTool(Tool::Arguments& arguments, PackageRegistry registry, Tools::Package* package)
{
    Console& console = arguments.console;

    SmallStringNative<256> packagesCacheDirectory;
    SmallStringNative<256> packagesInstallDirectory;
    SmallStringNative<256> buffer;

    auto builder = StringBuilder::create(buffer);
    SC_TRY(Path::join(packagesCacheDirectory, {arguments.toolDestination.view(), PackagesCacheDirectory}));
    SC_TRY(Path::join(packagesInstallDirectory, {arguments.toolDestination.view(), PackagesInstallDirectory}));
    SC_TRY(builder.append("packagesCache    = \"{}\"\n", packagesCacheDirectory.view()));
    SC_TRY(builder.append("packages         = \"{}\"", packagesInstallDirectory.view()));
    builder.finalize();
    console.printLine(buffer.view());

    Tools::Package clangPackage;
    if (package == nullptr)
    {
        package = &clangPackage;
    }
    auto packageNameFromArguments = [&]() -> StringView
    { return arguments.arguments.sizeInElements() > 0 ? arguments.arguments[0] : "clang"_a8; };

    if (arguments.action == "help" or arguments.action == "--help" or
        (arguments.action == "install" and arguments.arguments.sizeInElements() > 0 and
         (arguments.arguments[0] == "--help"_a8 or arguments.arguments[0] == "-h"_a8)))
    {
        SC_TRY(printPackageHelp(console));
    }
    else if (arguments.action == "install")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        if (entry->install != nullptr)
        {
            SC_TRY(entry->install(packagesCacheDirectory.view(), packagesInstallDirectory.view(), *package,
                                  arguments.arguments));
        }
        else if (entry->recipe != nullptr)
        {
            PackageRecipe recipe = *entry->recipe;
            if (recipe.download.packagesCacheDirectory.isEmpty())
            {
                SC_TRY(recipe.download.packagesCacheDirectory.assign(packagesCacheDirectory.view()));
            }
            if (recipe.download.packagesInstallDirectory.isEmpty())
            {
                SC_TRY(recipe.download.packagesInstallDirectory.assign(packagesInstallDirectory.view()));
            }
            if (recipe.download.packageName.isEmpty())
            {
                SC_TRY(recipe.download.packageName.assign(entry->installedName));
            }
            if (recipe.download.packageVersion.isEmpty())
            {
                SC_TRY(recipe.download.packageVersion.assign("local"));
            }
            if (recipe.package.installDirectoryLink.isEmpty())
            {
                SC_TRY(Path::join(recipe.package.installDirectoryLink,
                                  {packagesInstallDirectory.view(), entry->installedName}));
            }
            SC_TRY(installPackageRecipe(recipe, *package));
        }
        else
        {
            return Result::Error("Package registry entry is missing install handler or recipe");
        }
    }
    else if (arguments.action == "list")
    {
        SC_TRY(printKnownPackages(console, registry));
    }
    else if (arguments.action == "info")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        console.print("name            = ");
        console.printLine(entry->name);
        console.print("kind            = ");
        console.printLine(entry->kind);
        console.print("installedName   = ");
        console.printLine(entry->installedName);
        console.print("variants        = ");
        console.printLine(entry->variants);
        console.print("source          = ");
        console.printLine(entry->source);
        console.print("supportsImport  = ");
        console.printLine(entry->supportsImport ? "true"_a8 : "false"_a8);
        console.print("description     = ");
        console.printLine(entry->description);
        SC_TRY(printStringViewList(console, "exports        "_a8, entry->exports));
        SC_TRY(printStringViewList(console, "phases         "_a8, entry->phases));
    }
    else if (arguments.action == "status" or arguments.action == "verify")
    {
        if (arguments.arguments.empty())
        {
            SC_TRY(printAllPackageStatuses(console, registry, packagesInstallDirectory.view(),
                                           arguments.action == "verify"));
            return Result(true);
        }
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        String receiptPath = StringEncoding::Utf8;
        String packageRoot = StringEncoding::Utf8;
        bool   found       = false;
        SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory.view(), entry->installedName, receiptPath,
                                           packageRoot, found));
        if (not found)
        {
            console.print("not installed: ");
            console.printLine(entry->name);
            return arguments.action == "verify" ? Result::Error("Package receipt not found") : Result(true);
        }
        if (arguments.action == "verify")
        {
            SC_TRY(verifyPackageReceiptForEntry(*entry, receiptPath.view(), packageRoot.view()));
            console.print("verified: ");
        }
        else
        {
            console.print("installed: ");
        }
        console.print(entry->name);
        console.print(" at ");
        console.print(packageRoot.view());
        if (arguments.action == "status")
        {
            const Result validation = verifyPackageReceiptForEntry(*entry, receiptPath.view(), packageRoot.view());
            console.print(validation ? " (receipt valid)"_a8 : " (receipt invalid)"_a8);
        }
        console.printLine(""_a8);
    }
    else if (arguments.action == "doctor")
    {
        if (arguments.arguments.empty())
        {
            SC_TRY(printPackageDoctor(console, registry, packagesInstallDirectory.view(), nullptr));
            return Result(true);
        }
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        SC_TRY(printPackageDoctor(console, registry, packagesInstallDirectory.view(), entry));
    }
    else if (arguments.action == "repair")
    {
        SC_TRY_MSG(not arguments.arguments.empty(), "Package repair requires a package name");
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        SC_TRY(repairPackageReceipt(console, packagesInstallDirectory.view(), *entry));
    }
    else if (arguments.action == "receipt")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        SC_TRY(printPackageReceipt(console, packagesInstallDirectory.view(), *entry));
    }
    else if (arguments.action == "exports")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        SC_TRY(printPackageExports(console, packagesInstallDirectory.view(), *entry));
    }
    else if (arguments.action == "lock")
    {
        String lockPath = StringEncoding::Utf8;
        SC_TRY(Path::join(lockPath, {arguments.toolDestination.view(), "SC-package.lock"}));
        SC_TRY(lockInstalledPackages(packagesInstallDirectory.view(), lockPath.view()));
        console.print("lock            = ");
        console.printLine(lockPath.view());
    }
    else
    {
        SC_TRY(StringBuilder::format(buffer, "SC-package no action named \"{}\" exists", arguments.action));
        console.printLine(buffer.view());
        return Result::Error("SC-package error executing action");
    }
    return Result(true);
}

Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package)
{
    return runPackageTool(arguments, builtinPackageRegistry(), package);
}
