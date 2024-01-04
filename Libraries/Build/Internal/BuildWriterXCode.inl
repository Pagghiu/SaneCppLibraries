// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../FileSystemIterator/FileSystemIterator.h"
#include "../../Hashing/Hashing.h"
#include "../../Strings/StringBuilder.h"
#include "../Build.h"
#include "BuildWriter.h"

struct SC::Build::ProjectWriter::WriterXCode
{
    const Definition&         definition;
    const DefinitionCompiler& definitionCompiler;
    Hashing                   hashing;

    WriterXCode(const Definition& definition, const DefinitionCompiler& definitionCompiler)
        : definition(definition), definitionCompiler(definitionCompiler)
    {}
    using RenderItem  = WriterInternal::RenderItem;
    using RenderGroup = WriterInternal::RenderGroup;
    using Renderer    = WriterInternal::Renderer;

    [[nodiscard]] bool prepare(StringView destinationDirectory, const Project& project, Renderer& renderer)
    {
        SC_TRY(fillXCodeFiles(destinationDirectory, project, renderer.renderItems));
        SC_TRY(fillXCodeFrameworks(project, renderer.renderItems));
        SC_TRY(fillXCodeConfigurations(project, renderer.renderItems));
        SC_TRY(fillFileGroups(renderer.rootGroup, renderer.renderItems));
        SC_TRY(fillProductGroup(project, renderer.rootGroup));
        SC_TRY(fillFrameworkGroup(project, renderer.rootGroup, renderer.renderItems));
        return true;
    }

    [[nodiscard]] bool computeReferenceHash(StringView name, String& hash)
    {
        SC_TRY(hashing.setType(Hashing::TypeSHA1));
        SC_TRY(hashing.update("reference_"_a8.toBytesSpan()));
        SC_TRY(hashing.update(name.toBytesSpan()));
        Hashing::Result res;
        SC_TRY(hashing.finalize(res));
        SmallString<64> tmpHash = StringEncoding::Ascii;
        SC_TRY(StringBuilder(tmpHash).appendHex(res.toBytesSpan()));
        return hash.assign(tmpHash.view().sliceStartLength(0, 24));
    }

    [[nodiscard]] bool computeBuildHash(StringView name, String& hash)
    {
        SC_TRY(hashing.setType(Hashing::TypeSHA1));
        SC_TRY(hashing.update("build_"_a8.toBytesSpan()));
        SC_TRY(hashing.update(name.toBytesSpan()));
        Hashing::Result res;
        SC_TRY(hashing.finalize(res));
        SmallString<64> tmpHash = StringEncoding::Ascii;
        SC_TRY(StringBuilder(tmpHash).appendHex(res.toBytesSpan()));
        return hash.assign(tmpHash.view().sliceStartLength(0, 24));
    }

