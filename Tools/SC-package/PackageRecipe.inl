// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

static Result copyPackageRecipeDirectory(const PackageRecipe& recipe, Package& package)
{
    SC_TRY_MSG(not recipe.copySourceDirectory.isEmpty(), "Package copy recipe is missing source directory");
    SC_TRY_MSG(not package.installDirectoryLink.isEmpty(), "Package copy recipe is missing install directory");

    FileSystem fs;
    SC_TRY(fs.init("."));
    if (fs.existsAndIsDirectory(package.installDirectoryLink.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(package.installDirectoryLink.view()));
    }
    else
    {
        SC_TRY(fs.removeFileIfExists(package.installDirectoryLink.view()));
    }
    return fs.copyDirectory(recipe.copySourceDirectory, package.installDirectoryLink.view());
}

static Result writeCopiedPackageRecipeReceipt(const PackageRecipe& recipe, Package& package)
{
    PackageReceiptInfo info;
    info.packageName    = recipe.download.packageName.view();
    info.packageVersion = recipe.download.packageVersion.isEmpty() ? "local"_a8 : recipe.download.packageVersion.view();
    info.recipeVersion  = "1";
    info.hostPlatform   = recipe.download.packagePlatform.view();
    info.packageVariant = recipe.download.packagePlatform.view();
    info.source         = recipe.copySourceDirectory;
    info.sourceHash     = {};
    info.validation     = "passed";
    info.phases         = recipe.phases;
    return writePackageReceipt(package, info, recipe.exports);
}

static Result runPackageRecipePhases(const PackageRecipe& recipe, Package& package, PackagePhaseRegistry phaseRegistry)
{
    for (StringView phaseName : recipe.phases)
    {
        const PackagePhaseRegistryEntry* phase = phaseRegistry.find(phaseName);
        SC_TRY_MSG(phase != nullptr, "Unknown package recipe phase");
        SC_TRY_MSG(phase->handler != nullptr, "Package recipe phase is missing handler");
        SC_TRY(phase->handler(recipe, package));
    }
    return Result(true);
}

static constexpr PackagePhaseRegistryEntry BuiltinPackagePhaseRegistryEntries[] = {
    {"copyDirectory", copyPackageRecipeDirectory},
    {"writeReceipt", writeCopiedPackageRecipeReceipt},
};

PackagePhaseRegistry builtinPackagePhaseRegistry() { return {BuiltinPackagePhaseRegistryEntries}; }

Result installPackageRecipe(const PackageRecipe& recipe, Package& package)
{
    package = recipe.package;
    if (recipe.kind == PackageRecipeKind::CopyDirectory)
    {
        if (recipe.phases.empty())
        {
            SC_TRY(copyPackageRecipeDirectory(recipe, package));
            return writeCopiedPackageRecipeReceipt(recipe, package);
        }
        const PackagePhaseRegistry phaseRegistry =
            recipe.phaseRegistry.entries.empty() ? builtinPackagePhaseRegistry() : recipe.phaseRegistry;
        return runPackageRecipePhases(recipe, package, phaseRegistry);
    }
    SC_TRY(packageInstall(recipe.download, package, recipe.functions));
    return writeDownloadPackageReceipt(recipe.download, package, recipe.exports, recipe.phases);
}
