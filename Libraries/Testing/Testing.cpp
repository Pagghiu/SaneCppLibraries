// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Testing.h"
#include "../Foundation/Assert.h"
#include "../Foundation/Span.h"
#include <stdint.h> // uint16_t, uint32_t
#include <stdio.h>  // FILE
#include <stdlib.h> // exit
#include <string.h> // strlen

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#if SC_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#endif
#endif

namespace SC
{
static const StringSpan redEMOJI   = StringSpan({"\xf0\x9f\x9f\xa5", 4}, true, StringEncoding::Utf8);
static const StringSpan greenEMOJI = StringSpan({"\xf0\x9f\x9f\xa9", 4}, true, StringEncoding::Utf8);

struct StartupAttempt
{
    const char* source = nullptr;
    StringPath  path;
};

static constexpr native_char_t NativeSeparator =
#if SC_PLATFORM_WINDOWS
    L'\\';
#else
    '/';
#endif

static StringSpan nativeSpan(const native_char_t* text, size_t length, bool nullTerminated = false)
{
#if SC_PLATFORM_WINDOWS
    return StringSpan({text, length}, nullTerminated, StringEncoding::Native);
#else
    return StringSpan({text, length}, nullTerminated, StringEncoding::Native);
#endif
}

static size_t nativeLength(StringSpan view) { return view.sizeInBytes() / sizeof(native_char_t); }

static bool isSeparator(native_char_t ch)
{
    return ch == static_cast<native_char_t>('/') || ch == static_cast<native_char_t>('\\');
}

#if SC_PLATFORM_WINDOWS
static bool isAsciiAlpha(native_char_t ch)
{
    return (ch >= static_cast<native_char_t>('a') && ch <= static_cast<native_char_t>('z')) ||
           (ch >= static_cast<native_char_t>('A') && ch <= static_cast<native_char_t>('Z'));
}
#endif

static size_t findRootLength(StringSpan path)
{
    const native_char_t* text   = path.getNullTerminatedNative();
    const size_t         length = nativeLength(path);
    if (text == nullptr || length == 0)
        return 0;
#if SC_PLATFORM_WINDOWS
    if (length >= 4 && isSeparator(text[0]) && isSeparator(text[1]) && text[2] == static_cast<native_char_t>('?') &&
        isSeparator(text[3]))
    {
        return 4;
    }
    if (length >= 3 && isAsciiAlpha(text[0]) && text[1] == static_cast<native_char_t>(':') && isSeparator(text[2]))
    {
        return 3;
    }
    if (length >= 2 && isSeparator(text[0]) && isSeparator(text[1]))
    {
        size_t idx = 2;
        while (idx < length && !isSeparator(text[idx]))
            idx++;
        if (idx >= length)
            return 2;
        idx++;
        while (idx < length && !isSeparator(text[idx]))
            idx++;
        return idx;
    }
    return 0;
#else
    return text[0] == '/' ? 1 : 0;
#endif
}

static bool isAbsoluteNative(StringSpan path) { return findRootLength(path) > 0; }

static Result assignNativeSlice(StringPath& output, const native_char_t* text, size_t length)
{
    SC_TRY(output.assign(nativeSpan(text, length)));
    return Result(true);
}

static void trimTrailingSeparators(StringPath& path)
{
    const size_t   rootLength = findRootLength(path.view());
    size_t         length     = nativeLength(path.view());
    native_char_t* buffer     = path.writableSpan().data();
    while (length > rootLength && isSeparator(buffer[length - 1]))
    {
        length--;
    }
    (void)path.resize(length);
}

static Result pathJoin(StringPath& output, StringSpan base, StringSpan leaf)
{
    SC_TRY(output.assign(base));
    trimTrailingSeparators(output);
    const size_t length = nativeLength(output.view());
    if (length > 0)
    {
        native_char_t separatorBuffer[2] = {NativeSeparator, 0};
        const bool    needsSeparator     = !isSeparator(output.writableSpan().data()[length - 1]);
        if (needsSeparator)
        {
            SC_TRY(output.append(nativeSpan(separatorBuffer, 1, true)));
        }
    }
    SC_TRY(output.append(leaf));
    return Result(true);
}

static bool pathExistsFile(StringSpan path)
{
#if SC_PLATFORM_WINDOWS
    const DWORD attr = ::GetFileAttributesW(path.getNullTerminatedNative());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat info;
    return ::stat(path.getNullTerminatedNative(), &info) == 0 && S_ISREG(info.st_mode);
#endif
}

static bool pathExistsDirectory(StringSpan path)
{
#if SC_PLATFORM_WINDOWS
    const DWORD attr = ::GetFileAttributesW(path.getNullTerminatedNative());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return ::stat(path.getNullTerminatedNative(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

static bool parsePortOffset(const char* value, uint16_t& parsedOffset)
{
    if (value == nullptr or *value == '\0')
    {
        return false;
    }

    uint32_t parsed = 0;
    for (const char* ch = value; *ch != '\0'; ++ch)
    {
        if (*ch < '0' or *ch > '9')
        {
            return false;
        }
        parsed = parsed * 10 + static_cast<uint32_t>(*ch - '0');
        if (parsed > 65535)
        {
            return false;
        }
    }
    parsedOffset = static_cast<uint16_t>(parsed);
    return true;
}

static Result getDirectoryName(StringSpan path, StringPath& directory)
{
    const native_char_t* text   = path.getNullTerminatedNative();
    const size_t         length = nativeLength(path);
    size_t               last   = length;
    while (last > 0)
    {
        if (isSeparator(text[last - 1]))
        {
            return assignNativeSlice(directory, text, last - 1);
        }
        last--;
    }
    return Result::Error("Missing directory separator");
}

static bool repoHasMarkers(StringSpan path)
{
    if (!isAbsoluteNative(path) || !pathExistsDirectory(path))
        return false;

    StringPath marker;
    if (!pathJoin(marker, path, "SC.cpp"))
        return false;
    if (!pathExistsFile(marker.view()))
        return false;
    if (!pathJoin(marker, path, "SC.sh"))
        return false;
    return pathExistsFile(marker.view());
}

static void recordAttempt(StartupAttempt* attempts, size_t maxAttempts, size_t& numAttempts, const char* source,
                          StringSpan candidate)
{
    if (numAttempts >= maxAttempts)
        return;
    attempts[numAttempts].source = source;
    if (attempts[numAttempts].path.assign(candidate))
    {
        numAttempts++;
    }
}

static bool validateLibraryRoot(StringSpan path) { return repoHasMarkers(path); }

static bool resolveCompiledLibraryRoot(StringSpan executablePath, StringPath& libraryRoot, StartupAttempt* attempts,
                                       size_t maxAttempts, size_t& numAttempts)
{
    const StringSpan compiledLibraryRoot =
        StringSpan::fromNullTerminated(SC_COMPILER_LIBRARY_PATH, StringEncoding::Utf8);
    if (compiledLibraryRoot.isEmpty())
        return false;

    StringPath compiledLibraryRootPath;
    if (!compiledLibraryRootPath.assign(compiledLibraryRoot))
        return false;

    StringPath candidate;
    if (isAbsoluteNative(compiledLibraryRootPath.view()))
    {
        if (!candidate.assign(compiledLibraryRootPath.view()))
            return false;
    }
    else
    {
        StringPath executableDirectory;
        if (!getDirectoryName(executablePath, executableDirectory))
            return false;
        if (!pathJoin(candidate, executableDirectory.view(), compiledLibraryRootPath.view()))
            return false;
    }

    recordAttempt(attempts, maxAttempts, numAttempts, "compiled relative root", candidate.view());
    if (validateLibraryRoot(candidate.view()))
    {
        return libraryRoot.assign(candidate.view()) ? true : false;
    }
    return false;
}

static void reportLibraryRootFailure(TestReport::IOutput& console, StringSpan applicationRoot, StartupAttempt* attempts,
                                     size_t numAttempts, StringSpan explicitOverride, bool explicitOverrideProvided,
                                     StringSpan compiledLibraryRoot)
{
    console.print("TestReport::Failed resolving library root\n");
    if (explicitOverrideProvided)
    {
        console.print("  explicit override = {}\n", explicitOverride);
    }
    else
    {
        console.print("  compiled root     = {}\n", compiledLibraryRoot);
    }
    console.print("  application root  = {}\n", applicationRoot);
    if (numAttempts == 0)
    {
        console.print("  attempted paths   = <none>\n");
    }
    for (size_t idx = 0; idx < numAttempts; ++idx)
    {
        console.print("  - {}: {}\n", StringSpan::fromNullTerminated(attempts[idx].source, StringEncoding::Ascii),
                      attempts[idx].path.view());
    }
    console.flush();
}

static bool resolveLibraryRoot(TestReport::IOutput& console, StringSpan explicitOverride, bool explicitOverrideProvided,
                               StringSpan executablePath, StringSpan applicationRoot, StringPath& libraryRoot)
{
    StartupAttempt   attempts[16];
    size_t           numAttempts = 0;
    const StringSpan compiledLibraryRoot =
        StringSpan::fromNullTerminated(SC_COMPILER_LIBRARY_PATH, StringEncoding::Utf8);

    if (explicitOverrideProvided)
    {
        StringPath candidate;
        if (candidate.assign(explicitOverride))
        {
            recordAttempt(attempts, 16, numAttempts, "explicit override", candidate.view());
            if (validateLibraryRoot(candidate.view()))
            {
                return libraryRoot.assign(candidate.view()) ? true : false;
            }
        }
        reportLibraryRootFailure(console, applicationRoot, attempts, numAttempts, explicitOverride, true,
                                 compiledLibraryRoot);
        return false;
    }

    if (resolveCompiledLibraryRoot(executablePath, libraryRoot, attempts, 16, numAttempts))
        return true;

    reportLibraryRootFailure(console, applicationRoot, attempts, numAttempts, explicitOverride, false,
                             compiledLibraryRoot);
    return false;
}

static StringSpan getExecutablePath(StringPath& executablePath)
{
#if SC_PLATFORM_WINDOWS
    DWORD length =
        ::GetModuleFileNameW(nullptr, executablePath.writableSpan().data(), static_cast<DWORD>(StringPath::MaxPath));
    if (length == 0 || length >= StringPath::MaxPath)
    {
        (void)executablePath.resize(0);
        return {};
    }
    (void)executablePath.resize(length);
    return executablePath.view();
#elif SC_PLATFORM_APPLE
    uint32_t size = static_cast<uint32_t>(StringPath::MaxPath);
    if (_NSGetExecutablePath(executablePath.writableSpan().data(), &size) == 0)
    {
        const size_t length = ::strlen(executablePath.writableSpan().data());
        (void)executablePath.resize(length);
        return executablePath.view();
    }
    return {};
#else
    const int pathLength = ::readlink("/proc/self/exe", executablePath.writableSpan().data(), StringPath::MaxPath);
    if (pathLength > 0)
    {
        (void)executablePath.resize(static_cast<size_t>(pathLength));
        return executablePath.view();
    }
    return {};
#endif
}

static StringSpan getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
#if SC_PLATFORM_WINDOWS
    StringSpan exeView = getExecutablePath(applicationRootDirectory);
    if (exeView.isEmpty())
        return {};
    native_char_t* buffer        = applicationRootDirectory.writableSpan().data();
    size_t         lastSeparator = 0;
    bool           found         = false;
    for (size_t idx = 0; idx < nativeLength(exeView); ++idx)
    {
        if (isSeparator(buffer[idx]))
        {
            lastSeparator = idx;
            found         = true;
        }
    }
    if (!found)
    {
        (void)applicationRootDirectory.resize(0);
        return {};
    }
    (void)applicationRootDirectory.resize(lastSeparator);
    return applicationRootDirectory.view();
#elif SC_PLATFORM_APPLE
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle != nullptr)
    {
        CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle);
        if (bundleURL != nullptr)
        {
            uint8_t* path = reinterpret_cast<uint8_t*>(applicationRootDirectory.writableSpan().data());
            if (CFURLGetFileSystemRepresentation(bundleURL, true, path, StringPath::MaxPath))
            {
                (void)applicationRootDirectory.resize(::strlen(reinterpret_cast<char*>(path)));
                CFRelease(bundleURL);
                return applicationRootDirectory.view();
            }
            CFRelease(bundleURL);
        }
    }
    StringSpan executablePath = getExecutablePath(applicationRootDirectory);
    if (!executablePath.isEmpty())
    {
        native_char_t* buffer = applicationRootDirectory.writableSpan().data();
        for (size_t idx = nativeLength(executablePath); idx > 0; --idx)
        {
            if (buffer[idx - 1] == '/')
            {
                (void)applicationRootDirectory.resize(idx - 1);
                return applicationRootDirectory.view();
            }
        }
    }
    return {};
#else
    StringSpan executablePath = getExecutablePath(applicationRootDirectory);
    if (!executablePath.isEmpty())
    {
        native_char_t* buffer = applicationRootDirectory.writableSpan().data();
        for (size_t idx = nativeLength(executablePath); idx > 0; --idx)
        {
            if (buffer[idx - 1] == '/')
            {
                (void)applicationRootDirectory.resize(idx - 1);
                return applicationRootDirectory.view();
            }
        }
    }
    return {};
#endif
}

} // namespace SC
SC::TestReport::TestReport(IOutput& console, int argc, const char** argv) : console(console)
{
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4996) // getenv
#endif
    const char* envPortOffset = ::getenv("SC_TEST_PORT_OFFSET");
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif
    (void)parsePortOffset(envPortOffset, portOffset);

    StringSpan explicitLibraryRoot;
    bool       explicitLibraryRootProvided = false;

    for (int idx = 1; idx < argc; ++idx)
    {
        const auto param = StringSpan::fromNullTerminated(argv[idx], StringEncoding::Ascii);
        if (param == "--quiet")
        {
            quietMode = true;
        }
        if (param == "--test" && testToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                testToRun = StringSpan::fromNullTerminated(argv[idx + 1], StringEncoding::Ascii);
                if (not quietMode)
                {
                    console.print("TestReport::Running single test \"{}\"\n", testToRun);
                }
            }
        }
        if (param == "--test-section" && sectionToRun.isEmpty())
        {
            if (idx + 1 < argc)
            {
                sectionToRun = StringSpan::fromNullTerminated(argv[idx + 1], StringEncoding::Ascii);

                if (not quietMode)
                {
                    console.print("TestReport::Running single section \"{}\"\n", sectionToRun);
                }
            }
        }
        if (param == "--port-offset")
        {
            if (idx + 1 < argc)
            {
                uint16_t parsedOffset = portOffset;
                if (parsePortOffset(argv[idx + 1], parsedOffset))
                {
                    portOffset = parsedOffset;
                }
            }
        }
        if (param == "--all-tests")
        {
            runAllTests = true;
        }
        if (param == "--library-root")
        {
            explicitLibraryRootProvided = true;
            if (idx + 1 < argc)
            {
                explicitLibraryRoot = StringSpan::fromNullTerminated(argv[idx + 1], StringEncoding::Utf8);
            }
        }
    }

    (void)getExecutablePath(executableFile);
    (void)getApplicationRootDirectory(applicationRootDirectory);
    if (explicitLibraryRootProvided &&
        (explicitLibraryRoot.isEmpty() || !resolveLibraryRoot(console, explicitLibraryRoot, true, executableFile.view(),
                                                              applicationRootDirectory.view(), libraryRootDirectory)))
    {
        startupFailure = true;
        numTestsFailed = 1;
    }
    else if (!explicitLibraryRootProvided &&
             !resolveLibraryRoot(console, explicitLibraryRoot, false, executableFile.view(),
                                 applicationRootDirectory.view(), libraryRootDirectory))
    {
        startupFailure = true;
        numTestsFailed = 1;
    }

    if (not quietMode and portOffset > 0)
    {
        console.print("TestReport::Using port offset {}\n", static_cast<size_t>(portOffset));
    }
    if (not quietMode and runAllTests)
    {
        console.print("TestReport::Running optional tests enabled by --all-tests\n");
    }
    if (not quietMode and (not testToRun.isEmpty() or not sectionToRun.isEmpty()))
    {
        console.print("\n");
    }
    console.flush();
}

