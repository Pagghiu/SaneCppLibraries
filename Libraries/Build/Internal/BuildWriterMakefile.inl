// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../FileSystemIterator/FileSystemIterator.h"
#include "../../Hashing/Hashing.h"
#include "../../Strings/StringBuilder.h"
#include "../Build.h"
#include "BuildWriter.h"

struct SC::Build::ProjectWriter::WriterMakefile
{
    const Definition&         definition;
    const DefinitionCompiler& definitionCompiler;
    const Directories&        directories;

    VectorMap<String, String> outputDirectories;
    VectorMap<String, String> intermediateDirectories;

    WriterMakefile(const Definition& definition, const DefinitionCompiler& definitionCompiler,
                   const Directories& directories)
        : definition(definition), definitionCompiler(definitionCompiler), directories(directories)
    {}
    using RenderItem  = WriterInternal::RenderItem;
    using RenderGroup = WriterInternal::RenderGroup;
    using Renderer    = WriterInternal::Renderer;

    [[nodiscard]] Result writeMakefile(StringBuilder& builder, const Workspace& workspace, Renderer& renderer)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(ifeq ($(VERBOSE), 1)
VRBS =
else
VRBS = @
endif

ifndef CONFIG
 CONFIG=Debug
endif

space := $(null) $(null)

CURDIR_ESCAPED = $(subst $(space),\$(space),$(CURDIR))

.PHONY: clean all

CLANG_DETECTED := $(shell $(CXX) --version 2>&1 | grep -q clang && echo "yes")

# Splitting the Target string
HOST_TARGET := $(shell $(CXX) -v -E - </dev/null 2>&1 | sed -n 's/Target: \([^ ]*\)/\1/p')

# Splitting the HOST_TARGET string

HOST_ARCHITECTURE := $(word 1,$(subst -, ,$(HOST_TARGET)))
HOST_OS_AND_VERSION := $(wordlist 2, $(words $(subst -, ,$(HOST_TARGET))), $(subst -, ,$(HOST_TARGET)))
HOST_OS := $(strip $(word 1, $(HOST_OS_AND_VERSION)))
HOST_OS_VERSION := $(strip $(subst $(HOST_OS),, $(HOST_OS_AND_VERSION)))

ifeq ($(HOST_OS),unknown)
# Clang on linux reports aarch64-unknown-linux-gnu
HOST_OS_AND_VERSION := $(wordlist 3, $(words $(subst -, ,$(HOST_TARGET))), $(subst -, ,$(HOST_TARGET)))
HOST_OS := $(strip $(word 1, $(HOST_OS_AND_VERSION)))
HOST_OS_VERSION := $(strip $(subst $(HOST_OS),, $(HOST_OS_AND_VERSION)))
endif

ifeq ($(HOST_ARCHITECTURE),aarch64)
 HOST_ARCHITECTURE := arm64
endif

ifndef TARGET_OS
 TARGET_OS := $(HOST_OS)
 ifeq ($(TARGET_OS),apple)
	ifneq (,$(findstring ios,$(HOST_OS_VERSION)))
	   TARGET_OS := iOS
	else
	   TARGET_OS := macOS
	endif
 else
   TARGET_OS := linux
 endif
endif

ifndef TARGET_ARCHITECTURE
 TARGET_ARCHITECTURE := $(HOST_ARCHITECTURE)
endif

ifeq ($(CLANG_DETECTED),yes)
COMPILER_TYPE := clang
else
COMPILER_TYPE := gcc
endif

# Detecting Clang Compiler Type and Version
CLANG_VERSION := $(shell $(CXX) --version | sed -n 's/clang version \([0-9]*\)\..*/\1/p')
CLANG_MAJOR_VERSION := $(word 2, $(CLANG_VERSION))

# Detecting GCC Compiler Type and Version
GCC_VERSION := $(shell $(CXX) -dumpversion)
GCC_MAJOR_VERSION := $(firstword $(GCC_VERSION))

# Setting Compiler Type and Version based on detection
ifeq ($(CLANG_MAJOR_VERSION),)
COMPILER_TYPE := gcc
COMPILER_VERSION := $(GCC_MAJOR_VERSION)
else
COMPILER_TYPE := clang
COMPILER_VERSION := $(CLANG_MAJOR_VERSION)
endif

define MJ_if_Clang
    $(if $(CLANG_DETECTED),-MJ "$@.json")
endef

)delimiter");
        SmallString<255> makeTarget;

