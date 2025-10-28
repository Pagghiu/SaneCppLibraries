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
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>
#define stat       _stat
#define S_ISDIR(m) ((m) & _S_IFDIR)
#define POPEN      _popen
#define PCLOSE     _pclose
#include <shellapi.h>
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

std::string utf8_to_console(const std::string& s)
{
    std::wstring ws  = utf8_to_wstring(s);
    int          len = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
    std::string  str(len, 0);
    WideCharToMultiByte(CP_ACP, 0, ws.c_str(), -1, &str[0], len, NULL, NULL);
    if (!str.empty() && str.back() == '\0')
        str.pop_back();
    return str;
}

bool wide_file_exists(const std::string& path)
{
    std::wstring wpath = utf8_to_wstring(path);
    struct _stat info;
    return _wstat(wpath.c_str(), &info) == 0;
}

TimePoint wide_get_file_mod_time(const std::string& path)
{
    std::wstring wpath = utf8_to_wstring(path);
    struct _stat info;
    if (_wstat(wpath.c_str(), &info) == 0)
    {
        return info.st_mtime;
    }
    return 0;
}

bool wide_is_directory(const std::string& path)
{
    std::wstring wpath = utf8_to_wstring(path);
    struct _stat info;
    return _wstat(wpath.c_str(), &info) == 0 && (info.st_mode & _S_IFDIR);
}

int wide_create_directory(const std::string& path)
{
    std::wstring wpath = utf8_to_wstring(path);
    return _wmkdir(wpath.c_str());
}

FILE* wide_fopen(const std::string& path, const char* mode)
{
    std::wstring wpath = utf8_to_wstring(path);
    std::wstring wmode = utf8_to_wstring(std::string(mode));
    return _wfopen(wpath.c_str(), wmode.c_str());
}
#endif

TimePoint getFileModificationTime(const char* path)
{
#ifdef _WIN32
    std::string p(path);
    return wide_get_file_mod_time(p);
#else
    struct stat info;
    if (stat(path, &info) == 0)
    {
        return info.st_mtime;
    }
    return 0;
#endif
}

bool fileExists(const char* path)
{
#ifdef _WIN32
    std::string p(path);
    return wide_file_exists(p);
#else
    struct stat info;
    return stat(path, &info) == 0;
#endif
}