SC::TestReport::~TestReport()
{
    if (quietMode)
        return;
    if (numTestsFailed > 0)
    {
        console.print(redEMOJI);
        console.print(" TOTAL Failed = {} (Succeeded = {})", numTestsFailed, numTestsSucceeded);
    }
    else
    {
        console.print(greenEMOJI);
        console.print(" TOTAL Succeeded = {}", numTestsSucceeded, numTestsSucceeded);
    }
    console.print("\n---------------------------------------------------\n");
    console.flush();
}

void SC::TestReport::internalRunGlobalMemoryReport(MemoryStatistics stats, bool reportFailure)
{
    if (quietMode)
        return;
    console.print("[[ Memory Report ]]\n");
    console.print("\t - Allocations   = {}\n", stats.numAllocate);
    console.print("\t - Deallocations = {}\n", stats.numRelease);
    console.print("\t - Reallocations = {}\n", stats.numReallocate);
    if (reportFailure)
    {
        if (stats.numAllocate == stats.numRelease)
        {
            console.print(greenEMOJI);
            console.print(" [[ Memory Report ]] SUCCEEDED = 1\n");
        }
        else
        {
            console.print(redEMOJI);
            console.print(" [[ Memory Report ]] FAILED = 1\n");
            numTestsFailed++;
        }
    }
    console.print("---------------------------------------------------\n");
    console.flush();
}

