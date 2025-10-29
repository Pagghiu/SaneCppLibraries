// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
// Order is important
#include <direct.h>
#include <shellapi.h>
#include <sys/stat.h>
#define stat       _stat
#define S_ISDIR(m) ((m) & _S_IFDIR)
#define POPEN      _popen
#define PCLOSE     _pclose
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define POPEN  popen
#define PCLOSE pclose
#endif

#include <cstdint>
#include <ctime>

namespace
{
bool printMessages = false;

// Platform agnostic types
using TimePoint = time_t;

#ifdef _WIN32
std::wstring utf8_to_wstring(const std::string& s)
{
    int          len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}
#endif
} // namespace

namespace Path
{
#ifdef _WIN32
constexpr char Separator = '\\';
#else
constexpr char Separator = '/';
#endif

std::string join(const std::string& a, const std::string& b)
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    return a + Separator + b;
}
} // namespace Path

namespace FileSystem
{
#ifdef _WIN32
FILE* open(const std::string& path, const char* mode)
{
    std::wstring wpath = utf8_to_wstring(path);
    std::wstring wmode = utf8_to_wstring(std::string(mode));
    return _wfopen(wpath.c_str(), wmode.c_str());
}
#else
FILE* open(const std::string& path, const char* mode) { return fopen(path.c_str(), mode); }
#endif

TimePoint getModificationTime(const std::string& path)
{
    struct stat info;
#ifdef _WIN32
    std::wstring wpath = utf8_to_wstring(path);
    if (_wstat(wpath.c_str(), &info) == 0)
#else
    if (stat(path.c_str(), &info) == 0)
#endif
    {
        return info.st_mtime;
    }
    return 0;
}

bool exists(const std::string& path)
{
    struct stat info;
#ifdef _WIN32
    std::wstring wpath = utf8_to_wstring(path);
    return _wstat(wpath.c_str(), &info) == 0;
#else
    return stat(path.c_str(), &info) == 0;
#endif
}

bool isDirectory(const std::string& path)
{
    struct stat info;
#ifdef _WIN32
    std::wstring wpath = utf8_to_wstring(path);
    if (_wstat(wpath.c_str(), &info) != 0)
        return false;
#else
    if (stat(path.c_str(), &info) != 0)
        return false;
#endif
    return S_ISDIR(info.st_mode);
}

int createDirectory(const std::string& path)
{
#ifdef _WIN32
    std::wstring wpath = utf8_to_wstring(path);
    return _wmkdir(wpath.c_str());
#else
    return mkdir(path.c_str(), 0755);
#endif
}

int createDirectoryRecursive(const std::string& path)
{
    std::string subpath;
    subpath.reserve(path.length());
    for (size_t i = 0; i < path.length(); ++i)
    {
        subpath += path[i];
        if (path[i] == Path::Separator)
        {
            if (!subpath.empty() && !isDirectory(subpath))
            {
                if (createDirectory(subpath) != 0)
                {
                    return -1;
                }
            }
        }
    }
    if (!isDirectory(path))
    {
        return createDirectory(path);
    }
    return 0;
}
} // namespace FileSystem