    [[nodiscard]] bool fillFileGroups(RenderGroup& group, const Vector<RenderItem>& xcodeFiles)
    {
        SC_TRY(group.referenceHash.assign("7B0074092A73143F00660B94"));
        SC_TRY(group.name.assign("/"));
        for (const auto& file : xcodeFiles)
        {
            StringViewTokenizer tokenizer(file.referencePath.view());
            RenderGroup*        current = &group;
            while (tokenizer.tokenizeNext('/', StringViewTokenizer::SkipEmpty))
            {
                current = current->children.getOrCreate(tokenizer.component);
                if (current->name.isEmpty())
                {
                    current->name = tokenizer.component;
                    if (tokenizer.isFinished())
                    {
                        // for the leafs we hash just the name
                        SC_TRY(computeReferenceHash(tokenizer.component, current->referenceHash));
                    }
                    else
                    {
                        SC_TRY(computeReferenceHash(tokenizer.processed, current->referenceHash));
                    }
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool fillProductGroup(const Project& project, RenderGroup& group)
    {
        auto products = group.children.getOrCreate("Products"_a8);
        SC_TRY(products != nullptr);
        SC_TRY(products->name.assign("Products"));
        SC_TRY(products->referenceHash.assign("7B0074132A73143F00660B94"));
        auto test = products->children.getOrCreate(project.targetName.view());
        SC_TRY(test != nullptr);
        SC_TRY(test->name.assign(project.targetName.view()));
        SC_TRY(test->referenceHash.assign("7B0074122A73143F00660B94"));
        return true;
    }

    [[nodiscard]] bool fillFrameworkGroup(const Project&, RenderGroup& group, Vector<RenderItem>& xcodeFiles)
    {
        auto frameworksGroup = group.children.getOrCreate("Frameworks"_a8);
        SC_TRY(frameworksGroup != nullptr);
        SC_TRY(frameworksGroup->name.assign("Frameworks"));
        SC_TRY(frameworksGroup->referenceHash.assign("7B3D0EF12A74DEEF00AE03EE"));

        for (auto it : xcodeFiles)
        {
            if (it.type == RenderItem::Framework)
            {
                auto framework = frameworksGroup->children.getOrCreate(it.name);
                SC_TRY(framework != nullptr);
                SC_TRY(framework->name.assign(it.name.view()));
                SC_TRY(framework->referenceHash.assign(it.referenceHash.view()));
            }
        }
        return true;
    }

    [[nodiscard]] bool printGroupRecursive(StringBuilder& builder, const RenderGroup& parentGroup)
    {
        if (parentGroup.name == "/")
        {
            SC_TRY(builder.append("        {} = {{\n", parentGroup.referenceHash));
        }
        else
        {
            SC_TRY(builder.append("        {} /* {} */ = {{\n", parentGroup.referenceHash, parentGroup.name));
        }

        SC_TRY(builder.append("            isa = PBXGroup;\n"));
        SC_TRY(builder.append("            children = (\n"));
        for (const auto& group : parentGroup.children)
        {
            SC_TRY(builder.append("                {} /* {} */,\n", group.value.referenceHash, group.value.name));
        }
        SC_TRY(builder.append("            );\n"));
        if (parentGroup.name != "/")
        {
            SC_TRY(builder.append("            name = {};\n", parentGroup.name));
        }
        SC_TRY(builder.append("            sourceTree = \"<group>\";\n"));
        SC_TRY(builder.append("        };\n"));
        for (const auto& group : parentGroup.children)
        {
            if (not group.value.children.isEmpty())
            {
                SC_TRY(printGroupRecursive(builder, group.value));
            }
        }
        return true;
    }

    [[nodiscard]] bool writePBXBuildFile(StringBuilder& builder, const Vector<RenderItem>& xcodeFiles)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXBuildFile section */
)delimiter");
        for (auto& file : xcodeFiles)
        {
            if (file.type == RenderItem::CppFile)
            {
                builder.append("        {} /* {} in Sources */ = {{isa = PBXBuildFile; fileRef = {} /* {} */; }};\n",
                               file.buildHash, file.name, file.referenceHash, file.name);
            }
            else if (file.type == RenderItem::Framework)
            {

                builder.append("        {} /* {} in Frameworks */ = {{isa = PBXBuildFile; fileRef = {} /* {} */; }};\n",
                               file.buildHash, file.name, file.referenceHash, file.name);
            }
        }

        builder.append(R"delimiter(/* End PBXBuildFile section */
)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writePBXCopyFilesBuildPhase(StringBuilder& builder)
    {
        return builder.append(
            R"delimiter(
/* Begin PBXCopyFilesBuildPhase section */
        7B0074102A73143F00660B94 /* CopyFiles */ = {
            isa = PBXCopyFilesBuildPhase;
            buildActionMask = 2147483647;
            dstPath = /usr/share/man/man1/;
            dstSubfolderSpec = 0;
            files = (
            );
            runOnlyForDeploymentPostprocessing = 1;
        };
/* End PBXCopyFilesBuildPhase section */
)delimiter");
    }

    [[nodiscard]] bool writePBXFileReference(StringBuilder& builder, const Project& project,
                                             const Vector<RenderItem>& xcodeFiles)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;

        builder.append(R"delimiter(
/* Begin PBXFileReference section */)delimiter");

        // Target
        builder.append(R"delimiter(
        7B0074122A73143F00660B94 /* {} */ = {{isa = PBXFileReference; explicitFileType = "compiled.mach-o.executable"; includeInIndex = 0; path = "{}"; sourceTree = BUILT_PRODUCTS_DIR; }};)delimiter",
                       project.targetName.view(), project.targetName.view());

        for (auto& file : xcodeFiles)
        {
            if (file.type == RenderItem::HeaderFile)
            {
                builder.append(
                    "\n        {} /* {} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = "
                    "sourcecode.c.h; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; }};",
                    file.referenceHash, file.name, file.name, file.path);
            }
            else if (file.type == RenderItem::CppFile)
            {
                builder.append(
                    "\n        {} /* {} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = "
                    "sourcecode.cpp.cpp; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; }};",
                    file.referenceHash, file.name, file.name, file.path);
            }
            else if (file.type == RenderItem::InlineFile)
            {
                builder.append(
                    "\n        {} /* {} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = "
                    "text; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; }};",
                    file.referenceHash, file.name, file.name, file.path);
            }
            else if (file.type == RenderItem::Framework)
            {
                builder.append("\n        {} /* {} */ = {{isa = PBXFileReference; lastKnownFileType = "
                               "wrapper.framework; name = \"{}\"; path = \"{}\"; sourceTree = SDKROOT; }};",
                               file.referenceHash, file.name, file.name, file.path);
            }
        }

        builder.append("\n/* End PBXFileReference section */");

        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writePBXFrameworksBuildPhase(StringBuilder& builder, const Vector<RenderItem>& xcodeObjects)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXFrameworksBuildPhase section */
        7B00740F2A73143F00660B94 /* Frameworks */ = {
            isa = PBXFrameworksBuildPhase;
            buildActionMask = 2147483647;
            files = ()delimiter");
        for (const auto& it : xcodeObjects)
        {
            if (it.type == RenderItem::Framework)
            {
                SC_TRY(builder.append("\n                {} /* {} in Frameworks */,", it.buildHash.view(),
                                      it.name.view()));
            }
        }
        builder.append(R"delimiter(
            );
            runOnlyForDeploymentPostprocessing = 0;
        };
/* End PBXFrameworksBuildPhase section */
)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
    }
    [[nodiscard]] bool writePBXNativeTarget(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXNativeTarget section */
        7B0074112A73143F00660B94 /* {} */ = {{
            isa = PBXNativeTarget;
            buildConfigurationList = 7B0074192A73143F00660B94 /* Build configuration list for PBXNativeTarget "{}" */;
            buildPhases = (
                7B00740E2A73143F00660B94 /* Sources */,
                7B00740F2A73143F00660B94 /* Frameworks */,
                7B0074102A73143F00660B94 /* CopyFiles */,
                7B6078112B3CEF9400680265 /* ShellScript */,
            );
            buildRules = (
            );
            dependencies = (
            );
            name = {};
            productName = {};
            productReference = 7B0074122A73143F00660B94 /* {} */;
            productType = "com.apple.product-type.tool";
        }};
/* End PBXNativeTarget section */
)delimiter",
                       project.targetName.view(), project.targetName.view(), project.targetName.view(),
                       project.targetName.view(), project.targetName.view());
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writePBXProject(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXProject section */
        7B00740A2A73143F00660B94 /* Project object */ = {
            isa = PBXProject;
            attributes = {
                BuildIndependentTargetsInParallel = 1;
                LastUpgradeCheck = 1430;
                TargetAttributes = {
                    7B0074112A73143F00660B94 = {
                        CreatedOnToolsVersion = 14.3.1;
                    };
                };
            };
)delimiter");
        builder.append("            buildConfigurationList = 7B00740D2A73143F00660B94 /* Build configuration list for "
                       "PBXProject \"{}\" */;",
                       project.targetName.view());
        builder.append(R"delimiter(
            compatibilityVersion = "Xcode 14.0";
            developmentRegion = en;
            hasScannedForEncodings = 0;
            knownRegions = (
                en,
                Base,
            );
            mainGroup = 7B0074092A73143F00660B94;
            productRefGroup = 7B0074132A73143F00660B94 /* Products */;
            projectDirPath = "";
            projectRoot = "";
            targets = (
)delimiter");
        builder.append("                7B0074112A73143F00660B94 /* {} */,", project.targetName.view());
        builder.append(R"delimiter(
            );
        };
/* End PBXProject section */
)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writePBXShellScriptBuildPhase(StringBuilder& builder)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXShellScriptBuildPhase section */
        7B6078112B3CEF9400680265 /* ShellScript */ = {
            isa = PBXShellScriptBuildPhase;
            buildActionMask = 2147483647;
            files = (
			);
			inputFileListPaths = (
			);
			inputPaths = (
			);
			outputFileListPaths = (
			);
			outputPaths = (
				"$(SYMROOT)/compile_commands.json",
			);
			runOnlyForDeploymentPostprocessing = 0;
			shellPath = /bin/sh;
			shellScript = "sed -e '1s/^/[\\'$'\\n''/' -e '$s/,$/\\'$'\\n'']/' \"${SYMROOT}/CompilationDatabase/\"*.json > \"${SYMROOT}/\"compile_commands.json\nrm -rf \"${SYMROOT}/CompilationDatabase/\"";
			showEnvVarsInLog = 0;        };
