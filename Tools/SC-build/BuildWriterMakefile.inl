// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Build.h"

#include "../../Libraries/Strings/StringBuilder.h"
#include "BuildWriter.h"

struct SC::Build::ProjectWriter::WriterMakefile
{
    const Definition&        definition;
    const FilePathsResolver& filePathsResolver;
    const Directories&       directories;

    VectorMap<String, String> outputDirectories;
    VectorMap<String, String> intermediateDirectories;

    WriterMakefile(const Definition& definition, const FilePathsResolver& filePathsResolver,
                   const Directories& directories)
        : definition(definition), filePathsResolver(filePathsResolver), directories(directories)
    {}
    using RenderItem  = WriterInternal::RenderItem;
    using RenderGroup = WriterInternal::RenderGroup;
    using Renderer    = WriterInternal::Renderer;

    Result writeMakefile(StringBuilder& builder, const Workspace& workspace, Renderer& renderer)
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
            builder.append(" {0}_COMPILE_COMMANDS {0}", makeTarget);
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
            SC_TRY(WriterInternal::renderProject(directories.projectsDirectory.view(), project, filePathsResolver,
                                                 renderer.renderItems));
            SC_TRY(writeProject(builder, project, renderer, relativeDirectories));
        }

        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    Result writeProject(StringBuilder& builder, const Project& project, const Renderer& renderer,
                        const RelativeDirectories& relativeDirectories)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        SmallString<255> makeTarget;
        SC_TRY(sanitizeName(project.targetName.view(), makeTarget));

        writeTargetRule(builder, makeTarget.view());

        SC_TRY_MSG(not project.configurations.isEmpty(), "Needs at least one configuration");
        bool first = true;
        for (const Configuration& configuration : project.configurations)
        {
            SmallString<255> configName;
            SC_TRY(sanitizeName(configuration.name.view(), configName));
            if (first)
            {
                builder.append("\n\nifeq ($(CONFIG),{0})\n", configName.view());
                first = false;
            }
            else
            {
                builder.append("\n\nelse ifeq ($(CONFIG),{0})\n", configName.view());
            }
            SC_TRY(writeConfiguration(builder, project, configuration, relativeDirectories, makeTarget.view(),
                                      configName.view()))

            writePerFileConfiguration(builder, project, configuration, relativeDirectories, makeTarget.view());
        }

        builder.append(R"delimiter(

else

ifneq ($(filter {0}_% all compile run print-executable-paths,$(MAKECMDGOALS)),)
$(error "CONFIG = '$(CONFIG)' is unsupported on '$(MAKECMDGOALS)' because '{0}' does not have such configuration")
endif
endif # $(CONFIG)
)delimiter",
                       makeTarget.view());

        writeMergedCompileFlags(builder, makeTarget.view());
        writeTargetFlags(builder, makeTarget.view());
        writeLinkerFlags(builder, makeTarget.view(), project.link);

        // Rules
        writeCleanRule(builder, makeTarget.view());
        writeObjectFilesList(builder, makeTarget.view(), renderer);
        writeRebuildOnHeaderChangeRule(builder, makeTarget.view());
        writeCompileCommandsJsonRule(builder, makeTarget.view());

        writeLinkExecutableRule(builder, makeTarget.view());
        writeRunExecutableRule(builder, makeTarget.view());
        writeSourceFilesList(builder, makeTarget.view(), renderer, project.filesWithSpecificFlags);
        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    Result writePerFileConfiguration(StringBuilder& builder, const Project& project, const Configuration& configuration,
                                     const RelativeDirectories& relativeDirectories, StringView makeTarget)
    {
        // This is actually the most precise (in the sense of "correct") backend implementation "per file" flags.
        // All flags are being rewritten merging project, configuration and file specific flags.
        // All files in the selection will share the same _GROUP_ variables to keep the makefile short and readable.
        int index = 0;
        for (const SourceFiles& sourceFiles : project.filesWithSpecificFlags)
        {
            String perFileTarget;
            SC_TRY(StringBuilder::format(perFileTarget, "{0}_GROUP_{1}", makeTarget, index));
            index++;
            CompileFlags        compileFlags;
            const CompileFlags* compileSources[] = {&sourceFiles.compile, &configuration.compile,
                                                    &project.files.compile};
            SC_TRY(CompileFlags::merge(compileSources, compileFlags));
            SC_TRY(writeCompileFlags(builder, perFileTarget.view(), relativeDirectories, compileFlags));
            writeMergedCompileFlags(builder, perFileTarget.view());
        }
        return Result(true);
    }

    Result sanitizeName(StringView input, String& output)
    {
        SC_TRY_MSG(not input.isEmpty(), "Project name is empty");
        // TODO: Actually implement name sanitization
        if (not StringBuilder(output, StringBuilder::Clear).appendReplaceAll(input, ".", "_"))
        {
            return Result::Error("sanitizeName");
        }
        return Result(true);
    }

    void writeTargetRule(StringBuilder& builder, StringView makeTarget)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