//-------------------------------------------------------------------------------------------
SC::TestCase::TestCase(TestReport& report, StringSpan testName)
    : report(report), testName(testName), numTestsSucceeded(0), numTestsFailed(0), numSectionTestsFailed(0),
      printedSection(false)
{
    if (report.isTestEnabled(testName))
    {
        if (not report.quietMode)
        {
            report.console.print("[[ {} ]]\n\n", testName);
            report.console.flush();
        }
        report.firstFailedTest = StringSpan();
        report.currentSection  = StringSpan();
    }
}

SC::TestCase::~TestCase()
{
    if (report.isTestEnabled(testName))
    {
        if (not printedSection and not report.currentSection.isEmpty())
        {
            report.printSectionResult(*this);
        }
        if (not report.quietMode)
        {
            report.console.print("\n");
            if (numTestsFailed > 0)
            {
                report.console.print(redEMOJI);
                report.console.print(" [[ ");
                report.console.print(testName);
                report.console.print(" ]]");
                report.console.print(" FAILED = {} (Succeeded = {})\n", numTestsFailed, numTestsSucceeded);
            }
            else
            {
                report.console.print(greenEMOJI);
                report.console.print(" [[ ");
                report.console.print(testName);
                report.console.print(" ]]");
                report.console.print(" SUCCEEDED = {}\n", numTestsSucceeded);
            }
            report.console.print("---------------------------------------------------\n");
        }
        report.console.flush();
        report.numTestsFailed += numTestsFailed;
        report.numTestsSucceeded += numTestsSucceeded;
        report.testCaseFinished(*this);
    }
}

