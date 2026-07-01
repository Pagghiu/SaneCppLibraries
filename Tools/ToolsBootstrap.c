// ToolsBootstrap.c - Single-file C bootstrap for build system
// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#if defined(__linux__)
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#endif

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

char* Path_join(const char* a, const char* b);
int FileSystem_createDirectoryRecursive(const char* path);

#ifdef _WIN32
enum {
    SC_WINDOWS_LOGICAL_PATH_CAPACITY   = 1024,
    SC_WINDOWS_TRANSPORT_PATH_CAPACITY = SC_WINDOWS_LOGICAL_PATH_CAPACITY + 6,
    SC_WINDOWS_BOOTSTRAP_ALIAS_LIMIT   = 240
};

static int WindowsPath_isDriveLetter(wchar_t character) {
    return (character >= L'a' && character <= L'z') || (character >= L'A' && character <= L'Z');
}

static int WindowsPath_hasUNCServerAndShare(const wchar_t* data, int length) {
    int firstSeparator = 0;
    while (firstSeparator < length && data[firstSeparator] != L'\\') firstSeparator += 1;
    if (firstSeparator == 0 || firstSeparator >= length - 1) return 0;
    int secondSeparator = firstSeparator + 1;
    while (secondSeparator < length && data[secondSeparator] != L'\\') secondSeparator += 1;
    return secondSeparator > firstSeparator + 1;
}

static int WindowsPath_isUNC(const wchar_t* data, int length) {
    return length >= 5 && data[0] == L'\\' && data[1] == L'\\' &&
           WindowsPath_hasUNCServerAndShare(data + 2, length - 2);
}

static int WindowsPath_isDriveAbsolute(const wchar_t* data, int length) {
    return length >= 3 && WindowsPath_isDriveLetter(data[0]) && data[1] == L':' && data[2] == L'\\';
}

static int WindowsPath_stripTransportPrefix(wchar_t* logicalPath, int* logicalLength) {
    int length = *logicalLength;
    int hasWin32Prefix =
        length >= 4 && logicalPath[0] == L'\\' && logicalPath[1] == L'\\' && logicalPath[2] == L'?' &&
        logicalPath[3] == L'\\';
    int hasNtPrefix =
        length >= 4 && logicalPath[0] == L'\\' && logicalPath[1] == L'?' && logicalPath[2] == L'?' &&
        logicalPath[3] == L'\\';
    if (hasWin32Prefix || hasNtPrefix) {
        if (length >= 8 && (logicalPath[4] == L'U' || logicalPath[4] == L'u') &&
            (logicalPath[5] == L'N' || logicalPath[5] == L'n') &&
            (logicalPath[6] == L'C' || logicalPath[6] == L'c') && logicalPath[7] == L'\\') {
            if (!WindowsPath_hasUNCServerAndShare(logicalPath + 8, length - 8)) return 0;
            memmove(logicalPath + 2, logicalPath + 8, (size_t)(length - 8 + 1) * sizeof(wchar_t));
            logicalPath[0] = L'\\';
            logicalPath[1] = L'\\';
            *logicalLength = length - 6;
            return 1;
        }
        if (length >= 7 && WindowsPath_isDriveLetter(logicalPath[4]) && logicalPath[5] == L':' &&
            logicalPath[6] == L'\\') {
            memmove(logicalPath, logicalPath + 4, (size_t)(length - 4 + 1) * sizeof(wchar_t));
            *logicalLength = length - 4;
            return 1;
        }
        return 0;
    }

    if (length >= 3 && logicalPath[0] == L'\\' && logicalPath[1] == L'\\' && logicalPath[2] == L'?') return 0;
    if (length >= 3 && logicalPath[0] == L'\\' && logicalPath[1] == L'?' && logicalPath[2] == L'?') return 0;
    return 1;
}

static int WindowsPath_prepareTransportPath(const char* utf8Path, wchar_t* transportPath, size_t transportCapacity) {
    wchar_t logicalPath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    int convertedLength =
        MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, logicalPath, (int)(SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1));
    if (convertedLength <= 0) {
        return 0;
    }
    int logicalLength = convertedLength - 1;
    for (int idx = 0; idx < logicalLength; ++idx) {
        if (logicalPath[idx] == L'/') {
            logicalPath[idx] = L'\\';
        }
    }

    if (!WindowsPath_stripTransportPrefix(logicalPath, &logicalLength)) {
        return 0;
    }

    if (!(WindowsPath_isUNC(logicalPath, logicalLength) ||
          WindowsPath_isDriveAbsolute(logicalPath, logicalLength))) {
        wchar_t absolutePath[SC_WINDOWS_LOGICAL_PATH_CAPACITY + 1];
        DWORD absoluteLength =
            GetFullPathNameW(logicalPath, (DWORD)(SC_WINDOWS_LOGICAL_PATH_CAPACITY + 1), absolutePath, NULL);
        if (absoluteLength == 0 || absoluteLength > SC_WINDOWS_LOGICAL_PATH_CAPACITY) {
            return 0;
        }
        memcpy(logicalPath, absolutePath, ((size_t)absoluteLength + 1) * sizeof(wchar_t));
        logicalLength = (int)absoluteLength;
    } else if (logicalLength > SC_WINDOWS_LOGICAL_PATH_CAPACITY) {
        return 0;
    }

    if (logicalLength >= 2 && logicalPath[0] == L'\\' && logicalPath[1] == L'\\') {
        int written = _snwprintf(transportPath, transportCapacity, L"\\\\?\\UNC\\%ls", logicalPath + 2);
        return written > 0 && (size_t)written < transportCapacity;
    }
    int written = _snwprintf(transportPath, transportCapacity, L"\\\\?\\%ls", logicalPath);
    return written > 0 && (size_t)written < transportCapacity;
}

static int WindowsPath_createDirectoryAlias(const char* aliasPathUtf8, const char* targetPathUtf8) {
    wchar_t aliasPath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    wchar_t targetPath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    if (!WindowsPath_prepareTransportPath(aliasPathUtf8, aliasPath, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1) ||
        !WindowsPath_prepareTransportPath(targetPathUtf8, targetPath, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1)) {
        return 0;
    }

    DWORD attributes = GetFileAttributesW(aliasPath);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return 1;
    }

    DWORD flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
    return CreateSymbolicLinkW(aliasPath, targetPath, flags) != 0;
}

static int WindowsPath_isSeparator(char character) {
    return character == '\\' || character == '/';
}

static int WindowsPath_isUnderDirectory(const char* path, const char* directory, const char** relativePath) {
    size_t directoryLength = strlen(directory);
    while (directoryLength > 0 && WindowsPath_isSeparator(directory[directoryLength - 1])) {
        directoryLength -= 1;
    }
    if (directoryLength == 0 || _strnicmp(path, directory, directoryLength) != 0) {
        return 0;
    }
    if (path[directoryLength] == '\0') {
        *relativePath = path + directoryLength;
        return 1;
    }
    if (!WindowsPath_isSeparator(path[directoryLength])) {
        return 0;
    }
    *relativePath = path + directoryLength + 1;
    return 1;
}

static char* WindowsPath_createProjectRootSourceAlias(const char* toolOutputDir, const char* projectDir,
                                                      const char* sourcePath) {
    const char* relativePath = NULL;
    if (!WindowsPath_isUnderDirectory(sourcePath, projectDir, &relativePath) || relativePath[0] == '\0') {
        return NULL;
    }
    FileSystem_createDirectoryRecursive(toolOutputDir);

    char* aliasRoot = Path_join(toolOutputDir, "_ProjectRoot");
    if (!aliasRoot || !WindowsPath_createDirectoryAlias(aliasRoot, projectDir)) {
        free(aliasRoot);
        return NULL;
    }

    char* aliasToolCpp = Path_join(aliasRoot, relativePath);
    free(aliasRoot);
    return aliasToolCpp;
}

