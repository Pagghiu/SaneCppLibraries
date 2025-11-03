// ToolsBootstrap.c - Single-file C bootstrap for build system
// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// Declare popen/pclose for systems where they're not in stdio.h
#ifndef _WIN32
extern FILE *popen(const char *command, const char *type);
extern int pclose(FILE *stream);
#endif

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <shellapi.h>
#include <sys/stat.h>
#define mkdir _mkdir
#define stat _stat
#define S_IFDIR _S_IFDIR
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define POPEN popen
#define PCLOSE pclose
#endif
#include <time.h>
#include <stdint.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#ifndef _WIN32
#include <sys/time.h>
#endif

int printMessages = 0;
// Platform abstractions
typedef unsigned long long TimePoint;

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
} StringBuilder;

typedef struct {
    char** data;
    size_t count;
    size_t capacity;
} StringVector;

typedef struct {
    char separator;
} PathContext;

PathContext Path_init(void);

typedef struct {
    PathContext path;
} FileSystemContext;

FileSystemContext FileSystem_init(void);

typedef struct {
    int numRemainingArgs;
    char* libraryDir;
    char* toolSourceDir;
    char* buildDir;
    char* toolName;
    char** remainingArgs;
} BootloaderArgs;

void BootloaderArgs_init(BootloaderArgs* args);
void BootloaderArgs_destroy(BootloaderArgs* args);

typedef struct {
    BootloaderArgs* args;
    char* targetOS;
    char* toolOutputDir;
    char* intermediateDir;
    char* toolCpp;
    char* scCpp;
    char* toolsCpp;
    char* toolH;
    char* toolExe;
    char* toolsDepFile;
    char* toolDepFile;
} CompilationInfo;

CompilationInfo CompilationInfo_init(BootloaderArgs* args);
void CompilationInfo_destroy(CompilationInfo* ci);

// CommandLine
typedef struct {
    StringVector args;
} CommandLine;

void CommandLine_init(CommandLine* cl, const char* program);
void CommandLine_arg(CommandLine* cl, const char* argument);
void CommandLine_argQuoted(CommandLine* cl, const char* argument);
int CommandLine_run(CommandLine* cl);
void CommandLine_destroy(CommandLine* cl);

// Command execution
int runCommand(const char* command);

// Path and filesystem functions
char* Path_join(const char* a, const char* b);
int FileSystem_createDirectoryRecursive(const char* path);
int FileSystem_createDirectory(const char* path);
TimePoint FileSystem_getModificationTime(const char* path);
int FileSystem_exists(const char* path);
int FileSystem_isDirectory(const char* path);
FILE* FileSystem_open(const char* path, const char* mode);

// String/vector utilities
void StringVector_init(StringVector* sv, size_t initial_capacity);
void StringVector_destroy(StringVector* sv);
void StringVector_add(StringVector* sv, const char* str);
char* StringVector_join(const StringVector* sv, const char* sep);
char* StringBuilder_join(const StringBuilder* sb, const char* sep);

// Implementation
PathContext Path_init(void) {
    PathContext ctx;
#ifdef _WIN32
    ctx.separator = '\\';
#else
    ctx.separator = '/';
#endif
    return ctx;
}

FileSystemContext FileSystem_init(void) {
    FileSystemContext ctx;
    ctx.path = Path_init();
    return ctx;
}

// StringBuilder utilities
StringBuilder StringBuilder_init(size_t initial_capacity) {
    StringBuilder sb = {0};
    sb.capacity = initial_capacity > 0 ? initial_capacity : 256;
    sb.buffer = (char*)malloc(sb.capacity);
    if (!sb.buffer) return sb;
    sb.buffer[0] = 0;
    sb.length = 0;
    return sb;
}

void StringBuilder_destroy(StringBuilder* sb) {
    if (sb->buffer) free(sb->buffer);
    sb->buffer = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

void StringBuilder_append(StringBuilder* sb, const char* str) {
    size_t len = strlen(str);
    if (sb->length + len >= sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        if (new_capacity < sb->length + len + 1) new_capacity = sb->length + len + 1;
        char* new_buffer = (char*)realloc(sb->buffer, new_capacity);
        if (!new_buffer) return;
        sb->buffer = new_buffer;
        sb->capacity = new_capacity;
    }
    memcpy(sb->buffer + sb->length, str, len + 1);
    sb->length += len;
}

char* StringBuilder_get_buffer(StringBuilder* sb) {
    return sb->buffer;
}

// StringVector utilities
void StringVector_init(StringVector* sv, size_t initial_capacity) {
    sv->data = NULL;
    sv->count = 0;
    sv->capacity = initial_capacity > 0 ? initial_capacity : 8;
    if (sv->capacity > 0) {
        sv->data = (char**)malloc(sizeof(char*) * sv->capacity);
        if (!sv->data) sv->capacity = 0;
    }
}

void StringVector_destroy(StringVector* sv) {
    if (sv->data) {
        for (size_t i = 0; i < sv->count; ++i) {
            if (sv->data[i]) free(sv->data[i]);
        }
        free(sv->data);
        sv->data = NULL;
    }
    sv->count = 0;
    sv->capacity = 0;
}

void StringVector_add(StringVector* sv, const char* str) {
    if (sv->count >= sv->capacity) {
        size_t new_capacity = sv->capacity * 2;
        if (new_capacity == 0) new_capacity = 8;
        char** new_data = (char**)realloc(sv->data, sizeof(char*) * new_capacity);
        if (!new_data) return;
        sv->data = new_data;
        sv->capacity = new_capacity;
    }
    sv->data[sv->count] = (char*)malloc(strlen(str) + 1);
    if (sv->data[sv->count]) {
        strcpy(sv->data[sv->count], str);
        sv->count++;
    }
}

char* StringVector_join(const StringVector* sv, const char* sep) {
    if (sv->count == 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = 0;
        return empty;
    }
    size_t sep_len = strlen(sep);
    size_t total_len = 0;
    for (size_t i = 0; i < sv->count; ++i) {
        total_len += strlen(sv->data[i]);
    }
    total_len += (sv->count - 1) * sep_len + 1;
    char* result = (char*)malloc(total_len);
    if (!result) return NULL;
    result[0] = 0;
    for (size_t i = 0; i < sv->count; ++i) {
        if (i > 0) strcat(result, sep);
        strcat(result, sv->data[i]);
    }
    return result;
}

char* StringBuilder_join(const StringBuilder* sb, const char* sep) {
    // For StringBuilder, join is not typical, but perhaps split by sep and join
    // Actually, looking at usage, it might not be needed, but implement as simple copy
    char* result = (char*)malloc(sb->length + 1);
    if (result) {
        memcpy(result, sb->buffer, sb->length + 1);
    }
    return result;
}

// Path functions
char* Path_join(const char* a, const char* b) {
    if (!a || !b) return NULL;
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a == 0) {
        char* copy = (char*)malloc(len_b + 1);
        if (copy) strcpy(copy, b);
        return copy;
    }
    if (len_b == 0) {
        char* copy = (char*)malloc(len_a + 1);
        if (copy) strcpy(copy, a);
        return copy;
    }
    char sep = Path_init().separator;
    char last = a[len_a - 1];
    size_t sep_add = (last != '/' && last != '\\') ? 1 : 0;
    char* result = (char*)malloc(len_a + sep_add + len_b + 1);
    if (!result) return NULL;
    strcpy(result, a);
    if (sep_add) result[len_a] = sep;
    strcpy(result + len_a + sep_add, b);
    return result;
}