bool SC::TestCase::recordExpectation(StringSpan expression, bool status, StringSpan detailedError)
{
    SC_ASSERT_DEBUG(expression.isNullTerminated());
    if (status)
    {
        numTestsSucceeded++;
    }
    else
    {
        numSectionTestsFailed++;
        numTestsFailed++;
        report.printSectionResult(*this);
        printedSection = true;
        report.console.print("\t\t");
        report.console.print(redEMOJI);
        if (detailedError.isEmpty())
        {
            report.console.print(" [FAIL] {}\n", expression);
        }
        else
        {
            report.console.print(" [FAIL] {} - Error: {}\n", expression, detailedError);
        }
        report.console.flush();
        if (report.firstFailedTest.isEmpty())
        {
            report.firstFailedTest = expression;
        }
    }
    return status;
}

bool SC::TestCase::recordExpectation(StringSpan expression, Result status)
{
    return recordExpectation(
        expression, status,
        StringSpan({status.message, status.message ? ::strlen(status.message) : 0}, true, StringEncoding::Ascii));
}

bool SC::TestCase::test_section(StringSpan sectionName, Execute execution)
{
    numSectionTestsFailed = 0;
    bool isTestEnabled;
    switch (execution)
    {
    case Execute::Default: //
        isTestEnabled = report.isTestEnabled(testName) and report.isSectionEnabled(sectionName);
        break;
    case Execute::OnlyExplicit: //
        isTestEnabled = report.sectionToRun == sectionName;
        break;
    default: return false;
    }
    if (isTestEnabled)
    {
        SC_ASSERT_DEBUG(sectionName.isNullTerminated());
        if (not report.currentSection.isEmpty())
        {
            report.printSectionResult(*this);
        }
        report.currentSection = sectionName;
        return true;
    }
    else
    {
        report.currentSection = StringSpan();
        return false;
    }
}