static int WindowsPath_replaceWithProjectRootSourceAlias(char** sourcePath, const char* toolOutputDir,
                                                         const char* projectDir) {
    char* aliasPath = WindowsPath_createProjectRootSourceAlias(toolOutputDir, projectDir, *sourcePath);
    if (!aliasPath) {
        return 0;
    }
    if (strlen(aliasPath) >= MAX_PATH) {
        free(aliasPath);
        return -1;
    }
    free(*sourcePath);
    *sourcePath = aliasPath;
    return 1;
}

static void WindowsPath_applyProjectRootSourceAliasIfNeeded(char** sourcePath, const char* toolOutputDir,
                                                            const char* projectDir, const char* sourceDescription) {
    if (!projectDir || strlen(*sourcePath) < SC_WINDOWS_BOOTSTRAP_ALIAS_LIMIT) {
        return;
    }

    const int aliasResult = WindowsPath_replaceWithProjectRootSourceAlias(sourcePath, toolOutputDir, projectDir);
    if (aliasResult == 0) {
        fprintf(stderr, "Error: Failed creating long-path %s source alias\n", sourceDescription);
        exit(1);
    }
    if (aliasResult < 0) {
        fprintf(stderr,
                "Error: Long-path %s source has a project-relative path too long for bootstrap compilation\n",
                sourceDescription);
        exit(1);
    }
}

static const char* windowsCommandWorkingDirectory = NULL;
#endif
#include <stdint.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#ifndef _WIN32
#include <sys/time.h>
#endif

int printMessages = 0;

int bootstrapIncludeStdCpp(void) {
    const char* value = getenv("SC_BOOTSTRAP_INCLUDE_STD_CPP");
    return value == NULL || strcmp(value, "0") != 0;
}

int bootstrapLinkStdCpp(void) {
    const char* value = getenv("SC_BOOTSTRAP_LINK_STD_CPP");
    return value != NULL && strcmp(value, "0") != 0 && value[0] != '\0';
}

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

typedef struct {
#ifdef _WIN32
    clock_t startTime;
#else
    struct timeval startTime;
#endif
} BootstrapTimer;

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
    char* projectDir;
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
char* FileSystem_canonicalizePath(const char* path);

// String/vector utilities
void StringVector_init(StringVector* sv, size_t initial_capacity);
void StringVector_destroy(StringVector* sv);
void StringVector_add(StringVector* sv, const char* str);
char* StringVector_join(const StringVector* sv, const char* sep);
int StringVector_contains(const StringVector* sv, const char* str);
int StringVector_containsPath(const StringVector* sv, const char* str);
char* StringBuilder_join(const StringBuilder* sb, const char* sep);

char* String_duplicate(const char* str);
char* String_concat3(const char* a, const char* b, const char* c);
int String_containsPathSeparator(const char* str);
const char* Path_fileName(const char* path);
char* deriveToolIdentity(const char* toolPath);
int isSCBuildDefinitionSource(const char* toolPath);
const char* objectExtension(void);
const char* dependencyExtension(const CompilationInfo* ci);
char* Path_joinWithExtension(const char* directory, const char* stem, const char* extension);
char* buildToolStem(const char* toolName);
char* buildToolsObjectPath(const CompilationInfo* ci);
char* buildToolObjectPath(const CompilationInfo* ci);
char* buildToolsDependencyPath(const CompilationInfo* ci);
char* buildToolDependencyPath(const CompilationInfo* ci);
#ifdef _WIN32
unsigned long long String_hashFnv1a(const char* str);
unsigned long long String_hashCombine(unsigned long long hash, const char* str);
char* buildWindowsToolOutputDir(BootloaderArgs* args, const char* toolCpp);
#endif

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