// FileSystem functions
FILE* FileSystem_open(const char* path, const char* mode) {
#ifdef _WIN32
    size_t len = strlen(path);
    wchar_t* wpath = NULL;
    size_t wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wpath = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    if (wpath) {
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
        size_t mlen = strlen(mode);
        wchar_t* wmode = (wchar_t*)malloc((mlen + 1) * sizeof(wchar_t));
        if (wmode) {
            for (size_t i = 0; i <= mlen; ++i) wmode[i] = mode[i];
            FILE* fp = _wfopen(wpath, wmode);
            free(wmode);
            free(wpath);
            return fp;
        }
        free(wpath);
    }
    return NULL;
#else
    return fopen(path, mode);
#endif
}

TimePoint FileSystem_getModificationTime(const char* path) {
#ifdef _WIN32
    struct _stat info;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen > 0) {
        wchar_t* wpath = (wchar_t*)malloc(wlen * sizeof(wchar_t));
        if (wpath) {
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
            int ret = _wstat(wpath, &info);
            free(wpath);
            if (ret == 0) {
                return info.st_mtime;
            }
        }
    }
    return 0;
#else
    struct stat info;
    if (stat(path, &info) == 0) {
        return info.st_mtime;
    }
    return 0;
#endif
}

int FileSystem_exists(const char* path) {
#ifdef _WIN32
    struct _stat info;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen > 0) {
        wchar_t* wpath = (wchar_t*)malloc(wlen * sizeof(wchar_t));
        if (wpath) {
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
            int ret = _wstat(wpath, &info);
            free(wpath);
            return ret == 0;
        }
    }
    return 0;
#else
    struct stat info;
    return stat(path, &info) == 0;
#endif
}

int FileSystem_isDirectory(const char* path) {
#ifdef _WIN32
    struct _stat info;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen > 0) {
        wchar_t* wpath = (wchar_t*)malloc(wlen * sizeof(wchar_t));
        if (wpath) {
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
            int ret = _wstat(wpath, &info);
            free(wpath);
            if (ret != 0) return 0;
            return (info.st_mode & _S_IFDIR) != 0;
        }
    }
    return 0;
#else
    struct stat info;
    if (stat(path, &info) != 0) return 0;
    return S_ISDIR(info.st_mode);
#endif
}

int FileSystem_createDirectory(const char* path) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen > 0) {
        wchar_t* wpath = (wchar_t*)malloc(wlen * sizeof(wchar_t));
        if (wpath) {
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
            int ret = _wmkdir(wpath);
            free(wpath);
            return ret;
        }
    }
    return -1;
#else
    return mkdir(path, 0755);
#endif
}

int FileSystem_createDirectoryRecursive(const char* path) {
    StringVector subpaths;
    StringVector_init(&subpaths, 16);
    const char* p = path;
    size_t i = 0;
    char last_sep = 0;
    char sep = Path_init().separator;
    while (*p) {
        if (*p == sep || *p == '/' || *p == '\\') {
            if (i > 0) { // Avoid empty strings
                char* substr = (char*)malloc(i + 1);
                if (substr) {
                    memcpy(substr, path, i);
                    substr[i] = 0;
                    // Skip drive letters like "y:"
                    if (!(strlen(substr) == 2 && substr[1] == ':')) {
                        StringVector_add(&subpaths, substr);
                    }
                    free(substr);
                }
            }
            last_sep = *p;
        }
        i++;
        p++;
    }
    // Add the full path
    if (last_sep) {
        StringVector_add(&subpaths, path);
    }
    int ret = 0;
    for (size_t j = 0; j < subpaths.count; ++j) {
        if (!FileSystem_isDirectory(subpaths.data[j])) {
            if (FileSystem_createDirectory(subpaths.data[j]) != 0) {
                ret = -1;
                break;
            }
        }
    }
    StringVector_destroy(&subpaths);
    return ret;
}

// BootloaderArgs functions
void BootloaderArgs_init(BootloaderArgs* args) {
    args->numRemainingArgs = 0;
    args->libraryDir = NULL;
    args->toolSourceDir = NULL;
    args->buildDir = NULL;
    args->toolName = NULL;
    args->remainingArgs = NULL;
}

void BootloaderArgs_destroy(BootloaderArgs* args) {
    if (args->libraryDir) free(args->libraryDir);
    if (args->toolSourceDir) free(args->toolSourceDir);
    if (args->buildDir) free(args->buildDir);
    if (args->toolName) free(args->toolName);
    if (args->remainingArgs) {
        for (int i = 0; i < args->numRemainingArgs; ++i) {
            free(args->remainingArgs[i]);
        }
        free(args->remainingArgs);
    }
}

// CompilationInfo functions
CompilationInfo CompilationInfo_init(BootloaderArgs* args) {
    CompilationInfo ci;
    ci.args = args;
    ci.targetOS = NULL;
    ci.toolOutputDir = NULL;
    ci.intermediateDir = NULL;
    ci.toolCpp = NULL;
    ci.scCpp = NULL;
    ci.toolsCpp = NULL;
    ci.toolH = NULL;
    ci.toolExe = NULL;
    ci.toolsDepFile = NULL;
    ci.toolDepFile = NULL;

    return ci;
}

void CompilationInfo_destroy(CompilationInfo* ci) {
    if (ci->targetOS) free(ci->targetOS);
    if (ci->toolOutputDir) free(ci->toolOutputDir);
    if (ci->intermediateDir) free(ci->intermediateDir);
    if (ci->toolCpp) free(ci->toolCpp);
    if (ci->scCpp) free(ci->scCpp);
    if (ci->toolsCpp) free(ci->toolsCpp);
    if (ci->toolH) free(ci->toolH);
    if (ci->toolExe) free(ci->toolExe);
    if (ci->toolsDepFile) free(ci->toolsDepFile);
    if (ci->toolDepFile) free(ci->toolDepFile);
}