void SC::TestReport::printSectionResult(TestCase& testCase)
{
    if (quietMode)
    {
        return;
    }
    console.print("\t- ");
    console.print(testCase.numSectionTestsFailed > 0 ? redEMOJI : greenEMOJI);
    console.print(" {}::{}\n", testCase.testName, currentSection);
    console.flush();
}

void SC::TestReport::testCaseFinished(TestCase& testCase)
{
    if (abortOnFirstFailedTest && testCase.numTestsFailed > 0)
    {
#if SC_CONFIGURATION_RELEASE
        ::exit(-1);
#endif
    }
}

bool SC::TestReport::isTestEnabled(StringSpan testName) const { return testToRun.isEmpty() || testToRun == testName; }

bool SC::TestReport::isSectionEnabled(StringSpan sectionName) const
{
    return sectionToRun.isEmpty() || sectionName == sectionToRun;
}

int SC::TestReport::getTestReturnCode() const { return startupFailure || numTestsFailed > 0 ? -1 : 0; }

uint16_t SC::TestReport::mapPort(uint16_t basePort) const
{
    const uint32_t mappedPort = static_cast<uint32_t>(basePort) + static_cast<uint32_t>(portOffset);
    SC_ASSERT_RELEASE(mappedPort <= 65535);
    return static_cast<uint16_t>(mappedPort <= 65535 ? mappedPort : basePort);
}

bool SC::TestReport::isTestExplicitlySelected(StringSpan testName) const
{
    return not testToRun.isEmpty() and testToRun == testName;
}