bool isDirectory(const char* path)
{
#ifdef _WIN32
    std::string p(path);
    return wide_is_directory(p);
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

int createDirectory(const char* path)
{
#ifdef _WIN32
    std::string p(path);
    return wide_create_directory(p);
#else
    return mkdir(path, 0755);
#endif
}

int createDirectoryRecursive(const char* path)
{
#ifdef _WIN32
    // Split and create using wide functions.
    std::string p   = path;
    size_t      pos = 0;
    while ((pos = p.find('\\', pos)) != std::string::npos)
    {
        std::string sub = p.substr(0, pos);
        if (!sub.empty() && !isDirectory(sub.c_str()))
        {
            wide_create_directory(sub);
        }
        pos++;
    }
    wide_create_directory(p);
#else
    // Assuming Unix-like
    std::string p   = path;
    size_t      pos = 0;
    while ((pos = p.find('/', pos)) != std::string::npos)
    {
        std::string sub = p.substr(0, pos);
        if (!sub.empty() && !isDirectory(sub.c_str()))
        {
            mkdir(sub.c_str(), 0755);
        }
        pos++;
    }
    mkdir(path, 0755);
#endif
    return 0;
}

int runCommand(const char* command)
{
    if (printMessages)
    {
        printf("Running: %s\n", command);
    }
#ifdef _WIN32
    STARTUPINFOW        si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    std::string         cmdStr  = command;
    std::wstring        wideCmd = utf8_to_wstring(cmdStr);
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
    return system(command);
#endif
}

std::string joinPaths(const std::string& a, const std::string& b)
{
#ifdef _WIN32
    return a + "\\" + b;
#else
    return a + "/" + b;
#endif
}

// Parse positional bootloader arguments
struct BootloaderArgs
{
    std::string libraryDir;
    std::string toolSourceDir;
    std::string buildDir;
    std::string toolName;

    std::vector<std::string> remainingArgs;
};

BootloaderArgs parseArgs(int argc, char* argv[])
{
    BootloaderArgs args;
    // Assume the first 4 are from SC.sh/bat, then tool name, then args
    if (argc >= 4)
    {
        args.libraryDir    = argv[1];
        args.toolSourceDir = argv[2];
        args.buildDir      = argv[3];
        args.toolName      = (argc >= 5) ? argv[4] : "build";
        for (int i = 5; i < argc; ++i)
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
    std::string toolCpp       = joinPaths(args.toolSourceDir, "SC-" + args.toolName + ".cpp");
    std::string toolSourceDir = args.toolSourceDir;
    std::string toolName      = args.toolName;
    std::string toolH         = "";

    if (!fileExists(toolCpp.c_str()))
    {
        // Try if toolName is a path to cpp file
        std::string potentialCpp = args.toolName;
        if (fileExists(potentialCpp.c_str()))
        {
            // Assume it's a cpp file
            if (potentialCpp.find(".cpp") == std::string::npos)
            {
                fprintf(stderr, "Error: Tool file must end with .cpp\n");
                exit(1);
            }
            // Get dir and name
#ifdef _WIN32
            // Simple path handling
            size_t lastSlash = potentialCpp.find_last_of("\\");
#else
            size_t lastSlash = potentialCpp.find_last_of("/");
#endif
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
        toolH = joinPaths(toolSourceDir, "SC-" + toolName + ".h");
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
    std::string pathSep = ci.targetOS == "Windows" ? "\\" : "/";
    ci.toolOutputDir    = args.buildDir + pathSep + "_Tools";
    ci.intermediateDir  = ci.toolOutputDir + pathSep + "_Intermediates" + pathSep + ci.targetOS;

    // For dependency files, absolute paths
    ci.toolsDepFile = ci.intermediateDir + pathSep + "Tools" + (ci.targetOS == "Windows" ? ".json" : ".d");
    ci.toolDepFile =
        ci.intermediateDir + pathSep + "SC-" + ci.args->toolName + (ci.targetOS == "Windows" ? ".json" : ".d");

    ci.toolCpp  = toolCpp;
    ci.scCpp    = joinPaths(args.libraryDir, "SC.cpp");
    ci.toolsCpp = joinPaths(toolSourceDir, "Tools.cpp");
    ci.toolH    = toolH;
    ci.toolExe  = joinPaths(joinPaths(ci.toolOutputDir, ci.targetOS), exeName + exeExt);

    return ci;
}

// Parse .d or .json file to get dependencies
std::vector<std::string> parseDependencies(const std::string& depFilePath)
{
    std::vector<std::string> deps;
    bool                     isJson = depFilePath.size() > 5 && depFilePath.substr(depFilePath.size() - 5) == ".json";

    if (isJson)
    {
        // Parse JSON dependency file (similar to jsonToDependencies.ps1)
#ifdef _WIN32
        FILE* file = wide_fopen(depFilePath, "r");
#else
        FILE* file = fopen(depFilePath.c_str(), "r");
#endif
        if (!file)
            return deps;

        char line[2048];
        bool inIncludes = false;
        while (fgets(line, sizeof(line), file))
        {
            std::string str = line;
            // Simple parse: look for Includes array
            if (str.find("\"Includes\"") != std::string::npos)
            {
                inIncludes = true;
            }
            if (inIncludes && str.find('"') != std::string::npos)
            {
                // Extract includes between quotes, filter out system headers
                size_t      start = 0, end;
                std::string includeStr;
                while ((start = str.find('"', start + 1)) != std::string::npos)
                {
                    end = str.find('"', start + 1);
                    if (end != std::string::npos)
                    {
                        std::string dep = str.substr(start + 1, end - start - 1);
                        // Filter out system headers (like jsonToDependencies.ps1 does)
                        if (!dep.empty() && dep.find("windows kits") == std::string::npos &&
                            dep.find("microsoft visual studio") == std::string::npos)
                        {
                            deps.push_back(dep);
                        }
                        start = end;
                    }
                    else
                        break;
                }
                if (str.find(']') != std::string::npos)
                    break; // End of array
            }
        }
        fclose(file);
    }
    else
    {
        // Parse .d file (POSIX style)
#ifdef _WIN32
        FILE* file = wide_fopen(depFilePath, "r");
#else
        FILE* file = fopen(depFilePath.c_str(), "r");
#endif
        if (!file)
            return deps;

        char        line[1024];
        bool        afterColon = false;
        std::string continuation;

        while (fgets(line, sizeof(line), file))
        {
            std::string str = line;
            // Remove trailing newline
            if (!str.empty() && str.back() == '\n')
                str.pop_back();
            if (!str.empty() && str.back() == '\r')
                str.pop_back();

            // Handle continuation
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

            // Parse after colon
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
                // Split by spaces, trim quotes
                size_t pos = 0;
                while ((pos = str.find(' ', pos)) != std::string::npos || !str.empty())
                {
                    size_t start = str.find_first_not_of(" \t");
                    if (start == std::string::npos)
                        break;
                    size_t end = str.find(' ', start);
                    if (end == std::string::npos)
                        end = str.size();
                    std::string dep = str.substr(start, end - start);
                    if (!dep.empty())
                    {
                        // Remove quotes
                        if (dep.size() >= 2 && dep[0] == '"' && dep.back() == '"')
                        {
                            dep = dep.substr(1, dep.size() - 2);
                        }
                        deps.push_back(dep);
                    }
                    str = (end < str.size()) ? str.substr(end + 1) : "";
                }
            }
        }
        fclose(file);
    }
    return deps;
}

// Check if Tools.o needs rebuilding
bool needsRebuildTools(const CompilationInfo& ci)
{
    std::string objExt   = ci.targetOS == "Windows" ? ".obj" : ".o";
    std::string toolsObj = joinPaths(ci.intermediateDir, "Tools" + objExt);
    TimePoint   objTime  = getFileModificationTime(toolsObj.c_str());
    if (objTime == 0)
    {
        if (printMessages)
            printf("  Tools obj file not found, needs rebuild\n");
        return true; // Obj doesn't exist
    }

    // Check core sources for Tools.o
    TimePoint scTime = getFileModificationTime(ci.scCpp.c_str());
    if (scTime > objTime)
    {
        if (printMessages)
        {
            printf("  SC.cpp modified (time %lld > %lld), needs rebuild\n", static_cast<long long>(scTime),
                   static_cast<long long>(objTime));
        }
        return true;
    }
    TimePoint cppTime = getFileModificationTime(ci.toolsCpp.c_str());
    if (cppTime > objTime)
    {
        if (printMessages)
        {

            printf("  Tools.cpp modified (time %lld > %lld), needs rebuild\n", static_cast<long long>(cppTime),
                   static_cast<long long>(objTime));
        }
        return true;
    }

    // Check dependencies
    std::vector<std::string> toolsDeps = parseDependencies(ci.toolsDepFile);
    if (printMessages)
    {
        printf("- Found %zu dependencies for \"%s\"\n", toolsDeps.size(), ci.toolsDepFile.c_str());
    }
    for (const auto& dep : toolsDeps)
    {
        TimePoint depTime = getFileModificationTime(dep.c_str());
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
    std::string toolObj     = joinPaths(ci.intermediateDir, toolObjName);
    TimePoint   objTime     = getFileModificationTime(toolObj.c_str());
    if (objTime == 0)
    {
        if (printMessages)
        {
            printf("  Obj file %s not found, needs rebuild\n", toolObj.c_str());
        }
        return true; // Obj doesn't exist
    }

    // Check tool sources and headers
    TimePoint cppTime = getFileModificationTime(ci.toolCpp.c_str());
    if (cppTime > objTime)
    {
        if (printMessages)
        {
            printf("  Cpp %s modified (time %lld > %lld), needs rebuild\n", ci.toolCpp.c_str(),
                   static_cast<long long>(cppTime), static_cast<long long>(objTime));
        }
        return true;
    }
    if (!ci.toolH.empty())
    {
        TimePoint hTime = getFileModificationTime(ci.toolH.c_str());
        if (hTime > objTime)
        {
            if (printMessages)
            {
                printf("  Header %s modified (time %lld > %lld), needs rebuild\n", ci.toolH.c_str(),
                       static_cast<long long>(hTime), static_cast<long long>(objTime));
            }
            return true;
        }
    }

    // Check dependencies from tool .d or .json
    std::vector<std::string> toolDeps = parseDependencies(ci.toolDepFile);
    if (printMessages)
    {
        printf("- Found %zu dependencies for %s \n", toolDeps.size(), ci.toolCpp.c_str());
    }
    for (const auto& dep : toolDeps)
    {
        TimePoint depTime = getFileModificationTime(dep.c_str());
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

    if (printMessages)
    {
        printf("- \"%s\" object file up to date\n", ci.toolCpp.c_str());
    }
    return false;
}

// Simple check if need to rebuild (link)
bool needsRebuildExe(const CompilationInfo& ci)
{
    TimePoint exeTime = getFileModificationTime(ci.toolExe.c_str());
    if (exeTime == 0)
        return true; // Exe doesn't exist

    std::string toolsObj    = joinPaths(ci.intermediateDir, "Tools.o");
    std::string toolObjName = "SC-" + ci.args->toolName + ".o";
    std::string toolObj     = joinPaths(ci.intermediateDir, toolObjName);

    TimePoint toolsObjTime = getFileModificationTime(toolsObj.c_str());
    TimePoint toolObjTime  = getFileModificationTime(toolObj.c_str());

    if (toolsObjTime > exeTime || toolObjTime > exeTime)
        return true;

    return false;
}

int compilePOSIX(const CompilationInfo& ci)
{
    // Create directories recursively
    if (!isDirectory(ci.intermediateDir.c_str()))
    {
        createDirectoryRecursive(ci.intermediateDir.c_str());
    }
    std::string outputDir = joinPaths(ci.toolOutputDir, ci.targetOS);
    if (!isDirectory(outputDir.c_str()))
    {
        createDirectoryRecursive(outputDir.c_str());
    }

    // Flags
    std::string commonFlags = " -I\"../../..\" -std=c++14 -pthread -fstrict-aliasing -fvisibility=hidden "
                              "-fvisibility-inlines-hidden -fno-rtti -fno-exceptions";
    std::string linkFlags   = " -ldl -lpthread";

    // Determine OS and compiler (match Makefile logic)
    std::string targetOS  = "";
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
        std::string toolsObj = joinPaths(ci.intermediateDir, "Tools.o");
        std::string cmd1 = compiler + " " + commonFlags + " -MMD -o \"" + toolsObj + "\" -c \"" + ci.toolsCpp + "\"";
        if (runCommand(cmd1.c_str()) != 0)
            return 1;
    }

    // Compile tool .o if needed
    if (needsRebuildToolObj(ci))
    {
        std::string toolObjName = "SC-" + ci.args->toolName + ".o";
        std::string toolObj     = joinPaths(ci.intermediateDir, toolObjName);
        std::string cmd2 = compiler + " " + commonFlags + " -MMD -o \"" + toolObj + "\" -c \"" + ci.toolCpp + "\"";
        if (runCommand(cmd2.c_str()) != 0)
            return 1;
    }

    // Link if needed
    if (needsRebuildExe(ci))
    {
        std::string toolsObj    = joinPaths(ci.intermediateDir, "Tools.o");
        std::string toolObjName = "SC-" + ci.args->toolName + ".o";
        std::string toolObj     = joinPaths(ci.intermediateDir, toolObjName);
        std::string cmd3 =
            compiler + " -o \"" + ci.toolExe + "\" \"" + toolsObj + "\" \"" + toolObj + "\" " + linkFlags;
        if (runCommand(cmd3.c_str()) != 0)
            return 1;
    }

    return 0;
}

bool compileWindows(const CompilationInfo& ci, bool& objsCompiled)
{
    // Create directories recursively
    if (!isDirectory(ci.intermediateDir.c_str()))
    {
        createDirectoryRecursive(ci.intermediateDir.c_str());
    }
    std::string outputDir = joinPaths(ci.toolOutputDir, ci.targetOS);
    if (!isDirectory(outputDir.c_str()))
    {
        createDirectoryRecursive(outputDir.c_str());
    }

    // Flags
    std::string commonFlags = " /nologo /I\"../../..\" /std:c++14 /MTd /permissive- /EHsc";
    std::string linkFlags   = " Advapi32.lib Shell32.lib";

    // Compile Tools.obj if needed
    if (needsRebuildTools(ci))
    {
        objsCompiled          = true;
        std::string toolsObj  = joinPaths(ci.intermediateDir, "Tools.obj");
        std::string toolsJson = joinPaths(ci.intermediateDir, "Tools.json");
        std::string cmd1      = "cl.exe " + commonFlags + " /sourceDependencies \"" + toolsJson + "\" /c /Fd\"" +
                           joinPaths(ci.intermediateDir, "Tools.pdb") + "\" /Fo\"" + toolsObj + "\" \"" + ci.toolsCpp +
                           "\"";
        if (runCommand(cmd1.c_str()) != 0)
            return false;
    }

    // Compile tool .obj if needed
    if (needsRebuildToolObj(ci))
    {
        objsCompiled            = true;
        std::string toolObjName = "SC-" + ci.args->toolName + ".obj";
        std::string toolObj     = joinPaths(ci.intermediateDir, toolObjName);
        std::string toolJson    = joinPaths(ci.intermediateDir, "SC-" + ci.args->toolName + ".json");
        std::string cmd2        = "cl.exe " + commonFlags + " /sourceDependencies \"" + toolJson + "\" /c /Fd\"" +
                           joinPaths(ci.intermediateDir, toolObjName.substr(0, toolObjName.size() - 4) + ".pdb") +
                           "\" /Fo\"" + toolObj + "\" \"" + ci.toolCpp + "\"";
        if (runCommand(cmd2.c_str()) != 0)
            return false;
    }

    // Link if needed (or if objs compiled)
    // Note: Link is outside, handled by caller
    return true;
}

int executeTool(const BootloaderArgs& args, const CompilationInfo& ci)
{
    std::string cmd = "\"" + ci.toolExe + "\" \"" + args.libraryDir + "\" \"" + args.toolSourceDir + "\" \"" +
                      args.buildDir + "\" \"" + args.toolName + "\"";
    for (const auto& arg : args.remainingArgs)
    {
        cmd += " \"" + arg + "\"";
    }
    return runCommand(cmd.c_str());
}

int main(int argc, char* argv[])
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
        return 1;
    }
    std::vector<std::string> new_argv;
    for (int i = 0; i < argc; ++i)
    {
        int         len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        std::string str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &str[0], len, NULL, NULL);
        new_argv.push_back(str);
    }
    LocalFree(wargv);

    std::vector<char*> new_argv_ptr;
    for (auto& s : new_argv)
    {
        new_argv_ptr.push_back(&s[0]);
    }
    argc = new_argv_ptr.size();
    argv = new_argv_ptr.data();
#endif
    BootloaderArgs  args = parseArgs(argc, argv);
    CompilationInfo ci   = setupCompilation(args);

    bool rebuildNeeded = needsRebuildTools(ci) || needsRebuildToolObj(ci) || needsRebuildExe(ci);

    if (rebuildNeeded)
    {
        if (printMessages)
        {
            printf("Rebuilding %s tool...\n", args.toolName.c_str());
        }
        auto start_time   = std::chrono::high_resolution_clock::now();
        bool objsCompiled = false;
#ifdef _WIN32
        if (!compileWindows(ci, objsCompiled))
        {
            fprintf(stderr, "Compilation failed\n");
            return 1;
        }
        if (objsCompiled || needsRebuildExe(ci))
        {
            printf("Linking %s\n", args.toolName.c_str());
            std::string objExt      = ci.targetOS == "Windows" ? ".obj" : ".o";
            std::string toolsObj    = joinPaths(ci.intermediateDir, "Tools" + objExt);
            std::string toolObjName = "SC-" + ci.args->toolName + objExt;
            std::string toolObj     = joinPaths(ci.intermediateDir, toolObjName);
            std::string linkFlags   = " Advapi32.lib Shell32.lib";
            std::string cmd3 =
                "link /nologo /OUT:\"" + ci.toolExe + "\" \"" + toolsObj + "\" \"" + toolObj + "\" " + linkFlags;
            if (runCommand(cmd3.c_str()) != 0)
                return 1;
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