#ifdef _WIN32
unsigned long long String_hashFnv1a(const char* str) {
    unsigned long long hash = 1469598103934665603ULL;
    while (str && *str) {
        hash ^= (unsigned char)*str++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

unsigned long long String_hashCombine(unsigned long long hash, const char* str) {
    while (str && *str) {
        hash ^= (unsigned char)*str++;
        hash *= 1099511628211ULL;
    }
    hash ^= 0xFFU;
    hash *= 1099511628211ULL;
    return hash;
}

char* buildWindowsToolOutputDir(BootloaderArgs* args, const char* toolCpp) {
    const char* cacheBase = getenv("SC_BUILD_TOOL_CACHE_DIR");
    if (!cacheBase || !cacheBase[0]) cacheBase = getenv("LOCALAPPDATA");
    if (!cacheBase || !cacheBase[0]) cacheBase = getenv("TEMP");
    if (!cacheBase || !cacheBase[0]) {
        return Path_join(args->buildDir, "_Tools");
    }

    unsigned long long hash = String_hashFnv1a(args->projectDir ? args->projectDir : args->libraryDir);
    hash = String_hashCombine(hash, args->buildDir);
    hash = String_hashCombine(hash, args->toolName);
    hash = String_hashCombine(hash, toolCpp);

    char hashDirectory[32];
    snprintf(hashDirectory, sizeof(hashDirectory), "%016llx", hash);

    char* scBuildDir = Path_join(cacheBase, "SC-build");
    char* toolCache  = Path_join(scBuildDir, "ToolCache");
    char* outputDir  = Path_join(toolCache, hashDirectory);
    free(scBuildDir);
    free(toolCache);
    return outputDir;
}
#endif

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

int StringVector_contains(const StringVector* sv, const char* str) {
    for (size_t i = 0; i < sv->count; ++i) {
        if (strcmp(sv->data[i], str) == 0) {
            return 1;
        }
    }
    return 0;
}

char* FileSystem_canonicalizePath(const char* path) {
    if (!path) return NULL;
#ifdef _WIN32
    DWORD required = GetFullPathNameA(path, 0, NULL, NULL);
    if (required == 0) return String_duplicate(path);
    char* buffer = (char*)malloc(required);
    if (!buffer) return NULL;
    DWORD written = GetFullPathNameA(path, required, buffer, NULL);
    if (written == 0 || written >= required) {
        free(buffer);
        return String_duplicate(path);
    }
    return buffer;
#else
    char* resolved = realpath(path, NULL);
    if (resolved) return resolved;
    return String_duplicate(path);
#endif
}

int StringVector_containsPath(const StringVector* sv, const char* str) {
    char* canonicalTarget = FileSystem_canonicalizePath(str);
    if (!canonicalTarget) return 0;
    for (size_t i = 0; i < sv->count; ++i) {
        char* canonicalDep = FileSystem_canonicalizePath(sv->data[i]);
        if (!canonicalDep) continue;
#ifdef _WIN32
        const int matches = _stricmp(canonicalDep, canonicalTarget) == 0;
#else
        const int matches = strcmp(canonicalDep, canonicalTarget) == 0;
#endif
        free(canonicalDep);
        if (matches) {
            free(canonicalTarget);
            return 1;
        }
    }
    free(canonicalTarget);
    return 0;
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

char* String_duplicate(const char* str) {
    if (!str) return NULL;
    char* result = (char*)malloc(strlen(str) + 1);
    if (result) strcpy(result, str);
    return result;
}

char* String_concat3(const char* a, const char* b, const char* c) {
    size_t lenA = strlen(a);
    size_t lenB = strlen(b);
    size_t lenC = strlen(c);
    char* result = (char*)malloc(lenA + lenB + lenC + 1);
    if (!result) return NULL;
    memcpy(result, a, lenA);
    memcpy(result + lenA, b, lenB);
    memcpy(result + lenA + lenB, c, lenC);
    result[lenA + lenB + lenC] = 0;
    return result;
}

char* detectTargetOS(void) {
#ifdef _WIN32
    return String_duplicate("Windows");
#else
    FILE* uname_file = POPEN("uname", "r");
    if (uname_file) {
        char buf[256];
        if (fgets(buf, sizeof(buf), uname_file)) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = 0;
            PCLOSE(uname_file);
            return String_duplicate(buf);
        }
        PCLOSE(uname_file);
    }
    return String_duplicate("POSIX");
#endif
}

void String_unescapeJsonInPlace(char* str) {
    if (!str) return;
    char* read  = str;
    char* write = str;
    while (*read) {
        if (*read == '\\' && read[1] != '\0') {
            ++read;
        }
        *write++ = *read++;
    }
    *write = '\0';
}

int String_containsPathSeparator(const char* str) {
    if (!str) return 0;
    while (*str) {
        if (*str == '/' || *str == '\\') return 1;
        str++;
    }
    return 0;
}

const char* Path_fileName(const char* path) {
    const char* lastSlash = strrchr(path, '/');
    const char* lastBackslash = strrchr(path, '\\');
    if (!lastSlash) return lastBackslash ? lastBackslash + 1 : path;
    if (!lastBackslash) return lastSlash + 1;
    return (lastSlash > lastBackslash ? lastSlash : lastBackslash) + 1;
}

char* deriveToolIdentity(const char* toolPath) {
    const char* fileName = Path_fileName(toolPath);
    size_t len = strlen(fileName);
    if (len >= 4 && strcmp(fileName + len - 4, ".cpp") == 0) {
        len -= 4;
    }
    size_t start = 0;
    if (len >= 3 && fileName[0] == 'S' && fileName[1] == 'C' && fileName[2] == '-') {
        start = 3;
        len -= 3;
    }
    if (len == 0) {
        return String_duplicate("tool");
    }
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, fileName + start, len);
    result[len] = 0;
    return result;
}

int isSCBuildDefinitionSource(const char* toolPath) {
    const char* fileName = Path_fileName(toolPath);
    return strcmp(fileName, "SC-build.cpp") == 0;
}

char* buildBuiltInToolSourcePath(const BootloaderArgs* args) {
    char* toolCpp = Path_join(args->toolSourceDir, "SC-");
    toolCpp = (char*)realloc(toolCpp, strlen(toolCpp) + strlen(args->toolName) + strlen(".cpp") + 1);
    if (toolCpp) {
        strcat(toolCpp, args->toolName);
        strcat(toolCpp, ".cpp");
    }
    return toolCpp;
}

char* resolveToolSource(BootloaderArgs* args, int* builtInTool) {
    *builtInTool = 1;

    char* toolCpp = buildBuiltInToolSourcePath(args);
    if (FileSystem_exists(toolCpp)) {
        return toolCpp;
    }

    char* potentialCpp = String_duplicate(args->toolName);
    if (FileSystem_exists(potentialCpp)) {
        free(toolCpp);
        *builtInTool = 0;

        char* toolIdentity = deriveToolIdentity(potentialCpp);
        if (!toolIdentity) {
            fprintf(stderr, "Error: Failed deriving tool name from \"%s\"\n", potentialCpp);
            free(potentialCpp);
            exit(1);
        }
        free(args->toolName);
        args->toolName = toolIdentity;
        return potentialCpp;
    }

    free(potentialCpp);
    fprintf(stderr, "Error: Tool \"%s\" doesn't exist\n", args->toolName);
    free(toolCpp);
    exit(1);
}

char* buildBuiltInToolHeaderPath(const BootloaderArgs* args) {
    char* toolH = Path_join(args->toolSourceDir, "SC-");
    toolH = (char*)realloc(toolH, strlen(toolH) + strlen(args->toolName) + strlen(".h") + 1);
    if (toolH) {
        strcat(toolH, args->toolName);
        strcat(toolH, ".h");
    }
    return toolH;
}

char* buildToolExecutablePath(const CompilationInfo* ci) {
    char* exeDir = Path_join(ci->toolOutputDir, ci->targetOS ? ci->targetOS : "Unknown");
    char* exeName = "SC-";
    exeName = (char*)malloc(strlen(exeName) + strlen(ci->args->toolName) + 1);
    if (exeName) {
        strcpy(exeName, "SC-");
        strcat(exeName, ci->args->toolName);
        char* exeExt = "";
#ifdef _WIN32
        exeExt = ".exe";
#endif
        exeName = (char*)realloc(exeName, strlen(exeName) + strlen(exeExt) + 1);
        if (exeName) strcat(exeName, exeExt);
    }

    char* toolExe = exeName ? Path_join(exeDir, exeName) : NULL;
    free(exeName);
    free(exeDir);
    return toolExe;
}

char* buildToolOutputDirectory(BootloaderArgs* args, const char* toolCpp, const char* targetOS) {
#ifdef _WIN32
    if (targetOS && strcmp(targetOS, "Windows") == 0) {
        return buildWindowsToolOutputDir(args, toolCpp);
    }
#endif
    return Path_join(args->buildDir, "_Tools");
}

char* buildToolIntermediateDirectory(const CompilationInfo* ci) {
    char* inter1 = Path_join(ci->toolOutputDir, "_Intermediates");
    char* path = Path_join(inter1, ci->targetOS ? ci->targetOS : "Unknown");
    free(inter1);
    return path;
}

#ifdef _WIN32
void setupWindowsSourceAliases(CompilationInfo* ci) {
    WindowsPath_applyProjectRootSourceAliasIfNeeded(&ci->scCpp, ci->toolOutputDir, ci->args->projectDir, "SC.cpp");
    WindowsPath_applyProjectRootSourceAliasIfNeeded(&ci->toolsCpp, ci->toolOutputDir, ci->args->projectDir,
                                                    "shared-tool");
    WindowsPath_applyProjectRootSourceAliasIfNeeded(&ci->toolCpp, ci->toolOutputDir, ci->args->projectDir,
                                                    "build-tool");
}
#endif

const char* objectExtension(void) {
#ifdef _WIN32
    return "obj";
#else
    return "o";
#endif
}

const char* dependencyExtension(const CompilationInfo* ci) {
    return ci->targetOS && strcmp(ci->targetOS, "Windows") == 0 ? "json" : "d";
}

char* Path_joinWithExtension(const char* directory, const char* stem, const char* extension) {
    char* fileName = String_concat3(stem, ".", extension);
    if (!fileName) return NULL;
    char* path = Path_join(directory, fileName);
    free(fileName);
    return path;
}

char* buildToolStem(const char* toolName) {
    size_t prefixLength = strlen("SC-");
    size_t toolLength = strlen(toolName);
    char* stem = (char*)malloc(prefixLength + toolLength + 1);
    if (!stem) return NULL;
    memcpy(stem, "SC-", prefixLength);
    memcpy(stem + prefixLength, toolName, toolLength);
    stem[prefixLength + toolLength] = 0;
    return stem;
}

char* buildToolsObjectPath(const CompilationInfo* ci) {
    return Path_joinWithExtension(ci->intermediateDir, "Tools", objectExtension());
}

char* buildToolObjectPath(const CompilationInfo* ci) {
    char* stem = buildToolStem(ci->args->toolName);
    if (!stem) return NULL;
    char* path = Path_joinWithExtension(ci->intermediateDir, stem, objectExtension());
    free(stem);
    return path;
}

char* buildToolsDependencyPath(const CompilationInfo* ci) {
    return Path_joinWithExtension(ci->intermediateDir, "Tools", dependencyExtension(ci));
}

char* buildToolDependencyPath(const CompilationInfo* ci) {
    char* stem = buildToolStem(ci->args->toolName);
    if (!stem) return NULL;
    char* path = Path_joinWithExtension(ci->intermediateDir, stem, dependencyExtension(ci));
    free(stem);
    return path;
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
    wchar_t wpath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    if (!WindowsPath_prepareTransportPath(path, wpath, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1)) return NULL;

    size_t mlen = strlen(mode);
    wchar_t* wmode = (wchar_t*)malloc((mlen + 1) * sizeof(wchar_t));
    if (wmode) {
        for (size_t i = 0; i <= mlen; ++i) wmode[i] = mode[i];
        FILE* fp = _wfopen(wpath, wmode);
        free(wmode);
        return fp;
    }
    return NULL;
#else
    return fopen(path, mode);
#endif
}

TimePoint FileSystem_getModificationTime(const char* path) {
#ifdef _WIN32
    wchar_t wpath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    if (WindowsPath_prepareTransportPath(path, wpath, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1)) {
        WIN32_FILE_ATTRIBUTE_DATA info;
        int ret = GetFileAttributesExW(wpath, GetFileExInfoStandard, &info);
        if (ret != 0) {
            ULARGE_INTEGER fileTime;
            fileTime.LowPart  = info.ftLastWriteTime.dwLowDateTime;
            fileTime.HighPart = info.ftLastWriteTime.dwHighDateTime;
            return (TimePoint)(fileTime.QuadPart * 100ULL);
        }
    }
    return 0;
#else
    struct stat info;
    if (stat(path, &info) == 0) {
#ifdef __APPLE__
        return (TimePoint)info.st_mtimespec.tv_sec * 1000000000ULL + (TimePoint)info.st_mtimespec.tv_nsec;
#else
        return (TimePoint)info.st_mtim.tv_sec * 1000000000ULL + (TimePoint)info.st_mtim.tv_nsec;
#endif
    }
    return 0;
#endif
}

int FileSystem_exists(const char* path) {
#ifdef _WIN32
    struct _stat info;
    wchar_t wpath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    return WindowsPath_prepareTransportPath(path, wpath, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1) &&
           _wstat(wpath, &info) == 0;
#else
    struct stat info;
    return stat(path, &info) == 0;
#endif
}

int FileSystem_isDirectory(const char* path) {
#ifdef _WIN32
    struct _stat info;
    wchar_t wpath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    if (!WindowsPath_prepareTransportPath(path, wpath, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1)) return 0;
    if (_wstat(wpath, &info) != 0) return 0;
    return (info.st_mode & _S_IFDIR) != 0;
#else
    struct stat info;
    if (stat(path, &info) != 0) return 0;
    return S_ISDIR(info.st_mode);
#endif
}

int FileSystem_createDirectory(const char* path) {
#ifdef _WIN32
    wchar_t wpath[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    if (!WindowsPath_prepareTransportPath(path, wpath, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1)) return -1;
    return _wmkdir(wpath);
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
    args->projectDir = NULL;
    args->toolName = NULL;
    args->remainingArgs = NULL;
}

void BootloaderArgs_destroy(BootloaderArgs* args) {
    if (args->libraryDir) free(args->libraryDir);
    if (args->toolSourceDir) free(args->toolSourceDir);
    if (args->buildDir) free(args->buildDir);
    if (args->projectDir) free(args->projectDir);
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

void BootloaderArgs_copyRemainingArgs(BootloaderArgs* args, const char** string_argv, int firstIndex, int argc) {
    args->numRemainingArgs = (argc > firstIndex) ? (argc - firstIndex) : 0;
    if (args->numRemainingArgs > 0) {
        args->remainingArgs = (char**)malloc(args->numRemainingArgs * sizeof(char*));
        if (args->remainingArgs) {
            for (int i = 0; i < args->numRemainingArgs; ++i) {
                args->remainingArgs[i] = String_duplicate(string_argv[firstIndex + i]);
            }
        }
    }
}

// parseArgs
BootloaderArgs parseArgs(const char** string_argv, int argc) {
    BootloaderArgs args;
    BootloaderArgs_init(&args);
    // Assume the first 5 are from SC.sh/bat, then tool name, then args
    if (argc >= 5) {
        args.libraryDir = String_duplicate(string_argv[1]);
        args.toolSourceDir = String_duplicate(string_argv[2]);
        args.buildDir = String_duplicate(string_argv[3]);
        args.projectDir = String_duplicate(string_argv[4]);
        args.toolName = (argc >= 6) ? String_duplicate(string_argv[5]) : String_duplicate("build");
        BootloaderArgs_copyRemainingArgs(&args, string_argv, 6, argc);
    } else {
        args.toolName = String_duplicate("build");
    }
    return args;
}

// setupCompilation
void setupCompilation(CompilationInfo* ci) {
    BootloaderArgs* args = ci->args;
    ci->targetOS = detectTargetOS();

    // Set paths like original
    int builtInTool = 0;
    char* toolCpp = resolveToolSource(args, &builtInTool);

    // Set ci fields
    ci->toolCpp = toolCpp;
    ci->scCpp = Path_join(args->libraryDir, "SC.cpp");
    ci->toolsCpp = Path_join(args->toolSourceDir, "Tools.cpp");
    ci->toolOutputDir = buildToolOutputDirectory(args, toolCpp, ci->targetOS);

#ifdef _WIN32
    setupWindowsSourceAliases(ci);
    toolCpp = ci->toolCpp;
#endif

    // intermediateDir
    ci->intermediateDir = buildToolIntermediateDirectory(ci);

    // dep files
    ci->toolsDepFile = buildToolsDependencyPath(ci);
    ci->toolDepFile  = buildToolDependencyPath(ci);

    // toolH
    if (builtInTool && strstr(toolCpp, "ToolsBootstrap.") != toolCpp) { // Not this file
        ci->toolH = buildBuiltInToolHeaderPath(args);
    }

    // toolExe
    ci->toolExe = buildToolExecutablePath(ci);
}

// Command execution implementation
int runCommand(const char* command) {
    if (printMessages) {
        printf("Running: %s\n", command);
    }
#ifdef _WIN32
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    wchar_t wcmd[4096];
    wchar_t wdir[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    LPCWSTR workingDirectory = NULL;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, command, -1, wcmd, 4096);
    if (wlen == 0) return 1;
    if (windowsCommandWorkingDirectory &&
        WindowsPath_prepareTransportPath(windowsCommandWorkingDirectory, wdir, SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1)) {
        workingDirectory = wdir;
    }
    if (CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, 0, NULL, workingDirectory, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0xFFFFFFFFu;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0 ? 0 : 1;
    } else {
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

int CommandLine_runInWorkingDirectory(CommandLine* cl, const char* workingDirectory) {
#ifdef _WIN32
    const char* previousWorkingDirectory = windowsCommandWorkingDirectory;
    windowsCommandWorkingDirectory      = workingDirectory;
    int ret                             = CommandLine_run(cl);
    windowsCommandWorkingDirectory      = previousWorkingDirectory;
    return ret;
#else
    (void)workingDirectory;
    return CommandLine_run(cl);
#endif
}

void CommandLine_destroy(CommandLine* cl) {
    StringVector_destroy(&cl->args);
}

#ifdef _WIN32
static int WindowsCommandLine_spawn(CommandLine* cmd, LPCWSTR workingDirectory, HANDLE* process) {
    char* command = StringVector_join(&cmd->args, " ");
    if (!command) return 0;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
    if (wlen <= 0) {
        free(command);
        return 0;
    }

    wchar_t* wcmd = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    if (!wcmd) {
        free(command);
        return 0;
    }

    int spawned = 0;
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wcmd, wlen);

    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, 0, NULL, workingDirectory, &si, &pi)) {
        *process = pi.hProcess;
        CloseHandle(pi.hThread);
        spawned = 1;
    }

    free(wcmd);
    free(command);
    return spawned;
}
#endif

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
        if (strstr(str, "\"Source\"")) {
            char* colon = strstr(str, ":");
            char* start = colon ? strstr(colon, "\"") : NULL;
            char* end = start ? strstr(start + 1, "\"") : NULL;
            if (start && end) {
                *end = 0;
                String_unescapeJsonInPlace(start + 1);
                StringVector_add(&deps, start + 1);
            }
        }
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
                    String_unescapeJsonInPlace(dep);
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
        if (srcTime > objTime) {
            if (printMessages) {
                printf("  Source %s modified (time %llu > %llu), needs rebuild\n", sources->data[i],
                       (unsigned long long)srcTime, (unsigned long long)objTime);
            }
            return 1;
        }
    }
    for (size_t i = 0; i < dependencies->count; ++i) {
        if (!FileSystem_exists(dependencies->data[i])) {
            if (printMessages) {
                printf("  Dependency %s is missing, needs rebuild\n", dependencies->data[i]);
            }
            return 1;
        } else {
            TimePoint depTime = FileSystem_getModificationTime(dependencies->data[i]);
            if (depTime > objTime) {
                if (printMessages) {
                    printf("  Dependency %s modified (time %llu > %llu), needs rebuild\n", dependencies->data[i],
                           (unsigned long long)depTime, (unsigned long long)objTime);
                }
                return 1;
            }
        }
    }
    return 0;
}