/* End PBXShellScriptBuildPhase section */
)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writePBXSourcesBuildPhase(StringBuilder& builder, const Vector<RenderItem>& xcodeFiles)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXSourcesBuildPhase section */
        7B00740E2A73143F00660B94 /* Sources */ = {
            isa = PBXSourcesBuildPhase;
            buildActionMask = 2147483647;
            files = ()delimiter");
        for (auto& file : xcodeFiles)
        {
            if (file.type == RenderItem::CppFile)
                builder.append("\n                       {} /* {} in Sources */,", file.buildHash, file.name);
        }

        builder.append(
            R"delimiter(
            );
            runOnlyForDeploymentPostprocessing = 0;
        };
/* End PBXSourcesBuildPhase section */
)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
    }
    [[nodiscard]] bool writeIncludePaths(StringBuilder& builder, const Project& project)
    {
        auto includes = project.compile.get<Compile::includePaths>();
        if (includes and not includes->isEmpty())
        {
            SC_TRY(builder.append("\n                       HEADER_SEARCH_PATHS = ("));
            for (auto it : *includes)
            {
                SC_TRY(builder.append("\n                       \"{}\",", it.view())); // TODO: Escape double quotes
            }
            SC_TRY(builder.append("\n                       );"));
        }
        return true;
    }
    [[nodiscard]] bool writeDefines(StringBuilder& builder, const Project& project, const Configuration& configuration)
    {
        auto defines       = project.compile.get<Compile::preprocessorDefines>();
        auto configDefines = configuration.compile.get<Compile::preprocessorDefines>();
        bool opened        = false;
        if ((defines and not defines->isEmpty()) or (configDefines and not configDefines->isEmpty()))
        {
            opened = true;
            SC_TRY(builder.append("\n                       GCC_PREPROCESSOR_DEFINITIONS = ("));
        }
        if (defines)
        {
            for (auto& it : *defines)
            {
                SC_TRY(builder.append("\n                       \"{}\",", it.view())); // TODO: Escape double quotes
            }
        }

        if (configDefines)
        {
            for (auto it : *configDefines)
            {
                SC_TRY(builder.append("\n                       \"{}\",", it.view())); // TODO: Escape double quotes
            }
        }
        if (opened)
        {
            SC_TRY(builder.append("\n                       \"$(inherited)\","));
            SC_TRY(builder.append("\n                       );"));
        }

        return true;
    }

    [[nodiscard]] bool writeCommonOptions(StringBuilder& builder)
    {
        return builder.append(R"delimiter(
                       ALWAYS_SEARCH_USER_PATHS = NO;
                       CLANG_ANALYZER_NONNULL = YES;
                       CLANG_ANALYZER_NUMBER_OBJECT_CONVERSION = YES_AGGRESSIVE;
                       CLANG_CXX_LANGUAGE_STANDARD = "gnu++20";
                       CLANG_ENABLE_MODULES = YES;
                       CLANG_ENABLE_OBJC_ARC = YES;
                       CLANG_ENABLE_OBJC_WEAK = YES;
                       CLANG_WARN_ASSIGN_ENUM = YES;
                       CLANG_WARN_BLOCK_CAPTURE_AUTORELEASING = YES;
                       CLANG_WARN_BOOL_CONVERSION = YES;
                       CLANG_WARN_COMMA = YES;
                       CLANG_WARN_COMPLETION_HANDLER_MISUSE = YES;
                       CLANG_WARN_CONSTANT_CONVERSION = YES;
                       CLANG_WARN_DEPRECATED_OBJC_IMPLEMENTATIONS = YES;
                       CLANG_WARN_DIRECT_OBJC_ISA_USAGE = YES_ERROR;
                       CLANG_WARN_DOCUMENTATION_COMMENTS = YES;
                       CLANG_WARN_EMPTY_BODY = YES;
                       CLANG_WARN_ENUM_CONVERSION = YES;
                       CLANG_WARN_FLOAT_CONVERSION = YES_ERROR;
                       CLANG_WARN_IMPLICIT_FALLTHROUGH = YES_ERROR;
                       CLANG_WARN_IMPLICIT_SIGN_CONVERSION = YES_ERROR;
                       CLANG_WARN_INFINITE_RECURSION = YES;
                       CLANG_WARN_INT_CONVERSION = YES;
                       CLANG_WARN_NON_LITERAL_NULL_CONVERSION = YES;
                       CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF = YES;
                       CLANG_WARN_OBJC_LITERAL_CONVERSION = YES;
                       CLANG_WARN_OBJC_ROOT_CLASS = YES_ERROR;
                       CLANG_WARN_QUOTED_INCLUDE_IN_FRAMEWORK_HEADER = YES;
                       CLANG_WARN_RANGE_LOOP_ANALYSIS = YES;
                       CLANG_WARN_SEMICOLON_BEFORE_METHOD_BODY = YES;
                       CLANG_WARN_STRICT_PROTOTYPES = YES;
                       CLANG_WARN_SUSPICIOUS_IMPLICIT_CONVERSION = YES_ERROR;
                       CLANG_WARN_SUSPICIOUS_MOVE = YES;
                       CLANG_WARN_UNGUARDED_AVAILABILITY = YES_AGGRESSIVE;
                       CLANG_WARN_UNREACHABLE_CODE = YES;
                       CLANG_WARN__DUPLICATE_METHOD_MATCH = YES;
                       CLANG_WARN__EXIT_TIME_DESTRUCTORS = YES;
                       GCC_TREAT_IMPLICIT_FUNCTION_DECLARATIONS_AS_ERRORS = YES;
                       GCC_TREAT_INCOMPATIBLE_POINTER_TYPE_WARNINGS_AS_ERRORS = YES;
                       GCC_TREAT_WARNINGS_AS_ERRORS = YES;
                       GCC_WARN_64_TO_32_BIT_CONVERSION = YES;
                       GCC_WARN_ABOUT_MISSING_FIELD_INITIALIZERS = YES;
                       GCC_WARN_ABOUT_MISSING_NEWLINE = YES;
                       GCC_WARN_ABOUT_RETURN_TYPE = YES_ERROR;
                       GCC_WARN_FOUR_CHARACTER_CONSTANTS = YES;
                       GCC_WARN_HIDDEN_VIRTUAL_FUNCTIONS = YES;
                       GCC_WARN_INITIALIZER_NOT_FULLY_BRACKETED = YES;
                       GCC_WARN_NON_VIRTUAL_DESTRUCTOR = YES;
                       GCC_WARN_SHADOW = YES;
                       GCC_WARN_SIGN_COMPARE = YES;
                       GCC_WARN_UNDECLARED_SELECTOR = YES;
                       GCC_WARN_UNINITIALIZED_AUTOS = YES_AGGRESSIVE;
                       GCC_WARN_UNKNOWN_PRAGMAS = YES;
                       GCC_WARN_UNUSED_FUNCTION = YES;
                       GCC_WARN_UNUSED_LABEL = YES;
                       GCC_WARN_UNUSED_PARAMETER = YES;
                       GCC_WARN_UNUSED_VARIABLE = YES;
                       CLANG_CXX_LANGUAGE_STANDARD = "c++14";
                       ENABLE_STRICT_OBJC_MSGSEND = YES;
                       GCC_C_LANGUAGE_STANDARD = gnu11;
                       GCC_NO_COMMON_BLOCKS = YES;
                       MACOSX_DEPLOYMENT_TARGET = 13.0;
                       MTL_ENABLE_DEBUG_INFO = NO;
                       MTL_FAST_MATH = YES;
                       DEAD_CODE_STRIPPING = NO;
                       SDKROOT = macosx;)delimiter");
    }

    [[nodiscard]] bool writeConfiguration(StringBuilder& builder, const Project& project, const RenderItem& xcodeObject)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(
            R"delimiter(
        {} /* {} */ = {{
            isa = XCBuildConfiguration;
            buildSettings = {{)delimiter",
            xcodeObject.referenceHash.view(), xcodeObject.name.view());

        writeCommonOptions(builder);

        const Configuration* configuration = project.getConfiguration(xcodeObject.name.view());
        SC_TRY(configuration != nullptr);
        builder.append("\n                       CONFIGURATION_BUILD_DIR = \"");
        builder.appendReplaceMultiple(configuration->outputPath.view(), {{"$(SC_GENERATOR)", "XCode"}});
        builder.append("\";");
        builder.append("\n                       SYMROOT = \"");
        builder.appendReplaceMultiple(configuration->intermediatesPath.view(), {{"$(SC_GENERATOR)", "XCode"}});
        builder.append("\";");
        if (configuration->compile.hasValue<Compile::enableRTTI>(true))
        {
            builder.append("\n            GCC_ENABLE_CPP_RTTI = YES;");
        }
        else
        {
            builder.append("\n            GCC_ENABLE_CPP_RTTI = NO;");
        }
        if (configuration->compile.hasValue<Compile::enableExceptions>(true))
        {
            builder.append("\n            GCC_ENABLE_CPP_EXCEPTIONS = YES;");
        }
        else
        {
            builder.append("\n            GCC_ENABLE_CPP_EXCEPTIONS = NO;");
        }
        builder.append(R"delimiter(
        OTHER_CFLAGS = (
            "$(inherited)",
            "-gen-cdb-fragment-path \"$(SYMROOT)/CompilationDatabase\"",
        );)delimiter");

        if (not configuration->compile.hasValue<Compile::enableStdCpp>(true))
        {
            builder.append(R"delimiter(
        OTHER_CPLUSPLUSFLAGS = (
            "$(OTHER_CFLAGS)",
            "-nostdinc++",
        );)delimiter");
        }

        if (not configuration->link.hasValue<Link::enableStdCpp>(true))
        {

            builder.append("        OTHER_LDFLAGS = \"-nostdlib++\";");
        }

        if (configuration->compile.hasValue<Compile::optimizationLevel>(Optimization::Debug))
        {
            builder.append(R"delimiter(
                       COPY_PHASE_STRIP = NO;
                       ONLY_ACTIVE_ARCH = YES;
                       DEBUG_INFORMATION_FORMAT = dwarf;
                       ENABLE_TESTABILITY = YES;
                       GCC_DYNAMIC_NO_PIC = NO;
                       GCC_OPTIMIZATION_LEVEL = 0;)delimiter");
        }
        else
        {
            builder.append(R"delimiter(
                       COPY_PHASE_STRIP = YES;
                       DEBUG_INFORMATION_FORMAT = "dwarf-with-dsym";
                       ENABLE_NS_ASSERTIONS = NO;)delimiter");
        }

        writeDefines(builder, project, *configuration);
        writeIncludePaths(builder, project);
        builder.append(R"delimiter(
            }};
            name = {};
        }};)delimiter",
                       xcodeObject.name.view());
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeXCBuildConfiguration(StringBuilder& builder, const Project& project,
                                                 Vector<RenderItem>& xcodeObjects)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("\n/* Begin XCBuildConfiguration section */");

        for (auto& configuration : xcodeObjects)
        {
            if (configuration.type == RenderItem::Configuration)
            {
                SC_TRY(writeConfiguration(builder, project, configuration));
            }
        }

        for (auto& configuration : xcodeObjects)
        {
            if (configuration.type == RenderItem::Configuration)
            {
                builder.append(R"delimiter(
        {} /* {} */ = {{
            isa = XCBuildConfiguration;
            buildSettings = {{
                CODE_SIGN_STYLE = Automatic;
                PRODUCT_NAME = "$(TARGET_NAME)";
            }};
            name = {};
        }};)delimiter",
                               configuration.buildHash.view(), configuration.name.view(), configuration.name.view());
            }
        }
        builder.append("\n/* End XCBuildConfiguration section */\n");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeXCConfigurationList(StringBuilder& builder, const Project& project,
                                                const Vector<RenderItem>& xcodeObjects)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;

        builder.append(
            R"delimiter(
/* Begin XCConfigurationList section */
        7B00740D2A73143F00660B94 /* Build configuration list for PBXProject "{}" */ = {{
            isa = XCConfigurationList;
            buildConfigurations = ()delimiter",
            project.targetName.view());

        for (auto& configuration : xcodeObjects)
        {
            if (configuration.type == RenderItem::Configuration)
            {
                builder.append("\n                {} /* {} */,", configuration.referenceHash.view(),
                               configuration.name);
            }
        }
        builder.append(
            R"delimiter(
            );
            defaultConfigurationIsVisible = 0;
            defaultConfigurationName = Release;
        }};
        7B0074192A73143F00660B94 /* Build configuration list for PBXNativeTarget "{}" */ = {{
            isa = XCConfigurationList;
            buildConfigurations = ()delimiter",
            project.targetName.view());
        for (auto& configuration : xcodeObjects)
        {
            if (configuration.type == RenderItem::Configuration)
            {
                builder.append("\n                {} /* {} */,", configuration.buildHash.view(), configuration.name);
            }
        }
        builder.append(
            R"delimiter(
            );
            defaultConfigurationIsVisible = 0;
            defaultConfigurationName = Release;
        };