// getArgv functions
char** getArgv(int* out_argc, char** argv, int real_argc) {
#ifdef _WIN32
    // Set console output to UTF-8 to properly display Unicode
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), out_argc);
    if (!wargv) return NULL;
    char** args = (char**)malloc(*out_argc * sizeof(char*));
    if (!args) {
        LocalFree(wargv);
        return NULL;
    }
    for (int i = 0; i < *out_argc; ++i) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        args[i] = (char*)malloc(len);
        if (!args[i]) continue;
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, args[i], len, NULL, NULL);
    }
    LocalFree(wargv);
    return args;
#else
    *out_argc = real_argc;
    char** args = (char**)malloc(real_argc * sizeof(char*));
    if (!args) return NULL;
    for (int i = 0; i < real_argc; ++i) {
        args[i] = (char*)malloc(strlen(argv[i]) + 1);
        if (args[i]) strcpy(args[i], argv[i]);
    }
    return args;
#endif
}

void freeArgs(int argc, char** args) {
    for (int i = 0; i < argc; ++i) {
        if (args[i]) free(args[i]);
    }
    free(args);
}

// parseArgs
BootloaderArgs parseArgs(const char** string_argv, int argc) {
    BootloaderArgs args;
    BootloaderArgs_init(&args);
    // Assume the first 4 are from SC.sh/bat, then tool name, then args
    if (argc >= 4) {
        args.libraryDir = (char*)malloc(strlen(string_argv[1]) + 1);
        if (args.libraryDir) strcpy(args.libraryDir, string_argv[1]);
        args.toolSourceDir = (char*)malloc(strlen(string_argv[2]) + 1);
        if (args.toolSourceDir) strcpy(args.toolSourceDir, string_argv[2]);
        args.buildDir = (char*)malloc(strlen(string_argv[3]) + 1);
        if (args.buildDir) strcpy(args.buildDir, string_argv[3]);
        args.toolName = (argc >= 5) ? (char*)malloc(strlen(string_argv[4]) + 1) : (char*)malloc(strlen("build") + 1);
        if (args.toolName) strcpy(args.toolName, (argc >= 5) ? string_argv[4] : "build");
        args.numRemainingArgs = (argc > 5) ? (argc - 5) : 0;
        if (args.numRemainingArgs > 0) {
            args.remainingArgs = (char**)malloc(args.numRemainingArgs * sizeof(char*));
            if (args.remainingArgs) {
                for (int i = 0; i < args.numRemainingArgs; ++i) {
                    args.remainingArgs[i] = (char*)malloc(strlen(string_argv[5 + i]) + 1);
                    if (args.remainingArgs[i]) strcpy(args.remainingArgs[i], string_argv[5 + i]);
                }
            }
        }
    } else {
        args.toolName = (char*)malloc(strlen("build") + 1);
        if (args.toolName) strcpy(args.toolName, "build");
    }
    return args;
}

// setupCompilation
void setupCompilation(CompilationInfo* ci) {
    BootloaderArgs* args = ci->args;
#ifdef _WIN32
    ci->targetOS = (char*)malloc(strlen("Windows") + 1);
    if (ci->targetOS) strcpy(ci->targetOS, "Windows");
#else
    FILE* uname_file = POPEN("uname", "r");
    if (uname_file) {
        char buf[256];
        if (fgets(buf, sizeof(buf), uname_file)) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = 0;
            ci->targetOS = (char*)malloc(len + 1);
            if (ci->targetOS) strcpy(ci->targetOS, buf);
        }
        PCLOSE(uname_file);
    }
    if (!ci->targetOS) {
        ci->targetOS = (char*)malloc(strlen("POSIX") + 1);
        if (ci->targetOS) strcpy(ci->targetOS, "POSIX");
    }
#endif

    // Set paths like original
    char* toolCpp = Path_join(args->toolSourceDir, "SC-");
    toolCpp = (char*)realloc(toolCpp, strlen(toolCpp) + strlen(args->toolName) + strlen(".cpp") + 1);
    if (toolCpp) {
        strcat(toolCpp, args->toolName);
        strcat(toolCpp, ".cpp");
    }

    if (!FileSystem_exists(toolCpp)) {
        // Try if toolName is path to cpp file
        char* potentialCpp = (char*)malloc(strlen(args->toolName) + 1);
        if (potentialCpp) strcpy(potentialCpp, args->toolName);
        if (FileSystem_exists(potentialCpp)) {
            free(toolCpp);
            toolCpp = potentialCpp;
        } else {
            free(potentialCpp);
            fprintf(stderr, "Error: Tool \"%s\" doesn't exist\n", args->toolName);
            free(toolCpp);
            exit(1);
        }
    }

    // Set ci fields
    ci->toolCpp = toolCpp;
    ci->scCpp = Path_join(args->libraryDir, "SC.cpp");
    ci->toolsCpp = Path_join(args->toolSourceDir, "Tools.cpp");
    ci->toolOutputDir = Path_join(args->buildDir, "_Tools");

    // intermediateDir
    char* inter1 = Path_join(ci->toolOutputDir, "_Intermediates");
    ci->intermediateDir = Path_join(inter1, ci->targetOS ? ci->targetOS : "Unknown");
    free(inter1);

    // dep files
    char* ext = ci->targetOS && strcmp(ci->targetOS, "Windows") == 0 ? ".json" : ".d";
    char* temp = Path_join(ci->intermediateDir, "Tools");
    ci->toolsDepFile = (char*)malloc(strlen(temp) + strlen(ext) + 1);
    if (ci->toolsDepFile) {
        strcpy(ci->toolsDepFile, temp);
        strcat(ci->toolsDepFile, ext);
    }
    free(temp);
    temp = Path_join(ci->intermediateDir, "SC-");
    temp = (char*)realloc(temp, strlen(temp) + strlen(args->toolName) + strlen(ext) + 1);
    if (temp) {
        strcat(temp, args->toolName);
        strcat(temp, ext);
        ci->toolDepFile = temp;
    }

    // toolH
    if (strstr(toolCpp, "ToolsBootstrap.") != toolCpp) { // Not this file
        ci->toolH = Path_join(args->toolSourceDir, "SC-");
        ci->toolH = (char*)realloc(ci->toolH, strlen(ci->toolH) + strlen(args->toolName) + strlen(".h") + 1);
        if (ci->toolH) {
            strcat(ci->toolH, args->toolName);
            strcat(ci->toolH, ".h");
        }
    }

    // toolExe
    char* exeDir = Path_join(ci->toolOutputDir, ci->targetOS ? ci->targetOS : "Unknown");
    char* exeName = "SC-";
    exeName = (char*)malloc(strlen(exeName) + strlen(args->toolName) + 1);
    if (exeName) {
        strcpy(exeName, "SC-");
        strcat(exeName, args->toolName);
        char* exeExt = "";
#ifdef _WIN32
        exeExt = ".exe";
#endif
        exeName = (char*)realloc(exeName, strlen(exeName) + strlen(exeExt) + 1);
        if (exeName) strcat(exeName, exeExt);
        ci->toolExe = Path_join(exeDir, exeName);
        free(exeName);
    }
    free(exeDir);
}