static int dependenciesContainRequiredSources(StringVector* deps, const char* first, const char* second) {
    if (deps->count == 0 || !StringVector_containsPath(deps, first)) {
        return 0;
    }
    return second == NULL || StringVector_containsPath(deps, second);
}

static void printObjectRebuildStatus(const char* label, int needsRebuild) {
    if (!printMessages) {
        return;
    }
    if (needsRebuild) {
        printf("  %s obj needs rebuild\n", label);
    } else {
        printf("  %s obj up to date\n", label);
    }
}

static int needsRebuildObject(const char* label, const char* objectPath, const char* dependencyPath,
                              const char* dependencyBaseDir, StringVector* sources, const char* requiredFirst,
                              const char* requiredSecond) {
    TimePoint objTime = FileSystem_getModificationTime(objectPath);
    if (objTime == 0) {
        if (printMessages) printf("  %s obj file not found, needs rebuild\n", label);
        return 1;
    }

    StringVector deps = parseDependencies(dependencyPath, dependencyBaseDir);
    if (!dependenciesContainRequiredSources(&deps, requiredFirst, requiredSecond)) {
        if (printMessages) {
            printf("  %s obj dependencies are stale, needs rebuild\n", label);
        }
        StringVector_destroy(&deps);
        return 1;
    }

    int ret = checkNeedsRebuild(objTime, sources, &deps);
    printObjectRebuildStatus(label, ret);
    StringVector_destroy(&deps);
    return ret;
}

