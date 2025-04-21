// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Tools.h"

#include "../Libraries/File/File.h"
#include "../Libraries/FileSystem/FileSystem.h"
#include "../Libraries/FileSystem/Path.h"
#include "../Libraries/Foundation/Function.h"
#include "../Libraries/Hashing/Hashing.h"
#include "../Libraries/Process/Process.h"
#include "../Libraries/Strings/Console.h"
#include "../Libraries/Strings/StringBuilder.h"

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
    SmallString<255> fileMD5;

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

struct CustomFunctions
{
    Function<Result(const Download&, const Package&)> testFunction;
    Function<Result(StringView, StringView)>          extractFunction;
};

[[nodiscard]] inline Result createLink(StringView sourceFileOrDirectory, StringView linkFile)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    return fs.createSymbolicLink(sourceFileOrDirectory, linkFile);
}

[[nodiscard]] inline Result removeQuarantineAttribute(StringView directory)
{
    Process process;
    switch (HostPlatform)
    {
    case Platform::Apple:
        SC_TRY(process.exec({"xattr", "-r", "-d", "com.apple.quarantine", directory}));
        SC_TRY_MSG(process.getExitStatus() == 0, "xattr failed");
        break;
    case Platform::Linux:
    case Platform::Windows:
    case Platform::Emscripten:
        // Nothing to do
        break;
    }
    return Result(true);
}

[[nodiscard]] inline Result checkFileMD5(StringView fileName, StringView wantedMD5)
{
    FileDescriptor fd;
    SC_TRY(File(fd).open(fileName, File::OpenMode::ReadOnly));
    Hashing hashing;
    SC_TRY(hashing.setType(Hashing::TypeMD5));
    for (;;)
    {
        uint8_t       data[4096];
        Span<uint8_t> actuallyRead;
        SC_TRY(fd.read({data, sizeof(data)}, actuallyRead));
        if (actuallyRead.sizeInBytes() > 0)
        {
            SC_TRY(hashing.add(actuallyRead));
        }
        else
        {
            SC_TRY(fd.close());
            Hashing::Result res;
            SC_TRY(hashing.getHash(res));
            SmallString<32> result;
            SC_TRY(StringBuilder(result).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::LowerCase));
            SC_TRY_MSG(result.view() == wantedMD5, "MD5 doesn't match");
            return Result(true);
        }
    }
}

[[nodiscard]] inline Result downloadFileMD5(StringView remoteURL, StringView localFile, StringView localFileMD5)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsFile(localFile) or not checkFileMD5(localFile, localFileMD5))
    {
        Process process;
        SC_TRY(process.exec({"curl", "-L", "-o", localFile, remoteURL}));
        SC_TRY_MSG(process.getExitStatus() == 0, "Cannot download file");
        SC_TRY(checkFileMD5(localFile, localFileMD5));
    }
    return Result(true);
}

[[nodiscard]] inline Result tarExpandTo(StringView fileName, StringView directory, int stripComponents = 0)
{
    Process          process;
    SmallString<255> stripString;
    SC_TRY(StringBuilder(stripString).format("--strip-components={}", stripComponents));
    SC_TRY(process.exec({"tar", "-xvf", fileName, "-C", directory, stripString.view()}));
    return Result(process.getExitStatus() == 0);
}