int runCommand(const std::string& command)
{
    if (printMessages)
    {
        printf("Running: %s\n", command.c_str());
    }
#ifdef _WIN32
    STARTUPINFOW        si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    std::wstring        wideCmd = utf8_to_wstring(command);
    if (CreateProcessW(NULL, &wideCmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0 ? 0 : 1;
    }
    else
    {
        return 1;
    }
#else
    return system(command.c_str());
#endif
}

struct CommandLine
{
    std::string command;

    CommandLine(const std::string& program)
    {
        // Quote the program to handle paths with spaces
        command = '"';
        command += program;
        command += '"';
    }

    void arg(const std::string& argument)
    {
        command += ' ';
        command += argument;
    }

    void argQuoted(const std::string& argument)
    {
        command += ' ';
        command += '"';
        command += argument;
        command += '"';
    }

    int run() { return runCommand(command); }
};

// Parse positional bootloader arguments
struct BootloaderArgs
{
    std::string libraryDir;
    std::string toolSourceDir;
    std::string buildDir;
    std::string toolName;

    std::vector<std::string> remainingArgs;
};

BootloaderArgs parseArgs(const std::vector<std::string>& argv)
{
    BootloaderArgs args;
    // Assume the first 4 are from SC.sh/bat, then tool name, then args
    if (argv.size() >= 4)
    {
        args.libraryDir    = argv[1];
        args.toolSourceDir = argv[2];
        args.buildDir      = argv[3];
        args.toolName      = (argv.size() >= 5) ? argv[4] : "build";
        for (size_t i = 5; i < argv.size(); ++i)
        {
            args.remainingArgs.push_back(argv[i]);
        }
    }
    else
    {
        args.toolName = "build";
    }
    return args;
}

struct CompilationInfo
{
    const BootloaderArgs* args;

    std::string targetOS;
    std::string toolOutputDir;
    std::string intermediateDir;
    std::string toolCpp;
    std::string scCpp;
    std::string toolsCpp;
    std::string toolH;
    std::string toolExe;
    std::string toolsDepFile;
    std::string toolDepFile;
};

CompilationInfo setupCompilation(const BootloaderArgs& args)
{
    CompilationInfo ci;
    ci.args = &args;
    // Resolve tool cpp path
    std::string toolCpp       = Path::join(args.toolSourceDir, "SC-" + args.toolName + ".cpp");
    std::string toolSourceDir = args.toolSourceDir;
    std::string toolName      = args.toolName;
    std::string toolH         = "";

    if (!FileSystem::exists(toolCpp))
    {
        // Try if toolName is a path to cpp file
        std::string potentialCpp = args.toolName;
        if (FileSystem::exists(potentialCpp))
        {
            // Assume it's a cpp file
            if (potentialCpp.find(".cpp") == std::string::npos)
            {
                fprintf(stderr, "Error: Tool file must end with .cpp\n");
                exit(1);
            }
            // Get dir and name
            size_t lastSlash = potentialCpp.find_last_of(Path::Separator);
            if (lastSlash != std::string::npos)
            {
                toolSourceDir        = potentialCpp.substr(0, lastSlash);
                std::string filename = potentialCpp.substr(lastSlash + 1);
                size_t      dot      = filename.find('.');
                if (dot != std::string::npos)
                {
                    toolName = filename.substr(0, dot);
                }
            }
            toolCpp = potentialCpp;
        }
        else
        {
            fprintf(stderr, "Error: Tool \"%s\" doesn't exist\n", args.toolName.c_str());
            exit(1);
        }
    }
    else
    {
        toolH = Path::join(toolSourceDir, "SC-" + toolName + ".h");
    }

    std::string exeName = "SC-" + toolName;
    std::string exeExt  = "";
#ifdef _WIN32
    ci.targetOS = "Windows";
    exeExt      = ".exe";
#else
    // Use uname for precise OS
    FILE* unameFile = POPEN("uname", "r");
    ci.targetOS     = "POSIX"; // default
    if (unameFile)
    {
        char buf[256];
        if (fgets(buf, sizeof(buf), unameFile))
        {
            std::string os = buf;
            if (!os.empty() && os.back() == '\n')
                os.pop_back();
            ci.targetOS = os;
        }
        PCLOSE(unameFile);
    }
#endif

    // Use absolute paths for all file paths to avoid current directory issues
    ci.toolOutputDir   = Path::join(args.buildDir, "_Tools");
    ci.intermediateDir = Path::join(Path::join(ci.toolOutputDir, "_Intermediates"), ci.targetOS);

    // For dependency files, absolute paths
    ci.toolsDepFile =
        Path::join(ci.intermediateDir, std::string("Tools") + (ci.targetOS == "Windows" ? ".json" : ".d"));
    ci.toolDepFile = Path::join(ci.intermediateDir,
                                std::string("SC-") + ci.args->toolName + (ci.targetOS == "Windows" ? ".json" : ".d"));

    ci.toolCpp  = toolCpp;
    ci.scCpp    = Path::join(args.libraryDir, "SC.cpp");
    ci.toolsCpp = Path::join(toolSourceDir, "Tools.cpp");
    ci.toolH    = toolH;
    ci.toolExe  = Path::join(Path::join(ci.toolOutputDir, ci.targetOS), exeName + exeExt);

    return ci;
}

std::vector<std::string> parseJsonDependencies(FILE* file)
{
    std::vector<std::string> deps;

    char line[2048];
    bool inIncludes = false;
    bool inArray    = false;
    while (fgets(line, sizeof(line), file))
    {
        std::string str = line;
        if (str.find("\"Includes\"") != std::string::npos)
        {
            inIncludes = true;
        }
        if (inIncludes)
        {
            if (str.find('[') != std::string::npos)
            {
                inArray = true;
            }
            if (str.find(']') != std::string::npos)
            {
                inArray = false;
                break; // End of array
            }
        }
        if (inIncludes && inArray && str.find('"') != std::string::npos)
        {
            size_t start = 0, end;
            while ((start = str.find('"', start + 1)) != std::string::npos)
            {
                end = str.find('"', start + 1);
                if (end != std::string::npos)
                {
                    std::string dep = str.substr(start + 1, end - start - 1);
                    if (!dep.empty() && dep.find("windows kits") == std::string::npos &&
                        dep.find("microsoft visual studio") == std::string::npos && dep != "Includes" &&
                        dep != "ImportedModules")
                    {
                        deps.push_back(dep);
                    }
                    start = end;
                }
                else
                    break;
            }
        }
    }
    return deps;
}

std::string unescapeMakeDependency(const std::string& input)
{
    std::string out;
    bool        escaping = false;
    for (char c : input)
    {
        if (escaping)
        {
            out += c;
            escaping = false;
        }
        else if (c == '\\')
        {
            escaping = true;
        }
        else
        {
            out += c;
        }
    }
    return out;
}

std::vector<std::string> parseMakeDependencies(FILE* file, const std::string& baseDir)
{
    std::vector<std::string> deps;

    char        line[1024];
    bool        afterColon = false;
    std::string continuation;
    while (fgets(line, sizeof(line), file))
    {
        std::string str = line;
        if (!str.empty() && str.back() == '\n')
            str.pop_back();
        if (!str.empty() && str.back() == '\r')
            str.pop_back();

        if (!continuation.empty())
        {
            str = continuation + str;
            continuation.clear();
        }
        if (!str.empty() && str.back() == '\\')
        {
            continuation = str.substr(0, str.size() - 1);
            continue;
        }

        if (!afterColon)
        {
            size_t colon = str.find(':');
            if (colon != std::string::npos)
            {
                str        = str.substr(colon + 1);
                afterColon = true;
            }
        }

        if (afterColon)
        {
            size_t start = str.find_first_not_of(" \t");
            while (start != std::string::npos)
            {
                size_t end = start;
                while (end < str.length() && str[end] != ' ')
                {
                    if (str[end] == '\\' && end + 1 < str.length() && str[end + 1] == ' ')
                    {
                        end += 2; // skip \ and space
                    }
                    else
                    {
                        end++;
                    }
                }
                size_t      actual_end = (end < str.length()) ? end : end;
                std::string dep        = str.substr(start, actual_end - start);
                if (!dep.empty())
                {
                    if (dep.size() >= 2 && dep[0] == '"' && dep.back() == '"')
                    {
                        dep = dep.substr(1, dep.size() - 2);
                    }
                    // Unescape backslash sequences in make dependencies (e.g., \ for spaces)
                    dep = unescapeMakeDependency(dep);
                    deps.push_back(dep);
                }
                start = (end < str.length()) ? str.find_first_not_of(" \t", end) : std::string::npos;
            }
        }
    }
    return deps;
}

std::vector<std::string> parseDependencies(const std::string& depFilePath, const std::string& baseDir)
{
    FILE* file = FileSystem::open(depFilePath, "r");
    if (!file)
        return {};

    std::vector<std::string> deps;

    bool isJson = depFilePath.size() > 5 && depFilePath.substr(depFilePath.size() - 5) == ".json";
    if (isJson)
    {
        deps = parseJsonDependencies(file);
    }
    else
    {
        deps = parseMakeDependencies(file, baseDir);
    }
    for (auto& dep : deps)
    {
        bool is_absolute = dep[0] == '/' || dep[0] == '\\' || (dep.size() >= 3 && dep[1] == ':' && dep[2] == '\\');
        if (!is_absolute)
        {
            dep = Path::join(baseDir, dep);
        }
    }
    fclose(file);
    return deps;
}

bool checkNeedsRebuild(TimePoint objTime, const std::vector<std::string>& sources,
                       const std::vector<std::string>& dependencies)
{
    for (const auto& src : sources)
    {
        if (!FileSystem::exists(src))
        {
            fprintf(stderr, "Error: Source file %s does not exist\n", src.c_str());
            return true;
        }
        TimePoint srcTime = FileSystem::getModificationTime(src);
        if (srcTime > objTime)
        {
            if (printMessages)
            {
                printf("  Source %s modified (time %lld > %lld), needs rebuild\n", src.c_str(),
                       static_cast<long long>(srcTime), static_cast<long long>(objTime));
            }
            return true;
        }
    }
    for (const auto& dep : dependencies)
    {
        if (!FileSystem::exists(dep))
        {
            fprintf(stderr, "Error: Dependency file %s does not exist\n", dep.c_str());
        }
        else
        {
            TimePoint depTime = FileSystem::getModificationTime(dep);
            if (depTime > objTime)
            {
                if (printMessages)
                {
                    printf("  Dependency %s modified (time %lld > %lld), needs rebuild\n", dep.c_str(),
                           static_cast<long long>(depTime), static_cast<long long>(objTime));
                }
                return true;
            }
        }
    }
    return false;
}

// Check if Tools.o needs rebuilding
bool needsRebuildTools(const CompilationInfo& ci)
{
    std::string objExt   = ci.targetOS == "Windows" ? ".obj" : ".o";
    std::string toolsObj = Path::join(ci.intermediateDir, "Tools" + objExt);
    TimePoint   objTime  = FileSystem::getModificationTime(toolsObj);
    if (objTime == 0)
    {
        if (printMessages)
            printf("  Tools obj file not found, needs rebuild\n");
        return true; // Obj doesn't exist
    }

    std::vector<std::string> toolsDeps = parseDependencies(ci.toolsDepFile, ci.intermediateDir);
    if (printMessages)
    {
        printf("- Found %zu dependencies for \"%s\"\n", toolsDeps.size(), ci.toolsDepFile.c_str());
    }
    if (checkNeedsRebuild(objTime, {ci.scCpp, ci.toolsCpp}, toolsDeps))
    {
        return true;
    }

    if (printMessages)
    {
        printf("  Tools obj up to date\n");
    }
    return false;
}

// Check if tool obj needs rebuilding
bool needsRebuildToolObj(const CompilationInfo& ci)
{
    std::string objExt      = ci.targetOS == "Windows" ? ".obj" : ".o";
    std::string toolObjName = "SC-" + ci.args->toolName + objExt;
    std::string toolObj     = Path::join(ci.intermediateDir, toolObjName);
    TimePoint   objTime     = FileSystem::getModificationTime(toolObj);
    if (objTime == 0)
    {
        if (printMessages)
        {
            printf("  Obj file %s not found, needs rebuild\n", toolObj.c_str());
        }
        return true; // Obj doesn't exist
    }

    std::vector<std::string> sources = {ci.toolCpp};
    if (!ci.toolH.empty())
    {
        sources.push_back(ci.toolH);
    }
    std::vector<std::string> toolDeps = parseDependencies(ci.toolDepFile, ci.intermediateDir);
    if (printMessages)
    {
        printf("- Found %zu dependencies for %s \n", toolDeps.size(), ci.toolCpp.c_str());
    }
    if (checkNeedsRebuild(objTime, sources, toolDeps))
    {
        return true;
    }

    if (printMessages)
    {
        printf("- \"%s\" object file up to date\n", ci.toolCpp.c_str());
    }
    return false;
}

// Simple check if need to rebuild (link)
bool needsRebuildExe(const CompilationInfo& ci)
{
    TimePoint exeTime = FileSystem::getModificationTime(ci.toolExe);
    if (exeTime == 0)
        return true; // Exe doesn't exist

    std::string objExt      = ci.targetOS == "Windows" ? ".obj" : ".o";
    std::string toolsObj    = Path::join(ci.intermediateDir, "Tools" + objExt);
    std::string toolObjName = "SC-" + ci.args->toolName + objExt;
    std::string toolObj     = Path::join(ci.intermediateDir, toolObjName);

    TimePoint toolsObjTime = FileSystem::getModificationTime(toolsObj);
    TimePoint toolObjTime  = FileSystem::getModificationTime(toolObj);

    if (toolsObjTime > exeTime || toolObjTime > exeTime)
        return true;

    return false;
}

int compilePOSIX(const CompilationInfo& ci)
{
    // Create directories recursively
    FileSystem::createDirectoryRecursive(ci.intermediateDir);
    FileSystem::createDirectoryRecursive(Path::join(ci.toolOutputDir, ci.targetOS));

    // Flags
    std::string commonFlags = " -I\"../../..\" -std=c++14 -pthread -fstrict-aliasing -fvisibility=hidden "
                              "-fvisibility-inlines-hidden -fno-rtti -fno-exceptions";
    std::string linkFlags   = " -ldl -lpthread";

    // Determine OS and compiler (match Makefile logic)
    std::string targetOS;
    FILE*       unameFile = POPEN("uname", "r");
    if (unameFile)
    {
        char buf[256];
        if (fgets(buf, sizeof(buf), unameFile))
        {
            targetOS = buf;
            if (!targetOS.empty() && targetOS.back() == '\n')
                targetOS.pop_back();
        }
        PCLOSE(unameFile);
    }

    bool useClang = (runCommand("clang++ --version > /dev/null 2>&1") == 0);

    std::string compiler = useClang ? "clang++" : "g++";
    if (useClang)
    {
        commonFlags += " -nostdinc++";
    }

    if (targetOS == "Linux")
    {
        linkFlags += " -rdynamic";
    }
    else if (targetOS == "Darwin")
    {
        linkFlags += " -framework CoreFoundation -framework CoreServices";
    }

    if (useClang)
    {
        linkFlags += " -nostdlib++";
    }

    // Compile Tools.o if needed
    if (needsRebuildTools(ci))
    {
        std::string toolsObj = Path::join(ci.intermediateDir, "Tools.o");
        CommandLine cmd(compiler);
        cmd.arg(commonFlags);
        cmd.arg("-MMD");
        cmd.arg("-o");
        cmd.argQuoted(toolsObj);
        cmd.arg("-c");
        cmd.argQuoted(ci.toolsCpp);
        printf("Tools.cpp\n");
        if (cmd.run() != 0)
            return 1;
    }

    // Compile tool .o if needed
    if (needsRebuildToolObj(ci))
    {
        std::string toolObjName = "SC-" + ci.args->toolName + ".o";
        std::string toolObj     = Path::join(ci.intermediateDir, toolObjName);
        CommandLine cmd(compiler);
        cmd.arg(commonFlags);
        cmd.arg("-MMD");
        cmd.arg("-o");
        cmd.argQuoted(toolObj);
        cmd.arg("-c");
        cmd.argQuoted(ci.toolCpp);
        printf("SC-%s.cpp\n", ci.args->toolName.c_str());
        if (cmd.run() != 0)
            return 1;
    }

    // Link if needed
    if (needsRebuildExe(ci))
    {
        std::string toolsObj    = Path::join(ci.intermediateDir, "Tools.o");
        std::string toolObjName = "SC-" + ci.args->toolName + ".o";
        std::string toolObj     = Path::join(ci.intermediateDir, toolObjName);
        CommandLine cmd(compiler);
        cmd.arg("-o");
        cmd.argQuoted(ci.toolExe);
        cmd.argQuoted(toolsObj);
        cmd.argQuoted(toolObj);
        cmd.arg(linkFlags);
        if (cmd.run() != 0)
            return 1;
    }

    return 0;
}

int linkWindows(const CompilationInfo& ci)
{
    printf("Linking %s\n", ci.args->toolName.c_str());
    std::string toolsObj    = Path::join(ci.intermediateDir, "Tools.obj");
    std::string toolObjName = "SC-" + ci.args->toolName + ".obj";
    std::string toolObj     = Path::join(ci.intermediateDir, toolObjName);
    std::string linkFlags   = " Advapi32.lib Shell32.lib";
    CommandLine cmd("link");
    cmd.arg("/nologo");
    std::string outArg = "/OUT:\"" + ci.toolExe + "\"";
    cmd.arg(outArg);
    cmd.argQuoted(toolsObj);
    cmd.argQuoted(toolObj);
    cmd.arg(linkFlags);
    return cmd.run();
}

bool compileWindows(const CompilationInfo& ci, bool& objsCompiled)
{
    // Create directories recursively
    FileSystem::createDirectoryRecursive(ci.intermediateDir);
    FileSystem::createDirectoryRecursive(Path::join(ci.toolOutputDir, ci.targetOS));

    // Flags
    std::string commonFlags = " /nologo /I\"../../..\" /std:c++14 /MTd /permissive- /EHsc";

    // Compile Tools.obj if needed
    if (needsRebuildTools(ci))
    {
        objsCompiled          = true;
        std::string toolsObj  = Path::join(ci.intermediateDir, "Tools.obj");
        std::string toolsJson = Path::join(ci.intermediateDir, "Tools.json");
        CommandLine cmd("cl.exe");
        cmd.arg(commonFlags);
        cmd.arg("/sourceDependencies");
        cmd.argQuoted(toolsJson);
        cmd.arg("/c");
        cmd.arg("/Fd\"" + Path::join(ci.intermediateDir, "Tools.pdb") + "\"");
        cmd.arg("/Fo\"" + toolsObj + "\"");
        cmd.argQuoted(ci.toolsCpp);
        if (cmd.run() != 0)
            return false;
    }

    // Compile tool .obj if needed
    if (needsRebuildToolObj(ci))
    {
        objsCompiled            = true;
        std::string toolObjName = "SC-" + ci.args->toolName + ".obj";
        std::string toolObj     = Path::join(ci.intermediateDir, toolObjName);
        std::string toolJson    = Path::join(ci.intermediateDir, "SC-" + ci.args->toolName + ".json");
        CommandLine cmd("cl.exe");
        cmd.arg(commonFlags);
        cmd.arg("/sourceDependencies");
        cmd.argQuoted(toolJson);
        cmd.arg("/c");
        std::string pdbPath = toolObjName.substr(0, toolObjName.size() - 4) + ".pdb";
        cmd.arg("/Fd\"" + Path::join(ci.intermediateDir, pdbPath) + "\"");
        cmd.arg("/Fo\"" + toolObj + "\"");
        cmd.argQuoted(ci.toolCpp);
        if (cmd.run() != 0)
            return false;
    }
    return true;
}

int executeTool(const BootloaderArgs& args, const CompilationInfo& ci)
{
    CommandLine cmd(ci.toolExe);
    cmd.argQuoted(args.libraryDir);
    cmd.argQuoted(args.toolSourceDir);
    cmd.argQuoted(args.buildDir);
    cmd.argQuoted(args.toolName);
    for (const auto& arg : args.remainingArgs)
    {
        cmd.argQuoted(arg);
    }
    return cmd.run();
}

std::vector<std::string> getArgv(int& argc, char* argv[])
{
#ifdef _WIN32
    // Set console output to UTF-8 to properly display Unicode
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // On Windows, convert command line arguments to UTF-8
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv)
    {
        fprintf(stderr, "CommandLineToArgvW failed\n");
        exit(1);
    }
    std::vector<std::string> new_argv;
    for (int i = 0; i < argc; ++i)
    {
        int         len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        std::string str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &str[0], len, NULL, NULL);
        if (!str.empty() && str.back() == '\0')
            str.pop_back();
        new_argv.push_back(str);
    }
    LocalFree(wargv);
    return new_argv;