# {0} Target
{0}_TARGET_NAME := {0}

{0}_PRINT_EXECUTABLE_PATH:
	@echo $({0}_TARGET_DIR)/$({0}_TARGET_NAME)

)delimiter",
                       makeTarget);
        SC_COMPILER_WARNING_POP;
    }

    void writeCompileCommandsJsonRule(StringBuilder& builder, StringView makeTarget)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;

        builder.append(R"delimiter(

{0}: $({0}_TARGET_DIR)/$({0}_TARGET_NAME)

ifeq ($(CLANG_DETECTED),yes)
{0}_COMPILE_COMMANDS: $({0}_INTERMEDIATE_DIR)/compile_commands.json
else
# On GCC generating compile_commands.json is not supported but it's expected for _COMPILE_COMMANDS to compile the executable too
{0}_COMPILE_COMMANDS: $({0}_TARGET_DIR)/$({0}_TARGET_NAME)
endif

$({0}_INTERMEDIATE_DIR)/compile_commands.json: $({0}_TARGET_DIR)/$({0}_TARGET_NAME)
	@echo Writing {0} compile_commands.json
ifeq ($(TARGET_OS),linux)    
	$(VRBS)sed -e '1s/^/[\n/' -e '$$s/,$$/\n]/' $({0}_INTERMEDIATE_DIR)/*.o.json > $({0}_INTERMEDIATE_DIR)/compile_commands.json
else
	$(VRBS)sed -e '1s/^/[\'$$'\n''/' -e '$$s/,$$/\'$$'\n'']/' $({0}_INTERMEDIATE_DIR)/*.o.json > $({0}_INTERMEDIATE_DIR)/compile_commands.json
endif
)delimiter",
                       makeTarget);
        SC_COMPILER_WARNING_POP;
    }

    void writeLinkExecutableRule(StringBuilder& builder, StringView makeTarget)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;

        builder.append(R"delimiter(
$({0}_TARGET_DIR)/$({0}_TARGET_NAME): $({0}_OBJECT_FILES) | $({0}_TARGET_DIR)
	@echo Linking "{0}"
	$(VRBS)$(CXX) -o $({0}_TARGET_DIR)/$({0}_TARGET_NAME) $({0}_OBJECT_FILES) $({0}_LDFLAGS)
)delimiter",
                       makeTarget);
        SC_COMPILER_WARNING_POP;
    }

    void writeRunExecutableRule(StringBuilder& builder, StringView makeTarget)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;

        builder.append(R"delimiter(
{0}_RUN: {0}
	$({0}_TARGET_DIR)/$({0}_TARGET_NAME)
)delimiter",
                       makeTarget);
        SC_COMPILER_WARNING_POP;
    }

    void writeSourceFilesList(StringBuilder& builder, StringView makeTarget, const Renderer& renderer,
                              const Vector<SourceFiles>& filesWithSpecificFlags)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
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
            StringView flagsGroup = "";
            String     buffer;
            if (item.compileFlags != nullptr)
            {
                size_t index;
                if (filesWithSpecificFlags.find([&](const SourceFiles& it) { return &it.compile == item.compileFlags; },
                                                &index))
                {
                    StringBuilder::format(buffer, "_GROUP_{0}", index);
                    flagsGroup = buffer.view();
                }
            }
            if (item.type == RenderItem::CppFile or item.type == RenderItem::ObjCppFile)
            {
                builder.append(R"delimiter(
$({0}_INTERMEDIATE_DIR)/{1}.o: $(CURDIR_ESCAPED)/{2} | $({0}_INTERMEDIATE_DIR)
	@echo "Compiling {4}{3}"
	$(VRBS)$(CXX) $({0}_TARGET_CPPFLAGS) $({0}{5}_CXXFLAGS) -o "$@" -MMD -pthread $(call MJ_if_Clang) -c "$<"

    )delimiter",
                               makeTarget, escapedName, escapedPath, extension, itemName, flagsGroup);
            }
            else
            {
                builder.append(R"delimiter(
$({0}_INTERMEDIATE_DIR)/{4}.o: $(CURDIR_ESCAPED)/{2} | $({0}_INTERMEDIATE_DIR)
	@echo "Compiling {1}{3}"
	$(VRBS)$(CC) $({0}_TARGET_CPPFLAGS) $({0}{5}_CFLAGS) -o "$@" -MMD -pthread $(call MJ_if_Clang) -c "$<"

    )delimiter",
                               makeTarget, escapedName, escapedPath, extension, itemName, flagsGroup);
            }
        }
        SC_COMPILER_WARNING_POP;
    }

    void writeLinkerFlags(StringBuilder& builder, StringView makeTarget, const LinkFlags& link)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("\n{0}_FRAMEWORKS_ANY :=", makeTarget);
        {
            for (const String& it : link.frameworks)
            {
                builder.append(" -framework {0}", it.view());
            }
        }
        builder.append("\n{0}_FRAMEWORKS_MACOS :=", makeTarget);
        {
            for (const String& it : link.frameworksMacOS)
            {
                builder.append(" -framework {0}", it.view());
            }
        }
        builder.append("\n{0}_FRAMEWORKS_IOS :=", makeTarget);
        {
            for (const String& it : link.frameworksIOS)
            {
                builder.append(" -framework {0}", it.view());
            }
        }

        builder.append("\nifeq ($(TARGET_OS),macOS)\n");
        builder.append("     {0}_FRAMEWORKS := $({0}_FRAMEWORKS_ANY) $({0}_FRAMEWORKS_MACOS)\n", makeTarget);
        builder.append("else\n");
        builder.append("     {0}_FRAMEWORKS := $({0}_FRAMEWORKS_ANY) $({0}_FRAMEWORKS_IOS)\n", makeTarget);
        builder.append("endif\n");

        builder.append("\n{0}_LIBRARIES :=", makeTarget);
        {
            for (const String& it : link.libraries)
            {
                builder.append(" -l{}", it.view());
            }
        }

        builder.append("\nifeq ($(TARGET_OS),macOS)\n");
        builder.append("     {0}_OS_LDFLAGS := $({0}_FRAMEWORKS)\n", makeTarget);
        builder.append("else ifeq ($(TARGET_OS),iOS)\n");
        builder.append("     {0}_OS_LDFLAGS := $({0}_FRAMEWORKS)\n", makeTarget);
        builder.append("else ifeq ($(TARGET_OS),linux)\n");
        // -rdynamic is needed to resolve Plugin symbols in the executable
        builder.append("     {0}_OS_LDFLAGS := -rdynamic\n", makeTarget);
        builder.append("else\n");
        builder.append("     {0}_OS_LDFLAGS :=\n", makeTarget);
        builder.append("endif\n");

        builder.append("\n{0}_CONFIG_LDFLAGS := $({0}_SANITIZE_CPPFLAGS) $({0}_COMPILER_LDFLAGS)", makeTarget);
        builder.append("\n{0}_LDFLAGS := $({0}_TARGET_CPPFLAGS) $({0}_CONFIG_LDFLAGS) $({0}_LIBRARIES) "
                       "$({0}_OS_LDFLAGS) $(LDFLAGS)",
                       makeTarget);
        SC_COMPILER_WARNING_POP;
    }

    void writeCleanRule(StringBuilder& builder, StringView makeTarget)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
ifneq ($(and $({0}_TARGET_DIR),$({0}_INTERMEDIATE_DIR)),)
{0}_CLEAN:
	@echo Cleaning {0}
	$(VRBS)rm -rf $({0}_TARGET_DIR)/$(TARGET) $({0}_INTERMEDIATE_DIR)
else
{0}_CLEAN:
	@echo "Cleaning {0} (skipped for config '$(CONFIG)')"
endif
)delimiter",
                       makeTarget);

        SC_COMPILER_WARNING_POP;
    }

    void writeObjectFilesList(StringBuilder& builder, StringView makeTarget, const Renderer& renderer)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("{0}_OBJECT_FILES := \\", makeTarget);
        for (const RenderItem& item : renderer.renderItems)
        {
            const StringView extension = RenderItem::getExtension(item.type);
            if (extension.isEmpty())
                continue;
            builder.append("\n$({0}_INTERMEDIATE_DIR)/", makeTarget);
            builder.appendReplaceAll(Path::basename(item.name.view(), extension), " ", "\\ ");
            builder.append(".o \\");
        }
        SC_COMPILER_WARNING_POP;
    }

    void writeRebuildOnHeaderChangeRule(StringBuilder& builder, StringView makeTarget)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(

# Rebuild object files when an header dependency changes
-include $({0}_OBJECT_FILES:.o=.d)
)delimiter",
                       makeTarget);

        SC_COMPILER_WARNING_POP;
    }

    void appendWarnings(StringBuilder& builder, StringView makeTarget, const CompileFlags& compile)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        // TODO: On GCC we need to enable also the following fixing the warnings
        // -W error=conversion
        // -W shadow
        // -W sign-compare
        // -W error=sign-conversion
        // -W missing-field-initializers

        builder.append("\n{0}_WARNING_CXXFLAGS :=-Wnon-virtual-dtor -Woverloaded-virtual", makeTarget);

        builder.append("\n{0}_WARNING_CPPFLAGS :=-Werror -Werror=return-type -Wunreachable-code "
                       "-Wmissing-braces -Wparentheses -Wswitch -Wunused-function -Wunused-label "
                       "-Wunused-parameter -Wunused-variable -Wunused-value -Wempty-body -Wuninitialized "
                       "-Wunknown-pragmas -Wenum-conversion -Werror=float-conversion -Werror=implicit-fallthrough",
                       makeTarget);
        for (const Warning& warning : compile.warnings)
        {
            // TODO: Differentiate between Clang and GCC warnings
            if (warning.state == Warning::Disabled and warning.type != Warning::MSVCWarning)
            {
                builder.append(" -Wno-{0}", warning.name);
            }
        }
        SC_COMPILER_WARNING_POP;
    }

    Result appendDefines(StringBuilder& builder, StringView makeTarget, const RelativeDirectories& relativeDirectories,
                         const CompileFlags& compile)
    {
        SC_TRY(builder.append("\n{0}_DEFINES :=", makeTarget));
        for (const String& it : compile.defines)
        {
            SC_TRY(builder.append(" \"-D"));
            SC_TRY(appendVariable(builder, it.view(), makeTarget, relativeDirectories));
            SC_TRY(builder.append("\""));
        }
        return Result(true);
    }

    Result appendIncludes(StringBuilder& builder, StringView makeTarget, const RelativeDirectories& relativeDirectories,
                          const CompileFlags& compile)
    {
        SC_TRY(builder.append("\n{0}_INCLUDE_PATHS :=", makeTarget));
        for (const String& it : compile.includePaths)
        {
            SC_TRY(builder.append(" \"-I"));
            if (Path::isAbsolute(it.view(), Path::AsNative))
            {
                String relative;
                SC_TRY(Path::relativeFromTo(relative, directories.projectsDirectory.view(), it.view(), Path::AsNative));
                SC_TRY(builder.append("$(CURDIR)/{}", relative));
            }
            else
            {
                SC_TRY(builder.append("$(CURDIR)/{}/{}", relativeDirectories.relativeProjectsToProjectRoot, it.view()));
            }
            SC_TRY(builder.append("\""));
        }
        return Result(true);
    }
    Result appendSanitizeFlags(StringBuilder& builder, StringView makeTarget, const CompileFlags& compileFlags)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("\nifeq ($(TARGET_OS),iOS)");
        builder.append("\n{0}_SANITIZE_CPPFLAGS :=", makeTarget);
        builder.append("\n{0}_NO_SANITIZE_CPPFLAGS :=", makeTarget);
        builder.append("\nelse");
        if (compileFlags.enableASAN)
        {
            // TODO: Split the UBSAN flag
            builder.append("\n{0}_SANITIZE_CPPFLAGS := -fsanitize=address,undefined", makeTarget);
            builder.append("\n{0}_NO_SANITIZE_CPPFLAGS := -fno-sanitize=enum,return,float-divide-by-zero,function,vptr "
                           "# Needed on macOS x64",
                           makeTarget);
        }
        else
        {
            builder.append("\n{0}_SANITIZE_CPPFLAGS :=", makeTarget);
            builder.append("\n{0}_NO_SANITIZE_CPPFLAGS :=", makeTarget);
        }
        builder.append("\nendif");
        return Result(true);
        SC_COMPILER_WARNING_POP;
    }

    Result appendCommonFlags(StringBuilder& builder, StringView makeTarget, const CompileFlags& compileFlags)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;

        // TODO: De-hardcode -std=c++14
        builder.append("\n{0}_COMMON_CXXFLAGS := -std=c++14", makeTarget);

        if (not compileFlags.enableRTTI)
        {
            builder.append(" -fno-rtti");
        }

        if (not compileFlags.enableExceptions)
        {
            builder.append(" -fno-exceptions");
        }

        // TODO: De-hardcode visibility flags
        builder.append("\n{0}_VISIBILITY_CPPFLAGS := -fvisibility=hidden", makeTarget);
        builder.append("\n{0}_VISIBILITY_CXXFLAGS := -fvisibility-inlines-hidden", makeTarget);

        // TODO: De-hardcode debug and release optimization levels and aliasing
        switch (compileFlags.optimizationLevel)
        {
        case Optimization::Debug:
            builder.append("\n{0}_OPTIMIZATION_CPPFLAGS := -D_DEBUG=1 -g -ggdb -O0 -fstrict-aliasing", makeTarget);
            break;
        case Optimization::Release:
            builder.append("\n{0}_OPTIMIZATION_CPPFLAGS := -DNDEBUG=1 -O3 -fstrict-aliasing", makeTarget);
            break;
        }
        return Result(true);
        SC_COMPILER_WARNING_POP;
    }

    Result appendCompilerFlags(StringBuilder& builder, StringView makeTarget, const CompileFlags& compileFlags)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("\n\nifeq ($(CLANG_DETECTED),yes)\n");
        // Clang specific flags
        builder.append("{0}_COMPILER_CPPFLAGS :=", makeTarget);

        if (compileFlags.enableCoverage)
        {
            builder.append(" -fprofile-instr-generate -fcoverage-mapping");
        }

        {
            // The following prevent a linking error of the type
            // ...
            // Undefined symbols for architecture x86_64:
            //   "vtable for __cxxabiv1::__function_type_info", referenced from:
            //       typeinfo for void (SC::AlignedStorage<88, 8>&) in Async.o
            // ...
            // This happens on macOS (Intel only) with some combination of ASAN/UBSAN if standard library is not
            // linked. Note: It's important that these flags come AFTER -fsanitize=address,undefined otherwise they
            // will be overridden
            builder.append(" $({0}_NO_SANITIZE_CPPFLAGS)", makeTarget);
        }
        builder.append("\n{0}_COMPILER_CXXFLAGS :=", makeTarget);
        if (not compileFlags.enableStdCpp)
        {
            builder.append(" -nostdinc++");
        }
        builder.append(" $({0}_NO_SANITIZE_CPPFLAGS)", makeTarget);

        builder.append("\nelse");
        // Non Clang specific flags
        builder.append("\n{0}_COMPILER_CPPFLAGS :=", makeTarget);
        builder.append("\n{0}_COMPILER_CXXFLAGS :=", makeTarget);
        builder.append(" -DSC_COMPILER_ENABLE_STD_CPP=1"); // Only GCC 13+ supports nostdlib++
        builder.append("\nendif");

        return Result(true);
        SC_COMPILER_WARNING_POP;
    }

    Result appendCompilerLinkFlags(StringBuilder& builder, StringView makeTarget, const CompileFlags& compileFlags)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("\n\nifeq ($(CLANG_DETECTED),yes)");
        // Clang specific flags
        builder.append("\n{0}_COMPILER_LDFLAGS :=", makeTarget);
        if (compileFlags.enableCoverage)
        {
            builder.append(" -fprofile-instr-generate -fcoverage-mapping");
        }
        builder.append(" $({0}_NO_SANITIZE_CPPFLAGS)", makeTarget);

        if (not compileFlags.enableStdCpp)
        {
            // We still need to figure out how to make nostdlib++ work on Clang / Linux
            builder.append("\nifneq ($(TARGET_OS),linux)");
            builder.append("\n{0}_COMPILER_LDFLAGS += -nostdlib++", makeTarget); // This is only Clang and GCC 13+
            builder.append("\nendif");
        }
        builder.append("\nelse");
        // Non Clang specific flags
        builder.append("\n{0}_COMPILER_LDFLAGS :=", makeTarget);
        builder.append("\nendif");
        return Result(true);
        SC_COMPILER_WARNING_POP;
    }

    Result appendIntermediateDir(StringBuilder& builder, StringView makeTarget,
                                 const RelativeDirectories& relativeDirectories, StringView configName,
                                 StringView intermediatesPath)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        String        intermediate;
        StringBuilder intermediateBuilder(intermediate);

        WriterInternal::appendPrefixIfRelativePosix("$(CURDIR_ESCAPED)", intermediateBuilder, intermediatesPath,
                                                    relativeDirectories.relativeProjectsToIntermediates.view());

        SC_TRY(appendVariable(intermediateBuilder, intermediatesPath, makeTarget, relativeDirectories));
        intermediateBuilder.finalize();
        // Avoid Makefile warnings on intermediates and outputs directory creation.
        //
        // This happens when multiple projects define the same output or intermediates directory.
        // As the Makefile rule gets redefined for these targets, make prints a warning about it.
        // Here we are tracking if value for a previous _TARGET_DIR or _INTERMEDIATE_DIR was already
        // written (with the same value) to avoid re-defining it.
        // It will not work 100% of the times if the path string doesn't match 1:1 due for example to
        // the use of makefile variables but should handle most well written build files and common cases.

        String key;
        StringBuilder::format(key, "{}_{}", intermediate, configName);
        if (intermediateDirectories.insertIfNotExists({key.view(), makeTarget}))
        {
            builder.append("\n{0}_INTERMEDIATE_DIR := ", makeTarget);
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
            builder.append("\n{0}_INTERMEDIATE_DIR := $(", makeTarget);
            builder.append(intermediateDirectories.get(key.view())->view());
            builder.append("_INTERMEDIATE_DIR)");
        }
        return Result(true);
        SC_COMPILER_WARNING_POP;
    }

    Result appendTargetDir(StringBuilder& builder, StringView makeTarget,
                           const RelativeDirectories& relativeDirectories, StringView configName, StringView outputPath)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        String        output;
        StringBuilder outputBuilder(output);
        WriterInternal::appendPrefixIfRelativePosix("$(CURDIR_ESCAPED)", outputBuilder, outputPath,
                                                    relativeDirectories.relativeProjectsToOutputs.view());
        SC_TRY(appendVariable(outputBuilder, outputPath, makeTarget, relativeDirectories));
        outputBuilder.finalize();
        String key;
        StringBuilder::format(key, "{}_{}", output, configName);
        if (outputDirectories.insertIfNotExists({key.view(), makeTarget}))
        {
            builder.append("\n{0}_TARGET_DIR := ", makeTarget);
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
            builder.append("\n{0}_TARGET_DIR := $(", makeTarget);
            builder.append(outputDirectories.get(key.view())->view());
            builder.append("_TARGET_DIR)");
        }
        return Result(true);
        SC_COMPILER_WARNING_POP;
    }

    Result writeConfiguration(StringBuilder& builder, const Project& project, const Configuration& configuration,
                              const RelativeDirectories& relativeDirectories, StringView makeTarget,
                              StringView configName)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        CompileFlags        compileFlags;
        const CompileFlags* compileSources[] = {&configuration.compile, &project.files.compile};
        SC_TRY(CompileFlags::merge(compileSources, compileFlags));
        if (compileFlags.enableCoverage)
        {
            builder.append(
                R"delimiter(
ifeq ($(CLANG_DETECTED),yes)
else
$(error "Coverage is supported only when using clang")
endif
            )delimiter");
        }
        appendIntermediateDir(builder, makeTarget, relativeDirectories, configName,
                              configuration.intermediatesPath.view());
        appendTargetDir(builder, makeTarget, relativeDirectories, configName, configuration.outputPath.view());
        writeCompileFlags(builder, makeTarget, relativeDirectories, compileFlags);
        appendCompilerLinkFlags(builder, makeTarget, compileFlags);

        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    Result writeCompileFlags(StringBuilder& builder, StringView makeTarget,
                             const RelativeDirectories& relativeDirectories, const CompileFlags& compileFlags)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        appendDefines(builder, makeTarget, relativeDirectories, compileFlags);
        appendIncludes(builder, makeTarget, relativeDirectories, compileFlags);
        appendWarnings(builder, makeTarget, compileFlags);
        appendSanitizeFlags(builder, makeTarget, compileFlags);
        appendCommonFlags(builder, makeTarget, compileFlags);
        appendCompilerFlags(builder, makeTarget, compileFlags);
        return Result(true);
        SC_COMPILER_WARNING_POP;
    }

    static bool writeMergedCompileFlags(StringBuilder& builder, StringView makeTarget)
    {
        return builder.append(R"delimiter(
{0}_CONFIG_CPPFLAGS := $({0}_COMPILER_CPPFLAGS) $({0}_VISIBILITY_CPPFLAGS) $({0}_WARNING_CPPFLAGS) $({0}_OPTIMIZATION_CPPFLAGS) $({0}_SANITIZE_CPPFLAGS) $({0}_DEFINES) $({0}_INCLUDE_PATHS)
{0}_CONFIG_CXXFLAGS := $({0}_COMMON_CXXFLAGS) $({0}_COMPILER_CXXFLAGS) $({0}_VISIBILITY_CXXFLAGS) $({0}_WARNING_CXXFLAGS)

# Flags for both .c and .cpp files
{0}_CPPFLAGS := $({0}_CONFIG_CPPFLAGS) $(CPPFLAGS)

# Flags for .c files
{0}_CFLAGS := $({0}_CPPFLAGS) $(CFLAGS)

# Flags for .cpp files
{0}_CXXFLAGS := $({0}_CPPFLAGS) $({0}_CONFIG_CXXFLAGS) $(CXXFLAGS)
)delimiter",
                              makeTarget);
    }

    static bool writeTargetFlags(StringBuilder& builder, StringView makeTarget)
    {
        return builder.append(R"delimiter(
# Cross-compile support
ifeq ($(CLANG_DETECTED),yes)
ifeq ($(TARGET_OS),macOS)
ifeq ($(TARGET_ARCHITECTURE),arm64)
{0}_TARGET_CPPFLAGS := -target arm64-apple-macos11
else
{0}_TARGET_CPPFLAGS := -target x86_64-apple-macos11
endif # TARGET_ARCHITECTURE
endif # TARGET_OS
endif # CLANG_DETECTED

ifeq ($({0}_TARGET_CPPFLAGS),)
ifneq ($(HOST_ARCHITECTURE),$(TARGET_ARCHITECTURE))
$(error "Cross-compiling TARGET_ARCHITECTURE = $(TARGET_ARCHITECTURE) is unsupported")
endif
endif
)delimiter",
                              makeTarget);
    }

    [[nodiscard]] static bool appendVariable(StringBuilder& builder, StringView text, StringView makeTarget,
                                             const RelativeDirectories& relativeDirectories)
    {
        const StringView relativeRoot = relativeDirectories.projectRootRelativeToProjects.view();

        const ReplacePair replacements[] = {
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
        return appendReplaceMultiple(builder, text, {replacements, sizeof(replacements) / sizeof(replacements[0])});
    }
};