[[nodiscard]] inline Result packageInstall(const Download& download, Package& package, CustomFunctions functions)
{
    SC_TRY_MSG(not download.packageName.isEmpty(), "Missing packageName");
    SC_TRY_MSG(not download.packageVersion.isEmpty(), "Missing packageVersion");
    SC_TRY_MSG(not download.packagePlatform.isEmpty(), "Missing packagePlatform");
    SC_TRY_MSG(not download.url.isEmpty(), "Missing url");
    FileSystem fs;
    package.packageFullName =
        format("{0}-{1}-{2}", download.packageName, download.packageVersion, download.packagePlatform);
    if (package.packageBaseName.isEmpty())
    {
        package.packageBaseName = Path::basename(download.url.view(), Path::Type::AsPosix);
    }

    package.packageLocalFile =
        format("{0}/{1}/{2}", download.packagesCacheDirectory, download.packageName, package.packageBaseName);
    package.packageLocalTxt = format("{0}.txt", package.packageLocalFile);
    if (download.isGitClone)
    {
        package.packageLocalDirectory = format("{0}_{1}", package.packageLocalFile, download.packageVersion);
    }
    else
    {
        package.packageLocalDirectory = format("{0}_extracted", package.packageLocalFile);
    }

    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(download.packagesCacheDirectory.view()));
    SC_TRY(fs.makeDirectoryRecursive(download.packagesInstallDirectory.view()));
    SC_TRY(fs.makeDirectoryRecursive(package.packageLocalDirectory.view())); // Creates packages/packageName

    package.installDirectoryLink =
        format("{0}/{1}_{2}", download.packagesInstallDirectory, download.packageName, download.packagePlatform);

    // Test if the tool works
    Result testSucceeded = functions.testFunction(download, package);

    // If test failed just try recreating the link
    if (not testSucceeded and download.createLink and fs.existsAndIsFile(package.packageLocalTxt.view()))
    {
        SC_TRY(fs.removeLinkIfExists(package.installDirectoryLink.view()));
        if (createLink(package.packageLocalDirectory.view(), package.installDirectoryLink.view()))
        {
            testSucceeded = functions.testFunction(download, package);
        }
    }

    // If it's still failed let's re-download and extract + link everything
    if (not testSucceeded)
    {
        if (!download.isGitClone)
        {
            SC_TRY(downloadFileMD5(download.url.view(), package.packageLocalFile.view(), download.fileMD5.view()));
        }
        if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
        {
            SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
        }
        SC_TRY(fs.makeDirectoryRecursive(package.packageLocalDirectory.view()));

        if (download.isGitClone)
        {
            Process    process[4];
            StringView params[10];
            size_t     numParams;

            if (download.shallowClone.isEmpty())
            {
                SC_TRY_MSG(process[0].exec({"git", "clone", download.url.view(), package.packageLocalDirectory.view()}),
                           "git is missing");
                SC_TRY_MSG(process[0].getExitStatus() == 0, "git clone failed");
                SC_TRY(process[1].setWorkingDirectory(package.packageLocalDirectory.view()));

                SC_TRY(process[3].setWorkingDirectory(package.packageLocalDirectory.view()));
                SC_TRY(process[3].exec({"git", "checkout", download.packageVersion.view()}));
                SC_TRY_MSG(process[3].getExitStatus() == 0, "git checkout failed");
            }
            else
            {

                // git init
                // git remote add origin <url>
                // git fetch --depth 1 origin <sha1>
                // git checkout FETCH_HEAD
                SC_TRY(process[0].setWorkingDirectory(package.packageLocalDirectory.view()));
                SC_TRY_MSG(process[0].exec({"git", "init"}), "git is missing");
                SC_TRY_MSG(process[0].getExitStatus() == 0, "git init failed");
                numParams           = 0;
                params[numParams++] = "git";
                params[numParams++] = "remote";
                params[numParams++] = "add";
                params[numParams++] = "origin";
                params[numParams++] = download.url.view();
                SC_TRY(process[1].setWorkingDirectory(package.packageLocalDirectory.view()));
                SC_TRY_MSG(process[1].exec({params, numParams}), "git is missing");
                SC_TRY_MSG(process[1].getExitStatus() == 0, "git remote add failed");

                numParams           = 0;
                params[numParams++] = "git";
                params[numParams++] = "fetch";
                params[numParams++] = "--depth=1";
                params[numParams++] = "origin";
                params[numParams++] = download.shallowClone.view(); // Needs the entire Hash
                SC_TRY(process[2].setWorkingDirectory(package.packageLocalDirectory.view()));
                SC_TRY_MSG(process[2].exec({params, numParams}), "git is missing");
                SC_TRY_MSG(process[2].getExitStatus() == 0, "git fetch failed");

                SC_TRY(process[3].setWorkingDirectory(package.packageLocalDirectory.view()));
                SC_TRY(process[3].exec({"git", "checkout", "FETCH_HEAD"}));
                SC_TRY_MSG(process[3].getExitStatus() == 0, "git checkout failed");
            }
        }
        else
        {
            if (functions.extractFunction.isValid())
            {
                SC_TRY(
                    functions.extractFunction(package.packageLocalFile.view(), package.packageLocalDirectory.view()));
            }
            else
            {
                SC_TRY(tarExpandTo(package.packageLocalFile.view(), package.packageLocalDirectory.view(), 0));
            }
            SC_TRY(removeQuarantineAttribute(package.packageLocalDirectory.view()));
        }
        bool createPackageFile = true;
        if (download.createLink)
        {
            SC_TRY(fs.removeLinkIfExists(package.installDirectoryLink.view()));
            if (not createLink(package.packageLocalDirectory.view(), package.installDirectoryLink.view()))
            {
                SC_TRY(fs.copyDirectory(package.packageLocalDirectory.view(), package.installDirectoryLink.view()));
                createPackageFile = false;
            }
        }
        SC_TRY(functions.testFunction(download, package));
        if (createPackageFile)
        {
            String packageTxt =
                format("SC_PACKAGE_URL={0}\nSC_PACKAGE_MD5={1}\n", download.url.view(), download.fileMD5.view());
            SC_TRY(fs.writeString(package.packageLocalTxt.view(), packageTxt.view()));
        }
    }
    return Result(true);
}

inline Result verifyGitCommitHash(const Download& download, const Package& package)
{
    String  result;
    Process process;
    SC_TRY(process.setWorkingDirectory(package.packageLocalDirectory.view()));
    SC_TRY_MSG(process.exec(
                   {
                       "git",
                       "rev-parse",
                       "HEAD",
                   },
                   result),
               "git not installed on current system");
    return Result(result.view().startsWith(download.packageVersion.view()));
}

constexpr StringView PackagesCacheDirectory   = "_PackagesCache";
constexpr StringView PackagesInstallDirectory = "_Packages";

} // namespace Tools
} // namespace SC