// Command execution implementation
int runCommand(const char* command) {
    if (printMessages) {
        printf("Running: %s\n", command);
    }
#ifdef _WIN32
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    size_t len = strlen(command);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
    if (wlen == 0) return 1;
    wchar_t* wcmd = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    if (!wcmd) return 1;
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wcmd, wlen);
    if (CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(wcmd);
        return exitCode == 0 ? 0 : 1;
    } else {
        free(wcmd);
        return 1;
    }
#else
    return system(command);
#endif
}

// CommandLine implementation
void CommandLine_init(CommandLine* cl, const char* program) {
    StringVector_init(&cl->args, 8);
    CommandLine_argQuoted(cl, program);
}

void CommandLine_arg(CommandLine* cl, const char* argument) {
    StringVector_add(&cl->args, argument);
}

void CommandLine_argQuoted(CommandLine* cl, const char* argument) {
    StringBuilder sb = StringBuilder_init(128);
    StringBuilder_append(&sb, "\"");
    StringBuilder_append(&sb, argument);
    StringBuilder_append(&sb, "\"");
    StringVector_add(&cl->args, StringBuilder_get_buffer(&sb));
    StringBuilder_destroy(&sb);
}

int CommandLine_run(CommandLine* cl) {
    char* cmd = StringVector_join(&cl->args, " ");
    if (!cmd) return 1;
    int ret = runCommand(cmd);
    free(cmd);
    return ret;
}

void CommandLine_destroy(CommandLine* cl) {
    StringVector_destroy(&cl->args);
}

void StringVector_addFromString(StringVector* sv, const char* str, const char* delim) {
    char* s = (char*)malloc(strlen(str) + 1);
    strcpy(s, str);
    char* token = strtok(s, delim);
    while (token) {
        StringVector_add(sv, token);
        token = strtok(NULL, delim);
    }
    free(s);
}

char* unescapeMakeDependency(const char* input) {
    size_t input_len = strlen(input);
    char* out = (char*)malloc(input_len + 1);
    if (!out) return NULL;
    size_t out_pos = 0;
    int escaping = 0;
    for (size_t i = 0; i < input_len; ++i) {
        char ch = input[i];
        if (escaping) {
            out[out_pos++] = ch;
            escaping = 0;
        } else if (ch == '\\') {
            escaping = 1;
        } else {
            out[out_pos++] = ch;
        }
    }
    out[out_pos] = 0;
    return out;
}

StringVector parseJsonDependencies(FILE* file) {
    StringVector deps;
    StringVector_init(&deps, 16);

    char line[2048];
    int inIncludes = 0;
    int inArray = 0;
    while (fgets(line, sizeof(line), file)) {
        char* str = line;
        if (strstr(str, "\"Includes\"")) {
            inIncludes = 1;
        }
        if (inIncludes) {
            if (strstr(str, "[")) {
                inArray = 1;
            }
            if (strstr(str, "]")) {
                inArray = 0;
                break; // End of array
            }
        }
        if (inIncludes && inArray && strstr(str, "\"")) {
            char* start = strstr(str, "\"");
            while (start) {
                char* end = strstr(start + 1, "\"");
                if (end) {
                    *end = 0;
                    char* dep = start + 1;
                    if (strlen(dep) > 0 && strstr(dep, "windows kits") == NULL &&
                        strstr(dep, "microsoft visual studio") == NULL && strcmp(dep, "Includes") != 0 &&
                        strcmp(dep, "ImportedModules") != 0) {
                        StringVector_add(&deps, dep);
                    }
                    start = strstr(end + 1, "\"");
                } else {
                    break;
                }
            }
        }
    }
    return deps;
}

StringVector parseMakeDependencies(FILE* file, const char* baseDir) {
    StringVector deps;
    StringVector_init(&deps, 16);

    char line[1024];
    char* all_deps = NULL;
    size_t all_deps_len = 0;
    int afterColon = 0;
    while (fgets(line, sizeof(line), file)) {
        char* str = (char*)malloc(strlen(line) + 1);
        strcpy(str, line);
        if (str[strlen(str) - 1] == '\n') str[strlen(str) - 1] = 0;
        if (str[strlen(str) - 1] == '\r') str[strlen(str) - 1] = 0;

        if (!afterColon) {
            char* colon = strstr(str, ":");
            if (colon) {
                char* deps_start = colon + 1;
                all_deps = (char*)malloc(strlen(deps_start) + 1);
                strcpy(all_deps, deps_start);
                all_deps_len = strlen(all_deps);
                afterColon = 1;
            }
            free(str);
            continue;
        }

    if (afterColon) {
        // Remove trailing backslash
        size_t len = strlen(str);
        if (len >= 2 && str[len - 2] == '\\' && str[len - 1] == '\n') {
            str[len - 2] = 0;
            len -= 2;
        }
        // Append
        size_t new_len = all_deps_len + 1 + len + 1;
        all_deps = (char*)realloc(all_deps, new_len);
        if (all_deps_len > 0) {
            strcat(all_deps, " ");
        }
        strcat(all_deps, str);
        all_deps_len = strlen(all_deps);
    }
        free(str);
    }

    if (all_deps) {
        if (printMessages) {
            printf("all_deps: '%s'\n", all_deps);
        }
        // Now parse all_deps
        char* start = all_deps;
        while (*start && (*start == ' ' || *start == '\t')) start++;
        while (*start) {
            char* end = start;
            while (*end && *end != ' ') {
                if (*end == '\\' && *(end + 1) == ' ') {
                    end += 2; // skip \ and space
                } else {
                    end++;
                }
            }
            size_t len = end - start;
            if (len > 0) {
                char* dep = (char*)malloc(len + 1);
                memcpy(dep, start, len);
                dep[len] = 0;
                if (dep[0] == '"' && dep[len - 1] == '"') {
                    dep[len - 1] = 0;
                    memmove(dep, dep + 1, len - 1);
                }
                // Unescape backslash sequences in make dependencies (e.g., \ for spaces)
                char* unescaped = unescapeMakeDependency(dep);
                free(dep);
                if (strstr(unescaped, "\n") == NULL && strstr(unescaped, "\r") == NULL) {
                    // Skip empty or whitespace-only deps
                    char* trimmed = unescaped;
                    while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) trimmed++;
                    if (*trimmed) {
                        StringVector_add(&deps, unescaped);
                    } else {
                        free(unescaped);
                    }
                } else {
                    free(unescaped);
                }
            }
            start = end;
            while (*start && (*start == ' ' || *start == '\t')) start++;
        }
        free(all_deps);
    }

    return deps;
}

