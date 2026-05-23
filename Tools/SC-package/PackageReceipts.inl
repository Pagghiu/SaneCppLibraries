// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

struct PackageReceiptExportJSON
{
    String kind = StringEncoding::Utf8;
    String name = StringEncoding::Utf8;
    String path = StringEncoding::Utf8;
};

struct PackageReceiptJSON
{
    int    schema        = 1;
    String name          = StringEncoding::Utf8;
    String version       = StringEncoding::Utf8;
    String recipeVersion = StringEncoding::Utf8;
    String hostPlatform  = StringEncoding::Utf8;
    String variant       = StringEncoding::Utf8;
    String source        = StringEncoding::Utf8;
    String sourceHash    = StringEncoding::Utf8;
    String installRoot   = StringEncoding::Utf8;
    String validation    = StringEncoding::Utf8;

    Vector<String>                   phases;
    Vector<PackageReceiptExportJSON> exports;
};

struct PackageLockEntryJSON
{
    String name          = StringEncoding::Utf8;
    String version       = StringEncoding::Utf8;
    String recipeVersion = StringEncoding::Utf8;
    String hostPlatform  = StringEncoding::Utf8;
    String variant       = StringEncoding::Utf8;
    String source        = StringEncoding::Utf8;
    String sourceHash    = StringEncoding::Utf8;
    String installRoot   = StringEncoding::Utf8;
    String receipt       = StringEncoding::Utf8;

    Vector<PackageReceiptExportJSON> exports;
};

struct PackageLockJSON
{
    int    schema       = 2;
    String tool         = StringEncoding::Utf8;
    String toolVersion  = StringEncoding::Utf8;
    String generatedAt  = StringEncoding::Utf8;
    String hostPlatform = StringEncoding::Utf8;
    String hostArch     = StringEncoding::Utf8;
    int    packageCount = 0;

    Vector<PackageLockEntryJSON> packages;
};

} // namespace Tools
} // namespace SC

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageReceiptExportJSON)
SC_REFLECT_STRUCT_FIELD(0, kind)
SC_REFLECT_STRUCT_FIELD(1, name)
SC_REFLECT_STRUCT_FIELD(2, path)
SC_REFLECT_STRUCT_LEAVE()

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageReceiptJSON)
SC_REFLECT_STRUCT_FIELD(0, schema)
SC_REFLECT_STRUCT_FIELD(1, name)
SC_REFLECT_STRUCT_FIELD(2, version)
SC_REFLECT_STRUCT_FIELD(3, recipeVersion)
SC_REFLECT_STRUCT_FIELD(4, hostPlatform)
SC_REFLECT_STRUCT_FIELD(5, variant)
SC_REFLECT_STRUCT_FIELD(6, source)
SC_REFLECT_STRUCT_FIELD(7, sourceHash)
SC_REFLECT_STRUCT_FIELD(8, installRoot)
SC_REFLECT_STRUCT_FIELD(9, validation)
SC_REFLECT_STRUCT_FIELD(10, phases)
SC_REFLECT_STRUCT_FIELD(11, exports)
SC_REFLECT_STRUCT_LEAVE()

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageLockEntryJSON)
SC_REFLECT_STRUCT_FIELD(0, name)
SC_REFLECT_STRUCT_FIELD(1, version)
SC_REFLECT_STRUCT_FIELD(2, recipeVersion)
SC_REFLECT_STRUCT_FIELD(3, hostPlatform)
SC_REFLECT_STRUCT_FIELD(4, variant)
SC_REFLECT_STRUCT_FIELD(5, source)
SC_REFLECT_STRUCT_FIELD(6, sourceHash)
SC_REFLECT_STRUCT_FIELD(7, installRoot)
SC_REFLECT_STRUCT_FIELD(8, receipt)
SC_REFLECT_STRUCT_FIELD(9, exports)
SC_REFLECT_STRUCT_LEAVE()

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageLockJSON)
SC_REFLECT_STRUCT_FIELD(0, schema)
SC_REFLECT_STRUCT_FIELD(1, tool)
SC_REFLECT_STRUCT_FIELD(2, toolVersion)
SC_REFLECT_STRUCT_FIELD(3, generatedAt)
SC_REFLECT_STRUCT_FIELD(4, hostPlatform)
SC_REFLECT_STRUCT_FIELD(5, hostArch)
SC_REFLECT_STRUCT_FIELD(6, packageCount)
SC_REFLECT_STRUCT_FIELD(7, packages)
SC_REFLECT_STRUCT_LEAVE()