int needsRebuildTools(CompilationInfo* ci) {
    char* toolsObj = buildToolsObjectPath(ci);

    StringVector sources;
    StringVector_init(&sources, 2);
    StringVector_add(&sources, ci->scCpp);
    StringVector_add(&sources, ci->toolsCpp);

    int ret = needsRebuildObject("Tools", toolsObj, ci->toolsDepFile, ci->intermediateDir, &sources, ci->toolsCpp,
                                 ci->scCpp);

    StringVector_destroy(&sources);
    free(toolsObj);
    return ret;
}

int needsRebuildToolObj(CompilationInfo* ci) {
    char* toolObj = buildToolObjectPath(ci);

    StringVector sources;
    StringVector_init(&sources, 2);
    StringVector_add(&sources, ci->toolCpp);
    if (ci->toolH) StringVector_add(&sources, ci->toolH);

    int ret =
        needsRebuildObject("Tool", toolObj, ci->toolDepFile, ci->intermediateDir, &sources, ci->toolCpp, NULL);

    StringVector_destroy(&sources);
    free(toolObj);
    return ret;
}

int needsRebuildExe(CompilationInfo* ci) {
    TimePoint exeTime = FileSystem_getModificationTime(ci->toolExe);
    if (exeTime == 0) return 1;

    char* toolsObj = buildToolsObjectPath(ci);
    char* toolObj  = buildToolObjectPath(ci);
    TimePoint toolsObjTime = FileSystem_getModificationTime(toolsObj);
    TimePoint toolObjTime = FileSystem_getModificationTime(toolObj);

    free(toolsObj);
    free(toolObj);

    return (toolsObjTime > exeTime || toolObjTime > exeTime);
}

// Helper functions for building compile commands
void buildCompileCommandPOSIX(CommandLine* cmd, const char* compiler, const char* includeDir, const char* output,
                              const char* input, int useClang, int defineSCBuild) {
    (void)useClang;
    CommandLine_init(cmd, compiler);
    CommandLine_arg(cmd, "-I");
    CommandLine_argQuoted(cmd, includeDir);
    CommandLine_arg(cmd, "-std=c++17");
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
    if (!bootstrapIncludeStdCpp()) {
        CommandLine_arg(cmd, "-DSC_INCLUDE_STD_CPP=0");
        CommandLine_arg(cmd, "-nostdinc++");
    }
    if (!bootstrapLinkStdCpp()) {
        CommandLine_arg(cmd, "-DSC_PROVIDE_CPP_RUNTIME_SHIMS=1");
    }
    if (defineSCBuild) {
        CommandLine_arg(cmd, "-DSC_BUILD=1");
    }

    CommandLine_arg(cmd, "-o");
    CommandLine_argQuoted(cmd, output);
    CommandLine_arg(cmd, "-c");
    CommandLine_argQuoted(cmd, input);
}

static void prepareBootstrapExecutableDirectory(const CompilationInfo* ci) {
    char* exeDir = Path_join(ci->toolOutputDir, ci->targetOS);
    FileSystem_createDirectoryRecursive(exeDir);
    free(exeDir);
}

#ifndef _WIN32
static int runPOSIXObjectCompilation(const char* compiler, const char* includeDir, const char* output,
                                     const char* input, int useClang, int defineSCBuild) {
    CommandLine cmd;
    buildCompileCommandPOSIX(&cmd, compiler, includeDir, output, input, useClang, defineSCBuild);
    int ret = CommandLine_run(&cmd);
    CommandLine_destroy(&cmd);
    return ret == 0;
}

static int runSharedToolsPOSIXObjectCompilation(const CompilationInfo* ci, const char* compiler,
                                                const char* includeDir, int useClang) {
    char* toolsObj = buildToolsObjectPath(ci);
    int compiled = runPOSIXObjectCompilation(compiler, includeDir, toolsObj, ci->toolsCpp, useClang, 0);
    free(toolsObj);
    return compiled;
}