// Rebuild check functions
StringVector parseDependencies(const char* depFilePath, const char* baseDir) {
    FILE* file = FileSystem_open(depFilePath, "r");
    if (!file) {
        StringVector empty;
        StringVector_init(&empty, 0);
        return empty;
    }

    StringVector deps;
    int isJson = strstr(depFilePath, ".json") != NULL;
    if (isJson) {
        deps = parseJsonDependencies(file);
    } else {
        deps = parseMakeDependencies(file, baseDir);
    }
    if (printMessages) {
        printf("- Found %zu dependencies for \"%s\"\n", deps.count, depFilePath);
        for (size_t i = 0; i < deps.count && i < 10; ++i) {
            printf("  Dep %zu: %s\n", i, deps.data[i]);
        }
        if (deps.count > 10) printf("  ... and %zu more\n", deps.count - 10);
    }
    for (size_t i = 0; i < deps.count; ++i) {
        char* dep = deps.data[i];
        int is_absolute = (dep[0] == '/' || dep[0] == '\\' || (strlen(dep) >= 3 && dep[1] == ':' && dep[2] == '\\'));
        if (!is_absolute) {
            char* full = Path_join(baseDir, dep);
            if (printMessages && i < 5) {
                printf("  Converting relative dep '%s' to '%s'\n", dep, full);
            }
            free(dep);
            deps.data[i] = full;
        }
    }
    fclose(file);
    return deps;
}

int checkNeedsRebuild(TimePoint objTime, StringVector* sources, StringVector* dependencies) {
    for (size_t i = 0; i < sources->count; ++i) {
        if (!FileSystem_exists(sources->data[i])) {
            fprintf(stderr, "Error: Source file %s does not exist\n", sources->data[i]);
            return 1;
        }
        TimePoint srcTime = FileSystem_getModificationTime(sources->data[i]);
        if (printMessages) {
            printf("  Source %s modified (time %llu > %llu), needs rebuild\n", sources->data[i], (unsigned long long)srcTime, (unsigned long long)objTime);
        }
        if (srcTime > objTime) {
            return 1;
        }
    }
    for (size_t i = 0; i < dependencies->count; ++i) {
        if (!FileSystem_exists(dependencies->data[i])) {
            fprintf(stderr, "Error: Dependency file %s does not exist\n", dependencies->data[i]);
        } else {
            TimePoint depTime = FileSystem_getModificationTime(dependencies->data[i]);
            if (printMessages) {
                printf("  Dependency %s modified (time %llu > %llu), needs rebuild\n", dependencies->data[i], (unsigned long long)depTime, (unsigned long long)objTime);
            }
            if (depTime > objTime) {
                return 1;
            }
        }
    }
    return 0;
}

int needsRebuildTools(CompilationInfo* ci) {
    char objExt[5] = "o";
#ifdef _WIN32
    strcpy(objExt, "obj");
#endif
    char* toolsObj = Path_join(ci->intermediateDir, "Tools");
    toolsObj = (char*)realloc(toolsObj, strlen(toolsObj) + strlen(objExt) + 2);
    if (toolsObj) {
        strcat(toolsObj, ".");
        strcat(toolsObj, objExt);
    }
    TimePoint objTime = FileSystem_getModificationTime(toolsObj);
    if (objTime == 0) {
        if (printMessages) printf("  Tools obj file not found, needs rebuild\n");
        free(toolsObj);
        return 1; // Obj doesn't exist
    }

    StringVector sources;
    StringVector_init(&sources, 2);
    StringVector_add(&sources, ci->scCpp);
    StringVector_add(&sources, ci->toolsCpp);

    StringVector deps = parseDependencies(ci->toolsDepFile, ci->intermediateDir);

    int ret = checkNeedsRebuild(objTime, &sources, &deps);

    if (printMessages) {
        if (ret) {
            printf("  Tools obj needs rebuild\n");
        } else {
            printf("  Tools obj up to date\n");
        }
    }

    StringVector_destroy(&sources);
    StringVector_destroy(&deps);
    free(toolsObj);
    return ret;
}

int needsRebuildToolObj(CompilationInfo* ci) {
    char objExt[5] = "o";
#ifdef _WIN32
    strcpy(objExt, "obj");
#endif
    StringBuilder sb;
    sb = StringBuilder_init(256);
    StringBuilder_append(&sb, "SC-");
    StringBuilder_append(&sb, ci->args->toolName);
    StringBuilder_append(&sb, ".");
    StringBuilder_append(&sb, objExt);
    char* toolObj = Path_join(ci->intermediateDir, StringBuilder_get_buffer(&sb));
    StringBuilder_destroy(&sb);

    TimePoint objTime = FileSystem_getModificationTime(toolObj);
    if (objTime == 0) {
        if (printMessages) printf("  Tool obj file not found, needs rebuild\n");
        free(toolObj);
        return 1;
    }

    StringVector sources;
    StringVector_init(&sources, 2);
    StringVector_add(&sources, ci->toolCpp);
    if (ci->toolH) StringVector_add(&sources, ci->toolH);

    StringVector deps = parseDependencies(ci->toolDepFile, ci->intermediateDir);

    int ret = checkNeedsRebuild(objTime, &sources, &deps);

    if (printMessages) {
        if (ret) {
            printf("  Tool obj needs rebuild\n");
        } else {
            printf("  Tool obj up to date\n");
        }
    }

    StringVector_destroy(&sources);
    StringVector_destroy(&deps);
    free(toolObj);
    return ret;
}