namespace SC
{
namespace Tools
{

static Result assignJSONField(String& output, StringView value)
{
    SC_TRY(output.assign(value));
    return Result(true);
}

static Result appendJSONString(Vector<String>& output, StringView value)
{
    String item = StringEncoding::Utf8;
    SC_TRY(item.assign(value));
    SC_TRY(output.push_back(move(item)));
    return Result(true);
}

static Result appendJSONExport(Vector<PackageReceiptExportJSON>& output, StringView kind, StringView name,
                               StringView path)
{
    PackageReceiptExportJSON item;
    SC_TRY(assignJSONField(item.kind, kind));
    SC_TRY(assignJSONField(item.name, name));
    SC_TRY(assignJSONField(item.path, path));
    SC_TRY(output.push_back(move(item)));
    return Result(true);
}

static Result validatePackageReceiptExportPath(StringView path)
{
    SC_TRY_MSG(not path.isEmpty(), "Package receipt export is missing path");
    if (path == "."_a8)
    {
        return Result(true);
    }
    SC_TRY_MSG(not Path::isAbsolute(path, Path::AsPosix) and not Path::isAbsolute(path, Path::AsWindows),
               "Package receipt export path must be relative");

    StringViewTokenizer tokenizer(path);
    while (tokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::SkipEmpty))
    {
        SC_TRY_MSG(tokenizer.component != ".."_a8, "Package receipt export path cannot escape package root");
    }
    return Result(true);
}

static Result resolvePackageReceiptExportNativePath(StringView packageRoot, StringView exportPath, String& output)
{
    if (exportPath == "."_a8)
    {
        SC_TRY(output.assign(packageRoot));
        return Result(true);
    }

    StringView          components[64];
    size_t              numComponents = 0;
    StringViewTokenizer tokenizer(exportPath);
    while (tokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::SkipEmpty))
    {
        SC_TRY_MSG(numComponents < sizeof(components) / sizeof(components[0]),
                   "Package receipt export path has too many components");
        components[numComponents] = tokenizer.component;
        numComponents += 1;
    }

    SC_TRY(output.assign(packageRoot));
    SC_TRY_MSG(Path::append(output, {components, numComponents}, Path::AsNative),
               "Failed resolving package receipt export path");
    return Result(true);
}

static Result validatePackageReceiptSourceHash(StringView sourceHash)
{
    if (sourceHash.isEmpty())
    {
        return Result(true);
    }
    StringView algorithm;
    StringView digest;
    SC_TRY_MSG(sourceHash.splitBefore(":"_a8, algorithm) and sourceHash.splitAfter(":"_a8, digest),
               "Package receipt source hash is missing algorithm");
    SC_TRY_MSG(algorithm == "md5"_a8 or algorithm == "sha1"_a8 or algorithm == "sha256"_a8,
               "Package receipt source hash has an unsupported algorithm");
    SC_TRY_MSG(not digest.isEmpty(), "Package receipt source hash is missing digest");
    return Result(true);
}

static constexpr StringView hostPackagePlatformName()
{
    switch (HostPlatform)
    {
    case Platform::Apple: return "macos"_a8;
    case Platform::Linux: return "linux"_a8;
    case Platform::Windows: return "windows"_a8;
    case Platform::Emscripten: return "emscripten"_a8;
    }
    Assert::unreachable();
}

static Result packageReceiptPath(StringView packageRoot, String& output)
{
    SC_TRY(Path::join(output, {packageRoot, PackageReceiptFileName}));
    return Result(true);
}

Result writePackageReceipt(const Package& package, const PackageReceiptInfo& info,
                           Span<const PackageReceiptExport> exports)
{
    SC_TRY_MSG(not package.installDirectoryLink.isEmpty(), "Cannot write package receipt without install root");

    String receiptPath = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(package.installDirectoryLink.view(), receiptPath));

    String sourceHash = StringEncoding::Utf8;
    if (not info.sourceHash.isEmpty())
    {
        SC_TRY(validatePackageReceiptSourceHash(info.sourceHash));
        SC_TRY(sourceHash.assign(info.sourceHash));
    }

    PackageReceiptJSON receiptJSON;
    SC_TRY(assignJSONField(receiptJSON.name, info.packageName));
    SC_TRY(assignJSONField(receiptJSON.version, info.packageVersion));
    SC_TRY(assignJSONField(receiptJSON.recipeVersion, info.recipeVersion.isEmpty() ? "1"_a8 : info.recipeVersion));
    SC_TRY(assignJSONField(receiptJSON.hostPlatform,
                           info.hostPlatform.isEmpty() ? hostPackagePlatformName() : info.hostPlatform));
    SC_TRY(assignJSONField(receiptJSON.variant, info.packageVariant));
    SC_TRY(assignJSONField(receiptJSON.source, info.source));
    SC_TRY(assignJSONField(receiptJSON.sourceHash, sourceHash.view()));
    SC_TRY(assignJSONField(receiptJSON.installRoot, package.installDirectoryLink.view()));
    SC_TRY(assignJSONField(receiptJSON.validation, info.validation));
    for (size_t idx = 0; idx < info.phases.sizeInElements(); ++idx)
    {
        SC_TRY(appendJSONString(receiptJSON.phases, info.phases[idx]));
    }
    for (size_t idx = 0; idx < exports.sizeInElements(); ++idx)
    {
        SC_TRY(appendJSONExport(receiptJSON.exports, exports[idx].kind, exports[idx].name, exports[idx].relativePath));
    }

    String receipt = StringEncoding::Utf8;
    SC_TRY_MSG(SerializationJson::write(receiptJSON, receipt), "Failed writing package receipt JSON");

    FileSystem fs;
    SC_TRY(fs.init("."));
    return fs.writeString(receiptPath.view(), receipt.view());
}