        builder.append("\nall:");
        for (const Project& project : workspace.projects)
        {
            SC_TRY(sanitizeName(project.targetName.view(), makeTarget));
            builder.append(" {0}_COMPILE_COMMANDS {0}_COMPILE", makeTarget);
        }

        // Clean jobs are better done sequentially
        builder.append("\n\nclean: |");
        for (const Project& project : workspace.projects)
        {
            SC_TRY(sanitizeName(project.targetName.view(), makeTarget));
            builder.append(" {0}_CLEAN", makeTarget.view());
        }

        builder.append("\n\ncompile: all");

        builder.append("\n\nrun:");
        for (const Project& project : workspace.projects)
        {
            SC_TRY(sanitizeName(project.targetName.view(), makeTarget));
            builder.append(" {0}_RUN", makeTarget.view());
        }

        builder.append("\n\nprint-executable-paths:");
        for (const Project& project : workspace.projects)
        {
            SC_TRY(sanitizeName(project.targetName.view(), makeTarget));
            builder.append(" {0}_PRINT_EXECUTABLE_PATH", makeTarget.view());
        }

        builder.append(R"delimiter(

ifneq ($(MAKECMDGOALS),print-executable-paths)
CURRENT_MAKEFILE := $(firstword $(MAKEFILE_LIST))
# Force a clean when makefile is modified
$(CURRENT_MAKEFILE).$(CONFIG).touched: $(CURRENT_MAKEFILE)
	@touch "$@"
	@echo " " > $@ # touch doesn't set proper modification date on hgfs (VMWare)
	@$(MAKE) -f $(CURRENT_MAKEFILE) clean

# Implicitly evaluate the makefile rebuild force clean during parsing
-include $(CURRENT_MAKEFILE).$(CONFIG).touched
endif
)delimiter");
        RelativeDirectories relativeDirectories;
        for (const Project& project : workspace.projects)
        {
            SC_TRY(relativeDirectories.computeRelativeDirectories(directories, Path::AsPosix, project, "$(CURDIR)/{}"));
            renderer.renderItems.clear();
            SC_TRY(WriterInternal::getPathsRelativeTo(directories.projectsDirectory.view(), definitionCompiler, project,
                                                      renderer.renderItems));
            SC_TRY(writeProject(builder, project, renderer, relativeDirectories));
        }

        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    [[nodiscard]] Result writeProject(StringBuilder& builder, const Project& project, Renderer& renderer,
                                      const RelativeDirectories& relativeDirectories)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        SmallString<255> makeTarget;
        SC_TRY(sanitizeName(project.targetName.view(), makeTarget));