int needsRebuildExe(CompilationInfo* ci) {
    TimePoint exeTime = FileSystem_getModificationTime(ci->toolExe);
    if (exeTime == 0) return 1;

    char objExt[5] = "o";
#ifdef _WIN32
    strcpy(objExt, "obj");
#endif

    char* toolsObj = Path_join(ci->intermediateDir, "Tools");
    toolsObj = (char*)realloc(toolsObj, strlen(toolsObj) + strlen(objExt) + 2);
    if (toolsObj) {
        strcat(toolsObj, ".");
        strcat(toolsObj, objExt);
    }

    char* toolObj = Path_join(ci->intermediateDir, "SC-");
    toolObj = (char*)realloc(toolObj, strlen(toolObj) + strlen(ci->args->toolName) + strlen(".") + strlen(objExt) + 1);
    sprintf(toolObj + strlen(toolObj), "%s.%s", ci->args->toolName, objExt);

    TimePoint toolsObjTime = FileSystem_getModificationTime(toolsObj);
    TimePoint toolObjTime = FileSystem_getModificationTime(toolObj);

    free(toolsObj);
    free(toolObj);

    return (toolsObjTime > exeTime || toolObjTime > exeTime);
}

// Helper functions for building compile commands
void buildCompileCommandPOSIX(CommandLine* cmd, const char* compiler, const char* output, const char* input, int useClang) {
    CommandLine_init(cmd, compiler);
    CommandLine_arg(cmd, "-I../../..");
    CommandLine_arg(cmd, "-std=c++14");
    CommandLine_arg(cmd, "-pthread");
    CommandLine_arg(cmd, "-MMD");
    CommandLine_arg(cmd, "-fstrict-aliasing");
    CommandLine_arg(cmd, "-fvisibility=hidden");
    CommandLine_arg(cmd, "-fvisibility-inlines-hidden");
    CommandLine_arg(cmd, "-fno-rtti");
    CommandLine_arg(cmd, "-fno-exceptions");
    CommandLine_arg(cmd, "-D_DEBUG=1");
    CommandLine_arg(cmd, "-g");
    CommandLine_arg(cmd, "-ggdb");
    CommandLine_arg(cmd, "-O0");
    if (useClang) {
        CommandLine_arg(cmd, "-nostdinc++");
    }
    CommandLine_arg(cmd, "-o");
    CommandLine_argQuoted(cmd, output);
    CommandLine_arg(cmd, "-c");
    CommandLine_argQuoted(cmd, input);
}

void buildCompileCommandWindows(CommandLine* cmd, const char* intermediateDir, const char* output, const char* input, const char* jsonFile, const char* pdbName) {
    CommandLine_init(cmd, "cl.exe");
    CommandLine_arg(cmd, "/nologo");
    CommandLine_arg(cmd, "/I.");
    CommandLine_arg(cmd, "/std:c++14");
    CommandLine_arg(cmd, "/D_DEBUG");
    CommandLine_arg(cmd, "/Zi");
    CommandLine_arg(cmd, "/MTd");
    CommandLine_arg(cmd, "/GS");
    CommandLine_arg(cmd, "/Od");
    CommandLine_arg(cmd, "/permissive-");
    CommandLine_arg(cmd, "/EHsc");
    CommandLine_arg(cmd, "/sourceDependencies");
    CommandLine_argQuoted(cmd, jsonFile);
    CommandLine_arg(cmd, "/c");
    StringBuilder sb1 = StringBuilder_init(256);
    StringBuilder_append(&sb1, "/Fd\"");
    StringBuilder_append(&sb1, intermediateDir);
    StringBuilder_append(&sb1, "/");
    StringBuilder_append(&sb1, pdbName);
    StringBuilder_append(&sb1, ".pdb\"");
    CommandLine_arg(cmd, StringBuilder_get_buffer(&sb1));
    StringBuilder_destroy(&sb1);
    StringBuilder sb2 = StringBuilder_init(256);
    StringBuilder_append(&sb2, "/Fo\"");
    StringBuilder_append(&sb2, output);
    StringBuilder_append(&sb2, "\"");
    CommandLine_arg(cmd, StringBuilder_get_buffer(&sb2));
    StringBuilder_destroy(&sb2);
    CommandLine_argQuoted(cmd, input);
}