static Result downloadReceiptInfo(const Download& download, PackageReceiptInfo& info, String& sourceHash)
{
    SC_TRY(sourceHash.assign({}));
    if (not download.expectedHash.isEmpty())
    {
        SC_TRY(StringBuilder::format(sourceHash, "{}:{}", packageHashName(download.hashType), download.expectedHash));
    }
    info.packageName    = download.packageName.view();
    info.packageVersion = download.packageVersion.view();
    info.recipeVersion  = "1";
    info.hostPlatform   = hostPackagePlatformName();
    info.packageVariant = download.packagePlatform.view();
    info.source         = download.url.view();
    info.sourceHash     = sourceHash.view();
    info.validation     = "passed";
    return Result(true);
}

static Result writeDownloadPackageReceipt(const Download& download, const Package& package,
                                          Span<const PackageReceiptExport> exports = {},
                                          Span<const StringView>           phases  = {})
{
    PackageReceiptInfo info;
    String             sourceHash = StringEncoding::Utf8;
    SC_TRY(downloadReceiptInfo(download, info, sourceHash));
    info.phases = phases;
    return writePackageReceipt(package, info, exports);
}

static Result writeManualPackageReceipt(const Package& package, StringView name, StringView version, StringView variant,
                                        StringView source, StringView sourceHash,
                                        Span<const PackageReceiptExport> exports, Span<const StringView> phases)
{
    PackageReceiptInfo info;
    info.packageName    = name;
    info.packageVersion = version;
    info.recipeVersion  = "1";
    info.hostPlatform   = hostPackagePlatformName();
    info.packageVariant = variant;
    info.source         = source;
    info.sourceHash     = sourceHash;
    info.validation     = "passed";
    info.phases         = phases;
    return writePackageReceipt(package, info, exports);
}

static Result readPackageReceiptJSON(StringView receipt, PackageReceiptJSON& output)
{
    SC_TRY_MSG(SerializationJson::loadVersioned(output, receipt), "Malformed package receipt JSON");
    return Result(true);
}

static Result validatePackageReceiptHeader(const PackageReceiptJSON& receiptJSON)
{
    SC_TRY_MSG(receiptJSON.schema == 1, "Package receipt schema is unsupported");
    SC_TRY_MSG(not receiptJSON.name.isEmpty(), "Package receipt is missing package name");
    return Result(true);
}

template <typename Callback>
static Result forEachReceiptExport(StringView receipt, Callback&& callback)
{
    PackageReceiptJSON receiptJSON;
    SC_TRY(readPackageReceiptJSON(receipt, receiptJSON));
    SC_TRY(validatePackageReceiptHeader(receiptJSON));
    for (auto& exportView : receiptJSON.exports)
    {
        SC_TRY(callback(exportView));
    }
    return Result(true);
}

static Result resolvePackageReceiptExportPath(StringView packageRoot, StringView exportKind, StringView exportName,
                                              String& output)
{
    String receiptPath = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(packageRoot, receiptPath));

    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath.view(), receipt));

    bool found = false;
    SC_TRY(forEachReceiptExport(
        receipt.view(),
        [&](const PackageReceiptExportJSON& exportView) -> Result
        {
            if ((exportKind.isEmpty() or exportView.kind.view() == exportKind) and exportView.name.view() == exportName)
            {
                SC_TRY_MSG(not found, "Package receipt export is duplicated");
                SC_TRY(validatePackageReceiptExportPath(exportView.path.view()));
                SC_TRY(resolvePackageReceiptExportNativePath(packageRoot, exportView.path.view(), output));
                found = true;
            }
            return Result(true);
        }));
    return found ? Result(true) : Result::Error("Package export not found");
}

Result resolvePackageExportPath(StringView packageRoot, StringView exportName, String& output)
{
    return resolvePackageReceiptExportPath(packageRoot, {}, exportName, output);
}

Result resolvePackageCapabilityPath(StringView packageRoot, StringView capabilityName, String& output)
{
    return resolvePackageReceiptExportPath(packageRoot, "capability"_a8, capabilityName, output);
}