/* End XCConfigurationList section */
)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool fillXCodeFiles(StringView destinationDirectory, const Project& project,
                                      Vector<RenderItem>& outputFiles)
    {
        SC_TRY(WriterInternal::fillFiles(definitionCompiler, destinationDirectory, project, outputFiles));
        for (auto& it : outputFiles)
        {
            SC_TRY(computeReferenceHash(it.name.view(), it.referenceHash));
            SC_TRY(computeBuildHash(it.name.view(), it.buildHash));
        }
        return true;
    }

    [[nodiscard]] bool fillXCodeFrameworks(const Project& project, Vector<RenderItem>& xcodeObjects)
    {
        auto frameworks = project.link.get<Link::libraryFrameworks>();
        SC_TRY(frameworks != nullptr);
        for (auto it : *frameworks)
        {
            RenderItem xcodeFile;
            xcodeFile.name = Path::basename(it.view(), Path::AsPosix);
            SC_TRY(StringBuilder(xcodeFile.name, StringBuilder::DoNotClear).append(".framework"));
            xcodeFile.type = RenderItem::Framework;
            SC_TRY(Path::join(xcodeFile.path, {"System/Library/Frameworks", xcodeFile.name.view()}, "/"));
            SC_TRY(computeBuildHash(xcodeFile.name.view(), xcodeFile.buildHash));
            SC_TRY(computeReferenceHash(xcodeFile.name.view(), xcodeFile.referenceHash));
            SC_TRY(xcodeObjects.push_back(move(xcodeFile)));
        }
        return true;
    }

    [[nodiscard]] bool fillXCodeConfigurations(const Project& project, Vector<RenderItem>& xcodeObjects)
    {
        for (const Configuration& configuration : project.configurations)
        {
            RenderItem xcodeObject;
            xcodeObject.type = RenderItem::Configuration;
            SC_TRY(xcodeObject.name.assign(configuration.name.view()));
            SC_TRY(computeReferenceHash(configuration.name.view(), xcodeObject.referenceHash));
            SC_TRY(computeBuildHash(configuration.name.view(), xcodeObject.buildHash));
            SC_TRY(xcodeObjects.push_back(move(xcodeObject)));
        }
        return true;
    }

    [[nodiscard]] bool writeProject(StringBuilder& builder, const Project& project, Renderer& renderer)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(// !$*UTF8*$!
{
    archiveVersion = 1;
    classes = {
    };
    objectVersion = 56;
    objects = {
)delimiter");

        writePBXBuildFile(builder, renderer.renderItems);
        writePBXCopyFilesBuildPhase(builder);
        writePBXFileReference(builder, project, renderer.renderItems);
        writePBXFrameworksBuildPhase(builder, renderer.renderItems);

        builder.append("\n/* Begin PBXGroup section */\n");
        printGroupRecursive(builder, renderer.rootGroup);
        builder.append("/* End PBXGroup section */\n");

        writePBXNativeTarget(builder, project);
        writePBXProject(builder, project);
        writePBXShellScriptBuildPhase(builder);
        writePBXSourcesBuildPhase(builder, renderer.renderItems);
        writeXCBuildConfiguration(builder, project, renderer.renderItems);
        writeXCConfigurationList(builder, project, renderer.renderItems);
        builder.append(R"delimiter(    };
    rootObject = 7B00740A2A73143F00660B94 /* Project object */;
}
)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] Result writeScheme(StringBuilder& builder, const Project& project, Renderer& renderer,
                                     StringView destinationDirectory, StringView filename)
    {
        SC_COMPILER_UNUSED(destinationDirectory);
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        String output;
        for (auto& item : renderer.renderItems)
        {
            if (item.type == WriterInternal::RenderItem::DebugVisualizerFile)
            {
                if (output.isEmpty())
                {
                    Path::join(output, {"$(SRCROOT)", item.path.view()}, Path::Posix::SeparatorStringView());
                }
                else
                {
                    return Result::Error("XCode: only a single lldbinit file is supported");
                }
            }
        }
        StringView lldbinit = output.view();
        // TODO: De-hardcode this thing
        builder.append(R"delimiter(<?xml version="1.0" encoding="UTF-8"?>
<Scheme
   LastUpgradeVersion = "1430"
   version = "1.3">
   <BuildAction
      parallelizeBuildables = "YES"
      buildImplicitDependencies = "YES">
      <BuildActionEntries>
         <BuildActionEntry
            buildForTesting = "YES"
            buildForRunning = "YES"
            buildForProfiling = "YES"
            buildForArchiving = "YES"
            buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "{}"
               BuildableName = "{}"
               BlueprintName = "{}"
               ReferencedContainer = "container:{}.xcodeproj">
            </BuildableReference>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <TestAction
      buildConfiguration = "Debug"
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      shouldUseLaunchSchemeArgsEnv = "YES">
      <Testables>
      </Testables>
   </TestAction>
   <LaunchAction
      buildConfiguration = "Debug"
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      customLLDBInitFile = "{}"
      enableAddressSanitizer = "YES"
      enableASanStackUseAfterReturn = "YES"
      enableUBSanitizer = "YES"
      launchStyle = "0"
      useCustomWorkingDirectory = "NO"
      ignoresPersistentStateOnLaunch = "NO"
      debugDocumentVersioning = "YES"
      debugServiceExtension = "internal"
      allowLocationSimulation = "YES"
      viewDebuggingEnabled = "No">
      <BuildableProductRunnable
         runnableDebuggingMode = "0">
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "{}"
            BuildableName = "{}"
            BlueprintName = "{}"
            ReferencedContainer = "container:{}.xcodeproj">
         </BuildableReference>
      </BuildableProductRunnable>
   </LaunchAction>
   <ProfileAction
      buildConfiguration = "Release"
      shouldUseLaunchSchemeArgsEnv = "YES"
      savedToolIdentifier = ""
      useCustomWorkingDirectory = "NO"
      debugDocumentVersioning = "YES">
      <BuildableProductRunnable
         runnableDebuggingMode = "0">
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "{}"
            BuildableName = "{}"
            BlueprintName = "{}"
            ReferencedContainer = "container:{}.xcodeproj">
         </BuildableReference>
      </BuildableProductRunnable>
   </ProfileAction>
   <AnalyzeAction
      buildConfiguration = "Debug">
   </AnalyzeAction>
   <ArchiveAction
      buildConfiguration = "Release"
      revealArchiveInOrganizer = "YES">
   </ArchiveAction>
</Scheme>

)delimiter",
                       "7B00740A2A73143F00660B94", project.name.view(), project.name.view(), filename, lldbinit, //
                       "7B00740A2A73143F00660B94", project.name.view(), project.name.view(), filename,           //
                       "7B00740A2A73143F00660B94", project.name.view(), project.name.view(), filename);
        SC_COMPILER_WARNING_POP;
        return Result(true);
    }
};