#ifndef _WIN32
int compilePOSIX(CompilationInfo* ci) {
    // Simplified: assume clang++ or g++
    FileSystem_createDirectoryRecursive(ci->intermediateDir);
    FileSystem_createDirectoryRecursive(ci->toolOutputDir); // Wait, already there

    char* compiler = "";
    int useClang = 0;
    if (runCommand("clang++ --version > /dev/null 2>&1") == 0) {
        compiler = "clang++";
        useClang = 1;
    } else if (runCommand("g++ --version > /dev/null 2>&1") == 0) {
        compiler = "g++";
    } else {
        return 1;
    }

    int needTools = needsRebuildTools(ci);
    int needTool = needsRebuildToolObj(ci);

    if (needTools && needTool) {
        // Compile both in parallel
        pid_t pid1 = fork();
        if (pid1 == 0) {
            // Compile Tools.cpp
            char* toolsObj = Path_join(ci->intermediateDir, "Tools.o");
            CommandLine cmd;
            buildCompileCommandPOSIX(&cmd, compiler, toolsObj, ci->toolsCpp, useClang);
            int ret = CommandLine_run(&cmd);
            CommandLine_destroy(&cmd);
            free(toolsObj);
            exit(ret);
        }
        pid_t pid2 = fork();
        if (pid2 == 0) {
            // Compile SC-tool.cpp
            char* toolObj = Path_join(ci->intermediateDir, "SC-");
            toolObj = (char*)realloc(toolObj, strlen(toolObj) + strlen(ci->args->toolName) + strlen(".o") + 1);
            sprintf(toolObj + strlen(toolObj), "%s.o", ci->args->toolName);
            CommandLine cmd;
            buildCompileCommandPOSIX(&cmd, compiler, toolObj, ci->toolCpp, useClang);
            int ret = CommandLine_run(&cmd);
            CommandLine_destroy(&cmd);
            free(toolObj);
            exit(ret);
        }
        int status1, status2;
        waitpid(pid1, &status1, 0);
        waitpid(pid2, &status2, 0);
        if (WEXITSTATUS(status1) != 0 || WEXITSTATUS(status2) != 0) return 1;
        printf("Tools.cpp\n");
        printf("SC-%s.cpp\n", ci->args->toolName);
    } else if (needTools) {
        char* toolsObj = Path_join(ci->intermediateDir, "Tools.o");
        CommandLine cmd;
        buildCompileCommandPOSIX(&cmd, compiler, toolsObj, ci->toolsCpp, useClang);
        printf("Tools.cpp\n");
        int ret = CommandLine_run(&cmd);
        CommandLine_destroy(&cmd);
        free(toolsObj);
        if (ret != 0) return 1;
    } else if (needTool) {
        char* toolObj = Path_join(ci->intermediateDir, "SC-");
        toolObj = (char*)realloc(toolObj, strlen(toolObj) + strlen(ci->args->toolName) + strlen(".o") + 1);
        sprintf(toolObj + strlen(toolObj), "%s.o", ci->args->toolName);
        CommandLine cmd;
        buildCompileCommandPOSIX(&cmd, compiler, toolObj, ci->toolCpp, useClang);
        printf("SC-%s.cpp\n", ci->args->toolName);
        int ret = CommandLine_run(&cmd);
        CommandLine_destroy(&cmd);
        free(toolObj);
        if (ret != 0) return 1;
    } else {
        printf("\"%s\" is up to date\n", ci->toolsCpp);
        printf("\"%s\" is up to date\n", ci->toolCpp);
    }

    if (needsRebuildExe(ci)) {
        char* toolsObj = Path_join(ci->intermediateDir, "Tools.o");
        char* toolObj = Path_join(ci->intermediateDir, "SC-");
        toolObj = (char*)realloc(toolObj, strlen(toolObj) + strlen(ci->args->toolName) + strlen(".o") + 1);
        sprintf(toolObj + strlen(toolObj), "%s.o", ci->args->toolName);
        CommandLine cmd;
        CommandLine_init(&cmd, compiler);
        CommandLine_arg(&cmd, "-o");
        CommandLine_argQuoted(&cmd, ci->toolExe);
        CommandLine_argQuoted(&cmd, toolsObj);
        CommandLine_argQuoted(&cmd, toolObj);
        if (strcmp(ci->targetOS, "Linux") == 0) {
            CommandLine_arg(&cmd, "-rdynamic");
        }
        CommandLine_arg(&cmd, "-ldl");
        CommandLine_arg(&cmd, "-lpthread");
        if (useClang) {
            CommandLine_arg(&cmd, "-nostdlib++");
        }
        if (strcmp(ci->targetOS, "Darwin") == 0) {
            CommandLine_arg(&cmd, "-framework");
            CommandLine_arg(&cmd, "CoreFoundation");
            CommandLine_arg(&cmd, "-framework");
            CommandLine_arg(&cmd, "CoreServices");
        }
        int ret = CommandLine_run(&cmd);
        CommandLine_destroy(&cmd);
        free(toolsObj);
        free(toolObj);
        return ret;
    }
    return 0;
}
#endif

int linkWindows(CompilationInfo* ci) {
    printf("Linking %s\n", ci->args->toolName);
    char* exeDir = Path_join(ci->toolOutputDir, ci->targetOS);
    FileSystem_createDirectoryRecursive(exeDir);
    free(exeDir);

    char* toolsObj = Path_join(ci->intermediateDir, "Tools.obj");
    char* toolObj = Path_join(ci->intermediateDir, "SC-");
    toolObj = (char*)realloc(toolObj, strlen(toolObj) + strlen(ci->args->toolName) + strlen(".obj") + 1);
    sprintf(toolObj + strlen(toolObj), "%s.obj", ci->args->toolName);

    CommandLine cmd;
    CommandLine_init(&cmd, "link");
    CommandLine_arg(&cmd, "/nologo");
    CommandLine_arg(&cmd, "/DEBUG");
    StringBuilder sb = StringBuilder_init(256);
    StringBuilder_append(&sb, "/OUT:\"");
    StringBuilder_append(&sb, ci->toolExe);
    StringBuilder_append(&sb, "\"");
    StringBuilder_append(&sb, " /PDB:\"");
    StringBuilder_append(&sb, ci->toolOutputDir);
    StringBuilder_append(&sb, "/");
    StringBuilder_append(&sb, ci->targetOS);
    StringBuilder_append(&sb, "/SC-");
    StringBuilder_append(&sb, ci->args->toolName);
    StringBuilder_append(&sb,".pdb\"");
    CommandLine_arg(&cmd, StringBuilder_get_buffer(&sb));
    StringBuilder_destroy(&sb);
    CommandLine_argQuoted(&cmd, toolsObj);
    CommandLine_argQuoted(&cmd, toolObj);
    CommandLine_arg(&cmd, "Advapi32.lib");
    CommandLine_arg(&cmd, "Shell32.lib");
    int ret = CommandLine_run(&cmd);
    CommandLine_destroy(&cmd);
    free(toolsObj);
    free(toolObj);
    return ret;
}

int compileWindows(CompilationInfo* ci, int* objsCompiled);

int executeTool(BootloaderArgs* args, CompilationInfo* ci) {
    CommandLine cmd;
    CommandLine_init(&cmd, ci->toolExe);
    CommandLine_argQuoted(&cmd, args->libraryDir);
    CommandLine_argQuoted(&cmd, args->toolSourceDir);
    CommandLine_argQuoted(&cmd, args->buildDir);
    CommandLine_argQuoted(&cmd, args->toolName);
    for (int i = 0; i < args->numRemainingArgs; ++i) {
        CommandLine_argQuoted(&cmd, args->remainingArgs[i]);
    }
    int ret = CommandLine_run(&cmd);
    CommandLine_destroy(&cmd);
    return ret;
}

// Main
int main(int argc, char* argv[]) {
    int new_argc;
    char** new_argv = getArgv(&new_argc, argv, argc);
    if (!new_argv) {
        fprintf(stderr, "Failed to get arguments\n");
        return 1;
    }

    BootloaderArgs args = parseArgs((const char**)new_argv, new_argc);
    CompilationInfo ci = CompilationInfo_init(&args);
    setupCompilation(&ci);

    int needsCompile = needsRebuildTools(&ci) || needsRebuildToolObj(&ci);
    int needsLink = needsRebuildExe(&ci);

#ifdef _WIN32
    clock_t start_time = clock();
#else
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
#endif
    if (needsCompile || needsLink) {
        printf("Rebuilding %s tool...\n", args.toolName);
#ifdef _WIN32
        int objsCompiled = 0;
        if (!compileWindows(&ci, &objsCompiled)) {
            fprintf(stderr, "Compilation failed\n");
            CompilationInfo_destroy(&ci);
            BootloaderArgs_destroy(&args);
            freeArgs(new_argc, new_argv);
            return 1;
        }
        if (objsCompiled || needsLink) {
            if (linkWindows(&ci) != 0) {
                fprintf(stderr, "Linking failed\n");
                CompilationInfo_destroy(&ci);
                BootloaderArgs_destroy(&args);
                freeArgs(new_argc, new_argv);
                return 1;
            }
        }
#else
        if (compilePOSIX(&ci) != 0) {
            fprintf(stderr, "Compilation failed\n");
            CompilationInfo_destroy(&ci);
            BootloaderArgs_destroy(&args);
            freeArgs(new_argc, new_argv);
            return 1;
        }
#endif
#ifdef _WIN32
        clock_t end_time = clock();
        double duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
#else
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        double duration = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
#endif
        printf("Time to compile \"%s\" tool: %.2f seconds\n", args.toolName, duration);
    } else {
        printf("\"%s\" is up to date\n", ci.toolCpp);
    }

    int ret = executeTool(&args, &ci);
    CompilationInfo_destroy(&ci);
    BootloaderArgs_destroy(&args);
    freeArgs(new_argc, new_argv);
    return ret;
}