        builder.append(R"delimiter(
# {0} Target

{0}_TARGET_NAME := {0}

{0}_PRINT_EXECUTABLE_PATH:
	@echo $({0}_TARGET_DIR)/$({0}_TARGET_NAME)

)delimiter",
                       makeTarget.view());

        // TODO: On GCC we need to enable also the following fixing the warnings
        // -Werror=conversion
        // -Wshadow
        // -Wsign-compare
        // -Werror=sign-conversion
        // -Wmissing-field-initializers

        builder.append("\n{0}_WARNING_FLAGS_CXX :=-Wnon-virtual-dtor -Woverloaded-virtual", makeTarget.view());

        builder.append("\n{0}_WARNING_FLAGS :=-Werror -Werror=return-type -Wunreachable-code  "
                       " -Wmissing-braces -Wparentheses -Wswitch -Wunused-function -Wunused-label "
                       "-Wunused-parameter -Wunused-variable -Wunused-value -Wempty-body -Wuninitialized "
                       "-Wunknown-pragmas -Wenum-conversion -Werror=float-conversion -Werror=implicit-fallthrough",
                       makeTarget.view());

        builder.append("\n{0}_COMMON_FLAGS := $({0}_WARNING_FLAGS)", makeTarget.view());
        for (const String& it : project.compile.defines)
        {
            SC_TRY(builder.append(" \"-D"));
            SC_TRY(appendVariable(builder, it.view(), makeTarget.view(), relativeDirectories));
            SC_TRY(builder.append("\""));
        }
        for (auto& it : project.compile.includePaths)
        {
            SC_TRY(builder.append(" \"-I"));
            if (Path::isAbsolute(it.view(), Path::AsNative))
            {
                String relative;
                SC_TRY(Path::relativeFromTo(directories.projectsDirectory.view(), it.view(), relative, Path::AsNative));
                builder.append("$(CURDIR)/{}", relative);
            }
            else
            {
                builder.append("$(CURDIR)/{}/{}", relativeDirectories.relativeProjectsToProjectRoot, it.view());
            }
            SC_TRY(builder.append("\""));
        }

        for (const Configuration& configuration : project.configurations)
        {
            SC_TRY(writeConfiguration(builder, project, configuration, relativeDirectories, makeTarget.view()))
        }

        builder.append(R"delimiter(

# Cross-compile support
ifeq ($(CLANG_DETECTED),yes)
ifeq ($(TARGET_OS),macOS)
ifeq ($(TARGET_ARCHITECTURE),arm64)
{0}_TARGET_FLAGS := -target arm64-apple-macos11
else
{0}_TARGET_FLAGS := -target x86_64-apple-macos11
endif # TARGET_ARCHITECTURE
endif # TARGET_OS
endif # CLANG_DETECTED

ifeq ($({0}_TARGET_FLAGS),)
ifneq ($(HOST_ARCHITECTURE),$(TARGET_ARCHITECTURE))
$(error "Cross-compiling TARGET_ARCHITECTURE = $(TARGET_ARCHITECTURE) is unsupported")
endif
endif

# Flags for both .c and .cpp files
{0}_CPPFLAGS := $({0}_TARGET_FLAGS) $({0}_COMMON_FLAGS) $({0}_CONFIG_FLAGS) $({0}_CONFIG_COMPILER_FLAGS) $(CPPFLAGS)

# Flags for .c files
{0}_CFLAGS := $({0}_CPPFLAGS) $({0}_CONFIG_COMPILER_FLAGS) $(CFLAGS)
)delimiter",
                       makeTarget.view());

        builder.append("\n# Flags for .cpp files");
        builder.append("\n{0}_CXXFLAGS := $({0}_CPPFLAGS) $({0}_WARNING_FLAGS_CXX) $({0}_CONFIG_CXXFLAGS)",
                       makeTarget.view());

        builder.append(" $(CXXFLAGS)");

        builder.append("\n{0}_FRAMEWORKS_ANY :=", makeTarget.view());
        {
            for (const String& it : project.link.frameworks)
            {
                builder.append(" -framework {0}", it.view());
            }
        }
        builder.append("\n{0}_FRAMEWORKS_MACOS :=", makeTarget.view());
        {
            for (const String& it : project.link.frameworksMacOS)
            {
                builder.append(" -framework {0}", it.view());
            }
        }
        builder.append("\n{0}_FRAMEWORKS_IOS :=", makeTarget.view());
        {
            for (const String& it : project.link.frameworksIOS)
            {
                builder.append(" -framework {0}", it.view());
            }
        }

        builder.append("\nifeq ($(TARGET_OS),macOS)\n");
        builder.append("     {0}_FRAMEWORKS := $({0}_FRAMEWORKS_ANY) $({0}_FRAMEWORKS_MACOS)\n", makeTarget.view());
        builder.append("else\n");
        builder.append("     {0}_FRAMEWORKS := $({0}_FRAMEWORKS_ANY) $({0}_FRAMEWORKS_IOS)\n", makeTarget.view());
        builder.append("endif\n");

        builder.append("\n{0}_LIBRARIES :=", makeTarget.view());
        {
            for (const String& it : project.link.libraries)
            {
                builder.append(" -l{}", it.view());
            }
        }

        builder.append("\nifeq ($(TARGET_OS),macOS)\n");
        builder.append("     {0}_OS_LDFLAGS := $({0}_FRAMEWORKS)\n", makeTarget.view());
        builder.append("else ifeq ($(TARGET_OS),iOS)\n");
        builder.append("     {0}_OS_LDFLAGS := $({0}_FRAMEWORKS)\n", makeTarget.view());
        builder.append("else ifeq ($(TARGET_OS),linux)\n");
        // -rdynamic is needed to resolve Plugin symbols in the executable
        builder.append("     {0}_OS_LDFLAGS := -rdynamic\n", makeTarget.view());
        builder.append("else\n");
        builder.append("     {0}_OS_LDFLAGS :=\n", makeTarget.view());
        builder.append("endif\n");

        // TODO: De-hardcode LDFLAGS
        builder.append("\n{0}_LDFLAGS :=", makeTarget.view());

        builder.append(" $({0}_TARGET_FLAGS) $({0}_CONFIG_LDFLAGS) "
                       "$({0}_CONFIG_COMPILER_LDFLAGS) $({0}_LIBRARIES) "
                       "$({0}_OS_LDFLAGS) $(LDFLAGS)",
                       makeTarget.view());

        builder.append(R"delimiter(
{0}_CLEAN:
	@echo Cleaning {0}
	$(VRBS)rm -rf $({0}_TARGET_DIR)/$(TARGET) $({0}_INTERMEDIATE_DIR)

)delimiter",
                       makeTarget.view());

        builder.append("{0}_OBJECT_FILES := \\", makeTarget.view());

        for (const RenderItem& item : renderer.renderItems)
        {
            const StringView extension = RenderItem::getExtension(item.type);
            if (extension.isEmpty())
                continue;
            builder.append("\n$({0}_INTERMEDIATE_DIR)/", makeTarget.view());
            builder.appendReplaceAll(Path::basename(item.name.view(), extension), " ", "\\ ");
            builder.append(".o \\");
        }
        builder.append(R"delimiter(

# Rebuild object files when an header dependency changes
-include $({0}_OBJECT_FILES:.o=.d)
)delimiter",
                       makeTarget.view());

        builder.append(R"delimiter(

{0}_COMPILE: $({0}_TARGET_DIR)/$({0}_TARGET_NAME)

ifeq ($(CLANG_DETECTED),yes)
{0}_COMPILE_COMMANDS: $({0}_INTERMEDIATE_DIR)/compile_commands.json
else
# On GCC generating compile_commands.json is not supported but it's expected for _COMPILE_COMMANDS to compile the executable too
{0}_COMPILE_COMMANDS: $({0}_TARGET_DIR)/$({0}_TARGET_NAME)
endif

$({0}_INTERMEDIATE_DIR)/compile_commands.json: $({0}_TARGET_DIR)/$({0}_TARGET_NAME)
	@echo Generate compile_commands.json
	$(VRBS)sed -e '1s/^/[\'$$'\n''/' -e '$$s/,$$/\'$$'\n'']/' $({0}_INTERMEDIATE_DIR)/*.o.json > $({0}_INTERMEDIATE_DIR)/compile_commands.json