static int runBuildToolPOSIXObjectCompilation(const CompilationInfo* ci, const char* compiler,
                                              const char* includeDir, int useClang, int defineSCBuild) {
    char* toolObj = buildToolObjectPath(ci);
    int compiled = runPOSIXObjectCompilation(compiler, includeDir, toolObj, ci->toolCpp, useClang, defineSCBuild);
    free(toolObj);
    return compiled;
}

static int linkPOSIX(CompilationInfo* ci, const char* compiler, const char* cCompiler, int useClang) {
    prepareBootstrapExecutableDirectory(ci);

    char* toolsObj = buildToolsObjectPath(ci);
    char* toolObj  = buildToolObjectPath(ci);

    CommandLine cmd;
    CommandLine_init(&cmd, (!useClang && !bootstrapLinkStdCpp()) ? cCompiler : compiler);
    CommandLine_arg(&cmd, "-o");
    CommandLine_argQuoted(&cmd, ci->toolExe);
    CommandLine_argQuoted(&cmd, toolsObj);
    CommandLine_argQuoted(&cmd, toolObj);
    if (strcmp(ci->targetOS, "Linux") == 0) {
        CommandLine_arg(&cmd, "-rdynamic");
    }
    CommandLine_arg(&cmd, "-ldl");
    CommandLine_arg(&cmd, "-lpthread");
    if (strcmp(ci->targetOS, "Linux") == 0 && !useClang && !bootstrapLinkStdCpp()) {
        CommandLine_arg(&cmd, "-lm");
    }
    if (useClang && !bootstrapLinkStdCpp()) {
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

static int detectPOSIXCompiler(const char** compiler, const char** cCompiler, int* useClang) {
    if (runCommand("clang++ --version > /dev/null 2>&1") == 0) {
        *compiler  = "clang++";
        *cCompiler = "clang";
        *useClang  = 1;
        return 1;
    }
    if (runCommand("g++ --version > /dev/null 2>&1") == 0) {
        *compiler  = "g++";
        *cCompiler = "gcc";
        *useClang  = 0;
        return 1;
    }
    return 0;
}

static int waitForPOSIXProcess(pid_t pid) {
    int status = 0;
    if (waitpid(pid, &status, 0) != pid) {
        return 0;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int waitForPOSIXObjectCompilations(pid_t pid1, pid_t pid2) {
    int success1 = waitForPOSIXProcess(pid1);
    int success2 = waitForPOSIXProcess(pid2);
    return success1 && success2;
}

static int runParallelPOSIXObjectCompilations(const CompilationInfo* ci, const char* compiler, const char* includeDir,
                                              int useClang, int defineSCBuild) {
    pid_t pid1 = fork();
    if (pid1 < 0) {
        return 0;
    }
    if (pid1 == 0) {
        exit(runSharedToolsPOSIXObjectCompilation(ci, compiler, includeDir, useClang) ? 0 : 1);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        waitForPOSIXProcess(pid1);
        return 0;
    }
    if (pid2 == 0) {
        exit(runBuildToolPOSIXObjectCompilation(ci, compiler, includeDir, useClang, defineSCBuild) ? 0 : 1);
    }

    return waitForPOSIXObjectCompilations(pid1, pid2);
}
#endif

#ifdef _WIN32
void buildCompileCommandWindows(CommandLine* cmd, const char* intermediateDir, const char* includeDir,
                                const char* output, const char* input, const char* jsonFile, const char* pdbName,
                                int defineSCBuild) {
    CommandLine_init(cmd, "cl.exe");
    CommandLine_arg(cmd, "/nologo");
    CommandLine_arg(cmd, "/I");
    CommandLine_argQuoted(cmd, includeDir);
    CommandLine_arg(cmd, "/std:c++17");
    CommandLine_arg(cmd, "/D_DEBUG");
    if (!bootstrapIncludeStdCpp()) {
        CommandLine_arg(cmd, "/DSC_INCLUDE_STD_CPP=0");
    }
    if (!bootstrapLinkStdCpp()) {
        CommandLine_arg(cmd, "/DSC_PROVIDE_CPP_RUNTIME_SHIMS=1");
    }
    if (defineSCBuild) {
        CommandLine_arg(cmd, "/DSC_BUILD=1");
    }
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

char* buildWindowsLinkManifestSourcePath(const CompilationInfo* ci) {
    char* manifestPath = String_duplicate(ci->toolsCpp);
    if (!manifestPath) {
        return NULL;
    }

    char* lastSlash     = strrchr(manifestPath, '/');
    char* lastBackslash = strrchr(manifestPath, '\\');
    char* lastSeparator = lastSlash;
    if (lastBackslash && (!lastSeparator || lastBackslash > lastSeparator)) {
        lastSeparator = lastBackslash;
    }
    if (!lastSeparator) {
        free(manifestPath);
        return Path_join(ci->args->toolSourceDir, "LongPathAware.manifest");
    }

    lastSeparator[1] = '\0';
    char* resizedManifestPath =
        (char*)realloc(manifestPath, strlen(manifestPath) + strlen("LongPathAware.manifest") + 1);
    if (!resizedManifestPath) {
        free(manifestPath);
        return NULL;
    }
    manifestPath = resizedManifestPath;
    strcat(manifestPath, "LongPathAware.manifest");
    return manifestPath;
}

int stageWindowsLinkManifest(const CompilationInfo* ci) {
    char* sourcePath      = buildWindowsLinkManifestSourcePath(ci);
    char* destinationPath = Path_join(ci->toolOutputDir, "LongPathAware.manifest");
    if (!sourcePath || !destinationPath) {
        free(sourcePath);
        free(destinationPath);
        return 0;
    }

    wchar_t sourceTransport[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    wchar_t destinationTransport[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    int prepared = WindowsPath_prepareTransportPath(sourcePath, sourceTransport,
                                                    SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1) &&
                   WindowsPath_prepareTransportPath(destinationPath, destinationTransport,
                                                    SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1);
    int copied = prepared && CopyFileW(sourceTransport, destinationTransport, FALSE) != 0;
    if (!copied) {
        fprintf(stderr, "Error: Failed staging Windows long-path manifest\n");
    }
    free(sourcePath);
    free(destinationPath);
    return copied;
}

char* buildWindowsLinkOutputAndPdbFlag(const CompilationInfo* ci) {
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
    StringBuilder_append(&sb, ".pdb\"");
    char* flag = String_duplicate(StringBuilder_get_buffer(&sb));
    StringBuilder_destroy(&sb);
    return flag;
}

char* buildWindowsLinkManifestFlag(const CompilationInfo* ci) {
    if (!stageWindowsLinkManifest(ci)) {
        return NULL;
    }
    return String_duplicate("/MANIFESTINPUT:\"LongPathAware.manifest\"");
}

void addWindowsNoStdCppLinkArguments(CommandLine* cmd) {
    CommandLine_arg(cmd, "/NODEFAULTLIB:libcpmt");
    CommandLine_arg(cmd, "/NODEFAULTLIB:libcpmtd");
    CommandLine_arg(cmd, "/NODEFAULTLIB:msvcprt");
    CommandLine_arg(cmd, "/NODEFAULTLIB:msvcprtd");
    CommandLine_arg(cmd, "/NODEFAULTLIB:msvcp140");
    CommandLine_arg(cmd, "/NODEFAULTLIB:msvcp140d");
}

void addWindowsSystemLinkLibraries(CommandLine* cmd) {
    CommandLine_arg(cmd, "Advapi32.lib");
    CommandLine_arg(cmd, "Shell32.lib");
}

static int runWindowsObjectCompilation(const char* intermediateDir, const char* includeDir, const char* output,
                                       const char* input, const char* jsonFile, const char* pdbName,
                                       int defineSCBuild) {
    CommandLine cmd;
    buildCompileCommandWindows(&cmd, intermediateDir, includeDir, output, input, jsonFile, pdbName, defineSCBuild);
    int ret = CommandLine_run(&cmd);
    CommandLine_destroy(&cmd);
    return ret == 0;
}

static int runSharedToolsObjectCompilation(const CompilationInfo* ci, const char* includeDir) {
    char* toolsObj  = buildToolsObjectPath(ci);
    char* toolsJson = buildToolsDependencyPath(ci);
    int compiled =
        runWindowsObjectCompilation(ci->intermediateDir, includeDir, toolsObj, ci->toolsCpp, toolsJson, "Tools", 0);
    free(toolsObj);
    free(toolsJson);
    return compiled;
}

static int runBuildToolObjectCompilation(const CompilationInfo* ci, const char* includeDir, int defineSCBuild) {
    char* toolObj  = buildToolObjectPath(ci);
    char* toolJson = buildToolDependencyPath(ci);
    int compiled = runWindowsObjectCompilation(ci->intermediateDir, includeDir, toolObj, ci->toolCpp, toolJson,
                                               ci->args->toolName, defineSCBuild);
    free(toolObj);
    free(toolJson);
    return compiled;
}

static int spawnWindowsObjectCompilation(const char* intermediateDir, const char* includeDir, const char* output,
                                         const char* input, const char* jsonFile, const char* pdbName,
                                         int defineSCBuild, LPCWSTR workingDirectory, HANDLE* process) {
    CommandLine cmd;
    buildCompileCommandWindows(&cmd, intermediateDir, includeDir, output, input, jsonFile, pdbName, defineSCBuild);
    int spawned = WindowsCommandLine_spawn(&cmd, workingDirectory, process);
    CommandLine_destroy(&cmd);
    return spawned;
}

static int spawnSharedToolsObjectCompilation(const CompilationInfo* ci, const char* includeDir,
                                             LPCWSTR workingDirectory, HANDLE* process) {
    char* toolsObj  = buildToolsObjectPath(ci);
    char* toolsJson = buildToolsDependencyPath(ci);
    int spawned = spawnWindowsObjectCompilation(ci->intermediateDir, includeDir, toolsObj, ci->toolsCpp, toolsJson,
                                                "Tools", 0, workingDirectory, process);
    free(toolsObj);
    free(toolsJson);
    return spawned;
}

static int spawnBuildToolObjectCompilation(const CompilationInfo* ci, const char* includeDir, int defineSCBuild,
                                           LPCWSTR workingDirectory, HANDLE* process) {
    char* toolObj  = buildToolObjectPath(ci);
    char* toolJson = buildToolDependencyPath(ci);
    int spawned = spawnWindowsObjectCompilation(ci->intermediateDir, includeDir, toolObj, ci->toolCpp, toolJson,
                                                ci->args->toolName, defineSCBuild, workingDirectory, process);
    free(toolObj);
    free(toolJson);
    return spawned;
}

static int waitForWindowsObjectCompilations(HANDLE* processes, int count) {
    if (count != 2) {
        for (int idx = 0; idx < count; ++idx) {
            WaitForSingleObject(processes[idx], INFINITE);
            CloseHandle(processes[idx]);
        }
        return 0;
    }

    WaitForMultipleObjects(2, processes, TRUE, INFINITE);
    DWORD exit1, exit2;
    GetExitCodeProcess(processes[0], &exit1);
    GetExitCodeProcess(processes[1], &exit2);
    CloseHandle(processes[0]);
    CloseHandle(processes[1]);
    return exit1 == 0 && exit2 == 0;
}

typedef struct {
    const char* previousWorkingDirectory;
    wchar_t spawnWorkingDirectoryBuffer[SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1];
    LPCWSTR spawnWorkingDirectory;
} WindowsBootstrapCompileWorkingDirectory;

static void WindowsBootstrapCompileWorkingDirectory_enter(WindowsBootstrapCompileWorkingDirectory* context,
                                                          const char* toolOutputDir) {
    context->previousWorkingDirectory = windowsCommandWorkingDirectory;
    context->spawnWorkingDirectory    = NULL;
    windowsCommandWorkingDirectory    = toolOutputDir;
    if (WindowsPath_prepareTransportPath(toolOutputDir, context->spawnWorkingDirectoryBuffer,
                                         SC_WINDOWS_TRANSPORT_PATH_CAPACITY + 1)) {
        context->spawnWorkingDirectory = context->spawnWorkingDirectoryBuffer;
    }
}

static void WindowsBootstrapCompileWorkingDirectory_leave(WindowsBootstrapCompileWorkingDirectory* context) {
    windowsCommandWorkingDirectory = context->previousWorkingDirectory;
}

static int runParallelWindowsObjectCompilations(const CompilationInfo* ci, const char* includeDir, int defineSCBuild,
                                                LPCWSTR workingDirectory) {
    HANDLE processes[2];
    int count = 0;
    if (spawnSharedToolsObjectCompilation(ci, includeDir, workingDirectory, &processes[count])) {
        count += 1;
    }
    if (spawnBuildToolObjectCompilation(ci, includeDir, defineSCBuild, workingDirectory, &processes[count])) {
        count += 1;
    }
    return waitForWindowsObjectCompilations(processes, count);
}
#endif

static char* prepareBootstrapCompileDirectories(CompilationInfo* ci) {
    FileSystem_createDirectoryRecursive(ci->intermediateDir);
    FileSystem_createDirectoryRecursive(ci->toolOutputDir);
    return Path_join(ci->args->libraryDir, "Includes");
}

#ifndef _WIN32
int compilePOSIX(CompilationInfo* ci) {
    // Simplified: assume clang++ or g++
    const char* compiler  = "";
    const char* cCompiler = "";
    char* includeDir      = prepareBootstrapCompileDirectories(ci);
    int useClang = 0;
    int defineSCBuild = isSCBuildDefinitionSource(ci->toolCpp);
    int success = 1;
    if (!detectPOSIXCompiler(&compiler, &cCompiler, &useClang)) {
        success = 0;
        goto cleanup;
    }

    int needTools = needsRebuildTools(ci);
    int needTool = needsRebuildToolObj(ci);

    if (needTools && needTool) {
        if (!runParallelPOSIXObjectCompilations(ci, compiler, includeDir, useClang, defineSCBuild)) {
            success = 0;
            goto cleanup;
        }
        printf("Tools.cpp\n");
        printf("SC-%s.cpp\n", ci->args->toolName);
    } else if (needTools) {
        printf("Tools.cpp\n");
        if (!runSharedToolsPOSIXObjectCompilation(ci, compiler, includeDir, useClang)) {
            success = 0;
            goto cleanup;
        }
    } else if (needTool) {
        printf("SC-%s.cpp\n", ci->args->toolName);
        if (!runBuildToolPOSIXObjectCompilation(ci, compiler, includeDir, useClang, defineSCBuild)) {
            success = 0;
            goto cleanup;
        }
    } else {
        printf("\"%s\" is up to date\n", ci->toolsCpp);
        printf("\"%s\" is up to date\n", ci->toolCpp);
    }

cleanup:
    free(includeDir);
    if (!success) {
        return 1;
    }

    if (needsRebuildExe(ci)) {
        return linkPOSIX(ci, compiler, cCompiler, useClang);
    }
    return 0;
}
#endif

#ifdef _WIN32
int linkWindows(CompilationInfo* ci) {
    printf("Linking %s\n", ci->args->toolName);
    prepareBootstrapExecutableDirectory(ci);

    char* toolsObj = buildToolsObjectPath(ci);
    char* toolObj  = buildToolObjectPath(ci);

    CommandLine cmd;
    CommandLine_init(&cmd, "link");
    CommandLine_arg(&cmd, "/nologo");
    CommandLine_arg(&cmd, "/DEBUG");
    CommandLine_arg(&cmd, "/MANIFEST:EMBED");
    char* outputAndPdbFlag = buildWindowsLinkOutputAndPdbFlag(ci);
    if (outputAndPdbFlag) {
        CommandLine_arg(&cmd, outputAndPdbFlag);
        free(outputAndPdbFlag);
    }
    char* manifestFlag = buildWindowsLinkManifestFlag(ci);
    if (manifestFlag) {
        CommandLine_arg(&cmd, manifestFlag);
        free(manifestFlag);
    }
    CommandLine_argQuoted(&cmd, toolsObj);
    CommandLine_argQuoted(&cmd, toolObj);
    if (!bootstrapLinkStdCpp()) {
        addWindowsNoStdCppLinkArguments(&cmd);
    }
    addWindowsSystemLinkLibraries(&cmd);
    int ret = CommandLine_runInWorkingDirectory(&cmd, ci->toolOutputDir);
    CommandLine_destroy(&cmd);
    free(toolsObj);
    free(toolObj);
    return ret;
}

int compileWindows(CompilationInfo* ci, int* objsCompiled);
#endif

void CommandLine_addBootstrapToolArguments(CommandLine* cmd, BootloaderArgs* args, CompilationInfo* ci) {
    CommandLine_argQuoted(cmd, args->libraryDir);
    CommandLine_argQuoted(cmd, args->toolSourceDir);
    CommandLine_argQuoted(cmd, args->buildDir);
    CommandLine_argQuoted(cmd, args->projectDir ? args->projectDir : args->libraryDir);
    CommandLine_argQuoted(cmd, args->toolName);
    for (int i = 0; i < args->numRemainingArgs; ++i) {
        CommandLine_argQuoted(cmd, args->remainingArgs[i]);
    }
}

int executeTool(BootloaderArgs* args, CompilationInfo* ci) {
    CommandLine cmd;
    CommandLine_init(&cmd, ci->toolExe);
    CommandLine_addBootstrapToolArguments(&cmd, args, ci);
    int ret = CommandLine_runInWorkingDirectory(&cmd, args->libraryDir);
    CommandLine_destroy(&cmd);
    return ret;
}

static void destroyBootstrapInvocation(CompilationInfo* ci, BootloaderArgs* args, int argc, char** argv) {
    CompilationInfo_destroy(ci);
    BootloaderArgs_destroy(args);
    freeArgs(argc, argv);
}

static int failBootstrapInvocation(CompilationInfo* ci, BootloaderArgs* args, int argc, char** argv,
                                   const char* message) {
    fprintf(stderr, "%s\n", message);
    destroyBootstrapInvocation(ci, args, argc, argv);
    return 1;
}

static int reportBootstrapToolResult(BootloaderArgs* args, int ret) {
    if (ret != 0) {
        fprintf(stderr, "Tool \"%s\" execution failed with code %d\n", args->toolName, ret);
    } else {
        printf("Tool \"%s\" executed successfully\n", args->toolName);
    }
    return ret == 0 ? 0 : -1;
}

static BootstrapTimer BootstrapTimer_start(void) {
    BootstrapTimer timer;
#ifdef _WIN32
    timer.startTime = clock();
#else
    gettimeofday(&timer.startTime, NULL);
#endif
    return timer;
}

static double BootstrapTimer_elapsedSeconds(BootstrapTimer* timer) {
#ifdef _WIN32
    clock_t endTime = clock();
    return (double)(endTime - timer->startTime) / CLOCKS_PER_SEC;
#else
    struct timeval endTime;
    gettimeofday(&endTime, NULL);
    return (endTime.tv_sec - timer->startTime.tv_sec) + (endTime.tv_usec - timer->startTime.tv_usec) / 1000000.0;
#endif
}

// Main
int main(int argc, char* argv[]) {
    int new_argc;
    char** new_argv = getArgv(&new_argc, argv, argc);
    if (!new_argv) {
        fprintf(stderr, "Failed to get arguments\n");
        return 1;
    }

    printMessages = getenv("SC_BOOTSTRAP_DEBUG") != NULL;

    BootloaderArgs args = parseArgs((const char**)new_argv, new_argc);
    CompilationInfo ci = CompilationInfo_init(&args);
    setupCompilation(&ci);

    int needsCompile = needsRebuildTools(&ci) || needsRebuildToolObj(&ci);
    int needsLink = needsRebuildExe(&ci);

    BootstrapTimer timer = BootstrapTimer_start();
    if (needsCompile || needsLink) {
        printf("Rebuilding %s tool...\n", args.toolName);
#ifdef _WIN32
        int objsCompiled = 0;
        if (!compileWindows(&ci, &objsCompiled)) {
            return failBootstrapInvocation(&ci, &args, new_argc, new_argv, "Compilation failed");
        }
        if (objsCompiled || needsLink) {
            if (linkWindows(&ci) != 0) {
                return failBootstrapInvocation(&ci, &args, new_argc, new_argv, "Linking failed");
            }
        }
#else
        if (compilePOSIX(&ci) != 0) {
            return failBootstrapInvocation(&ci, &args, new_argc, new_argv, "Compilation failed");
        }
#endif
        double duration = BootstrapTimer_elapsedSeconds(&timer);
        printf("Time to compile \"%s\" tool: %.2f seconds\n", args.toolName, duration);
    } else {
        printf("\"%s\" is up to date\n", ci.toolCpp);
    }

    int ret        = executeTool(&args, &ci);
    int exitStatus = reportBootstrapToolResult(&args, ret);
    destroyBootstrapInvocation(&ci, &args, new_argc, new_argv);
    return exitStatus;
}

#ifdef _WIN32
int compileWindows(CompilationInfo* ci, int* objsCompiled) {
    // Simplified Windows compilation
    char* includeDir = prepareBootstrapCompileDirectories(ci);
    int defineSCBuild = isSCBuildDefinitionSource(ci->toolCpp);
    WindowsBootstrapCompileWorkingDirectory workingDirectory;
    WindowsBootstrapCompileWorkingDirectory_enter(&workingDirectory, ci->toolOutputDir);

    int needTools = needsRebuildTools(ci);
    int needTool = needsRebuildToolObj(ci);
    int success = 1;

    if (needTools && needTool) {
        *objsCompiled = 1;
        if (!runParallelWindowsObjectCompilations(ci, includeDir, defineSCBuild,
                                                  workingDirectory.spawnWorkingDirectory)) {
            success = 0;
            goto cleanup;
        }
    } else if (needTools) {
        *objsCompiled = 1;
        if (!runSharedToolsObjectCompilation(ci, includeDir)) {
            success = 0;
            goto cleanup;
        }
    } else if (needTool) {
        *objsCompiled = 1;
        if (!runBuildToolObjectCompilation(ci, includeDir, defineSCBuild)) {
            success = 0;
            goto cleanup;
        }
    } else {
        printf("\"%s\" is up to date\n", ci->toolsCpp);
        printf("\"%s\" is up to date\n", ci->toolCpp);
    }

cleanup:
    free(includeDir);
    WindowsBootstrapCompileWorkingDirectory_leave(&workingDirectory);
    return success;
}
#endif