int compileWindows(CompilationInfo* ci, int* objsCompiled) {
    // Simplified Windows compilation
FileSystem_createDirectoryRecursive(ci->intermediateDir);
    FileSystem_createDirectoryRecursive(ci->toolOutputDir);

    int needTools = needsRebuildTools(ci);
    int needTool = needsRebuildToolObj(ci);

    if (needTools && needTool) {
        // Compile both in parallel
#ifdef _WIN32
        HANDLE processes[2];
        int count = 0;
#endif
        // Spawn Tools.obj compilation
        {
            *objsCompiled = 1;
            char* toolsObj = Path_join(ci->intermediateDir, "Tools.obj");
            char* toolsJson = Path_join(ci->intermediateDir, "Tools.json");
            CommandLine cmd;
            buildCompileCommandWindows(&cmd, ci->intermediateDir, toolsObj, ci->toolsCpp, toolsJson, "Tools");
#ifdef _WIN32
            char* command = StringVector_join(&cmd.args, " ");
            STARTUPINFOW si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            size_t len = strlen(command);
            int wlen = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
            if (wlen > 0) {
                wchar_t* wcmd = (wchar_t*)malloc(wlen * sizeof(wchar_t));
                if (wcmd) {
                    MultiByteToWideChar(CP_UTF8, 0, command, -1, wcmd, wlen);
                    if (CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        processes[count++] = pi.hProcess;
                        CloseHandle(pi.hThread);
                    }
                    free(wcmd);
                }
            }
            free(command);
#else
            int ret = CommandLine_run(&cmd);
            if (ret != 0) return 0;
#endif
            CommandLine_destroy(&cmd);
            free(toolsObj);
            free(toolsJson);
        }
        // Spawn SC-tool.obj compilation
        {
            char* toolObj = Path_join(ci->intermediateDir, "SC-");
            toolObj = (char*)realloc(toolObj, strlen(toolObj) + strlen(ci->args->toolName) + strlen(".obj") + 1);
            sprintf(toolObj + strlen(toolObj), "%s.obj", ci->args->toolName);
            char* toolJson = Path_join(ci->intermediateDir, "SC-");
            toolJson = (char*)realloc(toolJson, strlen(toolJson) + strlen(ci->args->toolName) + strlen(".json") + 1);
            sprintf(toolJson + strlen(toolJson), "%s.json", ci->args->toolName);
            CommandLine cmd;
            buildCompileCommandWindows(&cmd, ci->intermediateDir, toolObj, ci->toolCpp, toolJson, ci->args->toolName);
#ifdef _WIN32
            char* command = StringVector_join(&cmd.args, " ");
            STARTUPINFOW si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            size_t len = strlen(command);
            int wlen = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
            if (wlen > 0) {
                wchar_t* wcmd = (wchar_t*)malloc(wlen * sizeof(wchar_t));
                if (wcmd) {
                    MultiByteToWideChar(CP_UTF8, 0, command, -1, wcmd, wlen);
                    if (CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        processes[count++] = pi.hProcess;
                        CloseHandle(pi.hThread);
                    }
                    free(wcmd);
                }
            }
            free(command);
#else
            int ret = CommandLine_run(&cmd);
            if (ret != 0) return 0;
#endif
            CommandLine_destroy(&cmd);
            free(toolObj);
            free(toolJson);
        }
#ifdef _WIN32
        // Wait for both
        if (count == 2) {
            WaitForMultipleObjects(2, processes, TRUE, INFINITE);
            DWORD exit1, exit2;
            GetExitCodeProcess(processes[0], &exit1);
            GetExitCodeProcess(processes[1], &exit2);
            CloseHandle(processes[0]);
            CloseHandle(processes[1]);
            if (exit1 != 0 || exit2 != 0) return 0;
        } else {
            return 0;
        }
#else
        printf("Tools.cpp\n");
        printf("SC-%s.cpp\n", ci->args->toolName);
#endif
    } else if (needTools) {
        *objsCompiled = 1;
        char* toolsObj = Path_join(ci->intermediateDir, "Tools.obj");
        char* toolsJson = Path_join(ci->intermediateDir, "Tools.json");
        CommandLine cmd;
        buildCompileCommandWindows(&cmd, ci->intermediateDir, toolsObj, ci->toolsCpp, toolsJson, "Tools");
        int ret = CommandLine_run(&cmd);
        CommandLine_destroy(&cmd);
        free(toolsObj);
        free(toolsJson);
        if (ret != 0) return 0;
    } else if (needTool) {
        *objsCompiled = 1;
        char* toolObj = Path_join(ci->intermediateDir, "SC-");
        toolObj = (char*)realloc(toolObj, strlen(toolObj) + strlen(ci->args->toolName) + strlen(".obj") + 1);
        sprintf(toolObj + strlen(toolObj), "%s.obj", ci->args->toolName);
        char* toolJson = Path_join(ci->intermediateDir, "SC-");
        toolJson = (char*)realloc(toolJson, strlen(toolJson) + strlen(ci->args->toolName) + strlen(".json") + 1);
        sprintf(toolJson + strlen(toolJson), "%s.json", ci->args->toolName);
        CommandLine cmd;
        buildCompileCommandWindows(&cmd, ci->intermediateDir, toolObj, ci->toolCpp, toolJson, ci->args->toolName);
        int ret = CommandLine_run(&cmd);
        CommandLine_destroy(&cmd);
        free(toolObj);
        free(toolJson);
        if (ret != 0) return 0;
    } else {
        printf("\"%s\" is up to date\n", ci->toolsCpp);
        printf("\"%s\" is up to date\n", ci->toolCpp);
    }
    return 1;
}