# Under GNU sed
# gsed -e '1s/^/[\n/' -e '$$s/,$$/\n]/' *.o.json > compile_commands.json
)delimiter",
                       makeTarget.view());

        builder.append(R"delimiter(
$({0}_TARGET_DIR)/$({0}_TARGET_NAME): $({0}_OBJECT_FILES) | $({0}_TARGET_DIR)
	@echo Linking "{0}"
	$(VRBS)$(CXX) -o $({0}_TARGET_DIR)/$({0}_TARGET_NAME) $({0}_OBJECT_FILES) $({0}_LDFLAGS)
)delimiter",
                       makeTarget.view());

        builder.append(R"delimiter(
{0}_RUN: {0}_COMPILE
	$({0}_TARGET_DIR)/$({0}_TARGET_NAME)
)delimiter",
                       makeTarget.view());

        SmallString<32> escapedPath;
        SmallString<32> escapedName;
        for (const RenderItem& item : renderer.renderItems)
        {
            const StringView extension = RenderItem::getExtension(item.type);
            if (extension.isEmpty())
                continue;
            StringBuilder(escapedPath, StringBuilder::Clear).appendReplaceAll(item.path.view(), " ", "\\ ");
            StringView itemName = Path::basename(item.name.view(), extension);
            StringBuilder(escapedName, StringBuilder::Clear).appendReplaceAll(itemName, " ", "\\ ");

            if (item.type == RenderItem::CppFile or item.type == RenderItem::ObjCppFile)
            {
                builder.append(R"delimiter(
$({0}_INTERMEDIATE_DIR)/{1}.o: $(CURDIR_ESCAPED)/{2} | $({0}_INTERMEDIATE_DIR)
	@echo "Compiling {4}{3}"
	$(VRBS)$(CXX) $({0}_CXXFLAGS) -o "$@" -MMD -pthread $(call MJ_if_Clang) -c "$<"

    )delimiter",
                               makeTarget.view(), escapedName, escapedPath, extension, itemName);
            }
            else
            {
                builder.append(R"delimiter(
$({0}_INTERMEDIATE_DIR)/{4}.o: $(CURDIR_ESCAPED)/{2} | $({0}_INTERMEDIATE_DIR)
	@echo "Compiling {1}{3}"
	$(VRBS)$(CC) $({0}_CFLAGS) -o "$@" -MMD -pthread $(call MJ_if_Clang) -c "$<"

    )delimiter",
                               makeTarget.view(), escapedName, escapedPath, extension, itemName);
            }
        }

        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    [[nodiscard]] bool sanitizeName(StringView input, String& output)
    {
        // TODO: Actually implement name sanitization
        return output.assign(input);
    }

    [[nodiscard]] Result writeConfiguration(StringBuilder& builder, const Project& project,
                                            const Configuration&       configuration,
                                            const RelativeDirectories& relativeDirectories, StringView makeTarget)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        SmallString<255> configName;
        SC_TRY(sanitizeName(configuration.name.view(), configName)); // TODO: Sanitize the name
        builder.append("\n\nifeq ($(CONFIG),{0})\n", configName.view());

        {
            String        intermediate;
            StringBuilder intermediateBuilder(intermediate);

            WriterInternal::appendPrefixIfRelativePosix("$(CURDIR_ESCAPED)", intermediateBuilder,
                                                        configuration.intermediatesPath.view(),
                                                        relativeDirectories.relativeProjectsToIntermediates.view());

            SC_TRY(appendVariable(intermediateBuilder, configuration.intermediatesPath.view(), makeTarget,
                                  relativeDirectories));

            // Avoid Makefile warnings on intermediates and outputs directory creation.
            //
            // This happens when multiple projects define the same output or intermediates directory.
            // As the Makefile rule gets redefined for these targets, make prints a warning about it.
            // Here we are tracking if value for a previous _TARGET_DIR or _INTERMEDIATE_DIR was already
            // written (with the same value) to avoid re-defining it.
            // It will not work 100% of the times if the path string doesn't match 1:1 due for example to
            // the use of makefile variables but should handle most well written build files and common cases.

            String key;
            StringBuilder(key).format("{}_{}", intermediate, configName);
            if (intermediateDirectories.insertIfNotExists({key.view(), makeTarget}))
            {
                builder.append("{0}_INTERMEDIATE_DIR := ", makeTarget);
                builder.append(intermediate.view());
                builder.append("\n");

                builder.append(R"delimiter(
$({0}_INTERMEDIATE_DIR):
	@echo Creating "$({0}_INTERMEDIATE_DIR)"
	$(VRBS)mkdir -p "$@"

)delimiter",
                               makeTarget);
            }
            else if (makeTarget != intermediateDirectories.get(key.view())->view())
            {
                builder.append("{0}_INTERMEDIATE_DIR := $(", makeTarget);
                builder.append(intermediateDirectories.get(key.view())->view());
                builder.append("_INTERMEDIATE_DIR)\n");
            }
        }

        {
            String        output;
            StringBuilder outputBuilder(output);
            WriterInternal::appendPrefixIfRelativePosix("$(CURDIR_ESCAPED)", outputBuilder,
                                                        configuration.outputPath.view(),
                                                        relativeDirectories.relativeProjectsToOutputs.view());
            SC_TRY(appendVariable(outputBuilder, configuration.outputPath.view(), makeTarget, relativeDirectories));
            String key;
            StringBuilder(key).format("{}_{}", output, configName);
            if (outputDirectories.insertIfNotExists({key.view(), makeTarget}))
            {
                builder.append("{0}_TARGET_DIR := ", makeTarget);
                builder.append(output.view());
                builder.append("\n");

                builder.append(R"delimiter(
$({0}_TARGET_DIR):
	@echo Creating "$({0}_TARGET_DIR)"
	$(VRBS)mkdir -p "$@"

)delimiter",
                               makeTarget);
            }
            else if (makeTarget != outputDirectories.get(key.view())->view())
            {
                builder.append("{0}_TARGET_DIR := $(", makeTarget);
                builder.append(outputDirectories.get(key.view())->view());
                builder.append("_TARGET_DIR)\n");
            }
        }

        if (configuration.compile.enableASAN)
        {
            builder.append("\nifeq ($(TARGET_OS),iOS)");
            builder.append("\n{0}_SANITIZE_FLAGS :=", makeTarget);
            builder.append("\n{0}_NO_SANITIZE_FLAGS :=", makeTarget);
            builder.append("\nelse");
            builder.append("\n{0}_SANITIZE_FLAGS := -fsanitize=address,undefined",
                           makeTarget); // TODO: Split the UBSAN flag
            builder.append("\n{0}_NO_SANITIZE_FLAGS := -fno-sanitize=enum,return,float-divide-by-zero,function,vptr # "
                           "Needed on macOS x64",
                           makeTarget); // TODO: Split the UBSAN flag
            builder.append("\nendif");
        }
        else
        {
            builder.append("\n{0}_SANITIZE_FLAGS :=", makeTarget);
            builder.append("\n{0}_NO_SANITIZE_FLAGS :=", makeTarget);
        }

        // TODO: De-hardcode all these flags
        builder.append("\n{0}_VISIBILITY_FLAGS :="
                       " -fstrict-aliasing"
                       " -fvisibility=hidden",
                       makeTarget);
        builder.append("\n{0}_VISIBILITY_CXXFLAGS :="
                       " -fvisibility-inlines-hidden",
                       makeTarget);

        // TODO: De-hardcode debug and release optimization levels
        switch (configuration.compile.optimizationLevel)
        {
        case Optimization::Debug:
            builder.append("\n{0}_OPTIMIZATION_FLAGS := -D_DEBUG=1 -g -ggdb -O0", makeTarget);
            builder.append("\n{0}_OPTIMIZATION_LDFLAGS :=", makeTarget);
            break;
        case Optimization::Release:
            builder.append("\n{0}_OPTIMIZATION_FLAGS := -DNDEBUG=1 -O3", makeTarget);
            builder.append("\n{0}_OPTIMIZATION_LDFLAGS :=", makeTarget);
            break;
        }

        builder.append("\n{0}_CONFIG_FLAGS := $({0}_OPTIMIZATION_FLAGS) $({0}_SANITIZE_FLAGS) $({0}_VISIBILITY_FLAGS)",
                       makeTarget);
        builder.append("\n{0}_CONFIG_LDFLAGS := $({0}_OPTIMIZATION_LDFLAGS) $({0}_SANITIZE_FLAGS)", makeTarget);

        // TODO: De-hardcode -std=c++14
        builder.append("\n{0}_CONFIG_CXXFLAGS := $({0}_VISIBILITY_CXXFLAGS) -std=c++14", makeTarget);
        if (not resolve(project.compile, configuration.compile, &CompileFlags::enableStdCpp))
        {
            builder.append(" -nostdlib++");
        }

        if (not resolve(project.compile, configuration.compile, &CompileFlags::enableRTTI))
        {
            builder.append(" -fno-rtti");
        }

        if (not resolve(project.compile, configuration.compile, &CompileFlags::enableExceptions))
        {
            builder.append(" -fno-exceptions");
        }

        builder.append("\n\nifeq ($(CLANG_DETECTED),yes)\n");
        // Clang specific flags
        builder.append("{0}_CONFIG_COMPILER_FLAGS :=", makeTarget);

        if (resolve(project.compile, configuration.compile, &CompileFlags::enableCoverage))
        {
            builder.append(" -fprofile-instr-generate -fcoverage-mapping");
        }

        if (not resolve(project.compile, configuration.compile, &CompileFlags::enableStdCpp))
        {
            builder.append(" -nostdinc++");
        }

        {
            // The following prevent a linking error of the type
            // ...
            // Undefined symbols for architecture x86_64:
            //   "vtable for __cxxabiv1::__function_type_info", referenced from:
            //       typeinfo for void (SC::AlignedStorage<88, 8>&) in Async.o
            // ...
            // This happens on macOS (Intel only) with some combination of ASAN/UBSAN if standard library is not linked.
            // Note:
            // It's important that these flags come AFTER -fsanitize=address,undefined otherwise they will be overridden
            builder.append(" $({0}_NO_SANITIZE_FLAGS)", makeTarget);
        }
        builder.append("\n{0}_CONFIG_COMPILER_LDFLAGS :=", makeTarget);
        if (resolve(project.compile, configuration.compile, &CompileFlags::enableCoverage))
        {
            builder.append(" -fprofile-instr-generate -fcoverage-mapping");
        }
        builder.append(" $({0}_NO_SANITIZE_FLAGS)", makeTarget);

        builder.append("\nelse\n");
        // Non Clang specific flags
        builder.append("{0}_CONFIG_COMPILER_FLAGS :=", makeTarget);
        builder.append("\n{0}_CONFIG_COMPILER_LDFLAGS :=", makeTarget);

        builder.append("\nendif\n");
        builder.append("\nendif");

        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    [[nodiscard]] static bool appendVariable(StringBuilder& builder, StringView text, StringView makeTarget,
                                             const RelativeDirectories& relativeDirectories)
    {
        const StringView relativeRoot = relativeDirectories.projectRootRelativeToProjects.view();

        const StringBuilder::ReplacePair replacements[] = {
            {"$(PROJECT_DIR)", "$(CURDIR)"},                       //
            {"$(PROJECT_ROOT)", relativeRoot},                     //
            {"$(CONFIGURATION)", "$(CONFIG)"},                     //
            {"$(PROJECT_NAME)", makeTarget},                       //
            {"$(TARGET_OS)", "$(TARGET_OS)"},                      //
            {"$(TARGET_OS_VERSION)", "$(TARGET_OS_VERSION)"},      //
            {"$(TARGET_ARCHITECTURES)", "$(TARGET_ARCHITECTURE)"}, //
            {"$(BUILD_SYSTEM)", "make"},                           //
            {"$(COMPILER)", "$(COMPILER_TYPE)"},                   //
            {"$(COMPILER_VERSION)", "$(COMPILER_VERSION)"},        //
            {"\"", "\\\""},                                        // Escape double quotes
        };
        return builder.appendReplaceMultiple(text, {replacements, sizeof(replacements) / sizeof(replacements[0])});
    }
};