#else
    std::vector<std::string> new_argv;
    for (int i = 0; i < argc; ++i)
    {
        new_argv.push_back(argv[i]);
    }
    return new_argv;
#endif
}

int main(int argc, char* argv[])
{
    std::vector<std::string> string_argv = getArgv(argc, argv);
    BootloaderArgs           args        = parseArgs(string_argv);
    CompilationInfo          ci          = setupCompilation(args);

    bool needsCompile = needsRebuildTools(ci) || needsRebuildToolObj(ci);
    bool needsLink    = needsRebuildExe(ci);

    if (needsCompile || needsLink)
    {
        if (printMessages)
        {
            printf("Rebuilding %s tool...\n", args.toolName.c_str());
        }
        auto start_time = std::chrono::high_resolution_clock::now();
#ifdef _WIN32
        bool objsCompiled = false;
        if (!compileWindows(ci, objsCompiled))
        {
            fprintf(stderr, "Compilation failed\n");
            return 1;
        }
        if (objsCompiled || needsLink)
        {
            if (linkWindows(ci) != 0)
            {
                fprintf(stderr, "Linking failed\n");
                return 1;
            }
        }
#else
        if (compilePOSIX(ci) != 0)
        {
            fprintf(stderr, "Compilation failed\n");
            return 1;
        }
#endif
        auto                          end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        printf("Time to compile \"%s\" tool: %.2f seconds\n", args.toolName.c_str(), duration.count());
    }
    else
    {
        if (printMessages)
        {
            printf("\"%s\" is up to date\n", ci.toolCpp.c_str());
        }
    }

    return executeTool(args, ci);
}
