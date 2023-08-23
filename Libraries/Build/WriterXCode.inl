// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../FileSystem/FileSystem.h"
#include "../FileSystem/FileSystemWalker.h"
#include "../FileSystem/Path.h"
#include "../Foundation/StringBuilder.h"
#include "../Foundation/StringViewAlgorithms.h"
#include "../Hashing/Hashing.h"
#include "Build.h"
#include "WriteInternal.h"

struct SC::Build::ProjectWriter::WriterXCode
{
    Definition&         definition;
    DefinitionCompiler& definitionCompiler;
    Hashing             hashing;

    WriterXCode(Definition& definition, DefinitionCompiler& definitionCompiler)
        : definition(definition), definitionCompiler(definitionCompiler)
    {}
    using RenderItem  = WriterInternal::RenderItem;
    using RenderGroup = WriterInternal::RenderGroup;
    using Renderer    = WriterInternal::Renderer;

    [[nodiscard]] bool prepare(StringView destinationDirectory, const Project& project, Renderer& renderer)
    {
        SC_TRY_IF(fillXCodeFiles(destinationDirectory, project, renderer.renderItems));
        SC_TRY_IF(fillXCodeFrameworks(project, renderer.renderItems));
        SC_TRY_IF(fillXCodeConfigurations(project, renderer.renderItems));
        SC_TRY_IF(fillFileGroups(renderer.rootGroup, renderer.renderItems));
        SC_TRY_IF(fillProductGroup(project, renderer.rootGroup));
        SC_TRY_IF(fillFrameworkGroup(project, renderer.rootGroup, renderer.renderItems));
        return true;
    }

    [[nodiscard]] bool computeReferenceHash(StringView name, String& hash)
    {
        SC_TRY_IF(hashing.setType(Hashing::TypeSHA1));
        SC_TRY_IF(hashing.update("reference_"_a8.toVoidSpan()));
        SC_TRY_IF(hashing.update(name.toVoidSpan()));
        Hashing::Result res;
        SC_TRY_IF(hashing.finalize(res));
        SmallString<64> tmpHash = StringEncoding::Ascii;
        SC_TRY_IF(StringBuilder(tmpHash).appendHex(res.toSpanVoid()));
        return hash.assign(tmpHash.view().sliceStartLength(0, 24));
    }

    [[nodiscard]] bool computeBuildHash(StringView name, String& hash)
    {
        SC_TRY_IF(hashing.setType(Hashing::TypeSHA1));
        SC_TRY_IF(hashing.update("build_"_a8.toVoidSpan()));
        SC_TRY_IF(hashing.update(name.toVoidSpan()));
        Hashing::Result res;
        SC_TRY_IF(hashing.finalize(res));
        SmallString<64> tmpHash = StringEncoding::Ascii;
        SC_TRY_IF(StringBuilder(tmpHash).appendHex(res.toSpanVoid()));
        return hash.assign(tmpHash.view().sliceStartLength(0, 24));
    }

    [[nodiscard]] bool fillFileGroups(RenderGroup& group, const Vector<RenderItem>& xcodeFiles)
    {
        SC_TRY_IF(group.referenceHash.assign("7B0074092A73143F00660B94"));
        SC_TRY_IF(group.name.assign("/"));
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
                        SC_TRY_IF(computeReferenceHash(tokenizer.component, current->referenceHash));
                    }
                    else
                    {
                        SC_TRY_IF(computeReferenceHash(tokenizer.processed, current->referenceHash));
                    }
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool fillProductGroup(const Project& project, RenderGroup& group)
    {
        auto products = group.children.getOrCreate("Products"_a8);
        SC_TRY_IF(products != nullptr);
        SC_TRY_IF(products->name.assign("Products"));
        SC_TRY_IF(products->referenceHash.assign("7B0074132A73143F00660B94"));
        auto test = products->children.getOrCreate(project.targetName.view());
        SC_TRY_IF(test != nullptr);
        SC_TRY_IF(test->name.assign(project.targetName.view()));
        SC_TRY_IF(test->referenceHash.assign("7B0074122A73143F00660B94"));
        return true;
    }

    [[nodiscard]] bool fillFrameworkGroup(const Project&, RenderGroup& group, Vector<RenderItem>& xcodeFiles)
    {
        auto frameworksGroup = group.children.getOrCreate("Frameworks"_a8);
        SC_TRY_IF(frameworksGroup != nullptr);
        SC_TRY_IF(frameworksGroup->name.assign("Frameworks"));
        SC_TRY_IF(frameworksGroup->referenceHash.assign("7B3D0EF12A74DEEF00AE03EE"));

        for (auto it : xcodeFiles)
        {
            if (it.type == RenderItem::Framework)
            {
                auto framework = frameworksGroup->children.getOrCreate(it.name);
                SC_TRY_IF(framework != nullptr);
                SC_TRY_IF(framework->name.assign(it.name.view()));
                SC_TRY_IF(framework->referenceHash.assign(it.referenceHash.view()));
            }
        }
        return true;
    }

    [[nodiscard]] bool printGroupRecursive(StringBuilder& builder, const RenderGroup& parentGroup)
    {
        if (parentGroup.name == "/")
        {
            SC_TRY_IF(builder.append("        {} = {{\n", parentGroup.referenceHash));
        }
        else
        {
            SC_TRY_IF(builder.append("        {} /* {} */ = {{\n", parentGroup.referenceHash, parentGroup.name));
        }

        SC_TRY_IF(builder.append("            isa = PBXGroup;\n"));
        SC_TRY_IF(builder.append("            children = (\n"));
        for (const auto& group : parentGroup.children)
        {
            SC_TRY_IF(builder.append("                {} /* {} */,\n", group.value.referenceHash, group.value.name));
        }
        SC_TRY_IF(builder.append("            );\n"));
        if (parentGroup.name != "/")
        {
            SC_TRY_IF(builder.append("            name = {};\n", parentGroup.name));
        }
        SC_TRY_IF(builder.append("            sourceTree = \"<group>\";\n"));
        SC_TRY_IF(builder.append("        };\n"));
        for (const auto& group : parentGroup.children)
        {
            if (not group.value.children.isEmpty())
            {
                SC_TRY_IF(printGroupRecursive(builder, group.value));
            }
        }
        return true;
    }

    [[nodiscard]] bool writePBXBuildFile(StringBuilder& builder, const Vector<RenderItem>& xcodeFiles)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;
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
        SC_WARNING_RESTORE;
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
        SC_WARNING_DISABLE_UNUSED_RESULT;

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

        SC_WARNING_RESTORE;
        return true;
    }

    [[nodiscard]] bool writePBXFrameworksBuildPhase(StringBuilder& builder, const Vector<RenderItem>& xcodeObjects)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;
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
                SC_TRY_IF(builder.append("\n                {} /* {} in Frameworks */,", it.buildHash.view(),
                                         it.name.view()));
            }
        }
        builder.append(R"delimiter(
            );
            runOnlyForDeploymentPostprocessing = 0;
        };
/* End PBXFrameworksBuildPhase section */
)delimiter");
        SC_WARNING_RESTORE;
        return true;
    }
    [[nodiscard]] bool writePBXNativeTarget(StringBuilder& builder, const Project& project)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXNativeTarget section */
        7B0074112A73143F00660B94 /* {} */ = {{
            isa = PBXNativeTarget;
            buildConfigurationList = 7B0074192A73143F00660B94 /* Build configuration list for PBXNativeTarget "{}" */;
            buildPhases = (
                7B00740E2A73143F00660B94 /* Sources */,
                7B00740F2A73143F00660B94 /* Frameworks */,
                7B0074102A73143F00660B94 /* CopyFiles */,
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
        SC_WARNING_RESTORE;
        return true;
    }

    [[nodiscard]] bool writePBXProject(StringBuilder& builder, const Project& project)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;
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
        SC_WARNING_RESTORE;
        return true;
    }

    [[nodiscard]] bool writePBXSourcesBuildPhase(StringBuilder& builder, const Vector<RenderItem>& xcodeFiles)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;
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
        SC_WARNING_RESTORE;
        return true;
    }
    [[nodiscard]] bool writeIncludePaths(StringBuilder& builder, const Project& project)
    {
        auto includes = project.compile.get<Compile::includePaths>();
        if (includes and not includes->isEmpty())
        {
            SC_TRY_IF(builder.append("\n                       HEADER_SEARCH_PATHS = ("));
            for (auto it : *includes)
            {
                SC_TRY_IF(builder.append("\n                       \"{}\",", it.view())); // TODO: Escape double quotes
            }
            SC_TRY_IF(builder.append("\n                       );"));
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
            SC_TRY_IF(builder.append("\n                       GCC_PREPROCESSOR_DEFINITIONS = ("));
        }
        if (defines)
        {
            for (auto& it : *defines)
            {
                SC_TRY_IF(builder.append("\n                       \"{}\",", it.view())); // TODO: Escape double quotes
            }
        }

        if (configDefines)
        {
            for (auto it : *configDefines)
            {
                SC_TRY_IF(builder.append("\n                       \"{}\",", it.view())); // TODO: Escape double quotes
            }
        }
        if (opened)
        {
            SC_TRY_IF(builder.append("\n                       \"$(inherited)\","));
            SC_TRY_IF(builder.append("\n                       );"));
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
                       DEAD_CODE_STRIPPING = YES;
                       SDKROOT = macosx;)delimiter");
    }

    [[nodiscard]] bool writeConfiguration(StringBuilder& builder, const Project& project, const RenderItem& xcodeObject)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;
        builder.append(
            R"delimiter(
        {} /* {} */ = {{
            isa = XCBuildConfiguration;
            buildSettings = {{)delimiter",
            xcodeObject.referenceHash.view(), xcodeObject.name.view());

        writeCommonOptions(builder);

        const Configuration* configuration = project.getConfiguration(xcodeObject.name.view());
        SC_TRY_IF(configuration != nullptr);
        builder.append("\n                       CONFIGURATION_BUILD_DIR = \"");
        builder.appendReplaceMultiple(configuration->outputPath.view(), {{"$(SC_GENERATOR)", "xcode13"}});
        builder.append("\";");
        builder.append("\n                       SYMROOT = \"");
        builder.appendReplaceMultiple(configuration->intermediatesPath.view(), {{"$(SC_GENERATOR)", "xcode13"}});
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
        SC_WARNING_RESTORE;
        return true;
    }

    [[nodiscard]] bool writeXCBuildConfiguration(StringBuilder& builder, const Project& project,
                                                 Vector<RenderItem>& xcodeObjects)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;
        builder.append("\n/* Begin XCBuildConfiguration section */");

        for (auto& configuration : xcodeObjects)
        {
            if (configuration.type == RenderItem::Configuration)
            {
                SC_TRY_IF(writeConfiguration(builder, project, configuration));
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
        SC_WARNING_RESTORE;
        return true;
    }

    [[nodiscard]] bool writeXCConfigurationList(StringBuilder& builder, const Project& project,
                                                const Vector<RenderItem>& xcodeObjects)
    {
        SC_WARNING_DISABLE_UNUSED_RESULT;

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
        SC_WARNING_RESTORE;
        return true;
    }

    [[nodiscard]] bool fillXCodeFiles(StringView destinationDirectory, const Project& project,
                                      Vector<RenderItem>& outputFiles)
    {
        SC_TRY_IF(WriterInternal::fillFiles(definitionCompiler, destinationDirectory, project, outputFiles));
        for (auto& it : outputFiles)
        {
            SC_TRY_IF(computeReferenceHash(it.name.view(), it.referenceHash));
            SC_TRY_IF(computeBuildHash(it.name.view(), it.buildHash));
        }
        return true;
    }

    [[nodiscard]] bool fillXCodeFrameworks(const Project& project, Vector<RenderItem>& xcodeObjects)
    {
        auto frameworks = project.link.get<Link::libraryFrameworks>();
        SC_TRY_IF(frameworks != nullptr);
        for (auto it : *frameworks)
        {
            RenderItem xcodeFile;
            xcodeFile.name = Path::basename(it.view(), Path::AsPosix);
            xcodeFile.type = RenderItem::Framework;
            SC_TRY_IF(Path::join(xcodeFile.path, {"System/Library/Frameworks", xcodeFile.name.view()}, "/"));
            SC_TRY_IF(computeBuildHash(xcodeFile.name.view(), xcodeFile.buildHash));
            SC_TRY_IF(computeReferenceHash(xcodeFile.name.view(), xcodeFile.referenceHash));
            SC_TRY_IF(xcodeObjects.push_back(move(xcodeFile)));
        }
        return true;
    }

    [[nodiscard]] bool fillXCodeConfigurations(const Project& project, Vector<RenderItem>& xcodeObjects)
    {
        for (const Configuration& configuration : project.configurations)
        {
            RenderItem xcodeObject;
            xcodeObject.type = RenderItem::Configuration;
            SC_TRY_IF(xcodeObject.name.assign(configuration.name.view()));
            SC_TRY_IF(computeReferenceHash(configuration.name.view(), xcodeObject.referenceHash));
            SC_TRY_IF(computeBuildHash(configuration.name.view(), xcodeObject.buildHash));
            SC_TRY_IF(xcodeObjects.push_back(move(xcodeObject)));
        }
        return true;
    }

    [[nodiscard]] bool write(StringBuilder& builder, StringView destinationDirectory)
    {
        const Project& project = definition.workspaces[0].projects[0];
        Renderer       renderer;
        SC_TRY_IF(prepare(destinationDirectory, project, renderer));
        SC_WARNING_DISABLE_UNUSED_RESULT;
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
        writePBXSourcesBuildPhase(builder, renderer.renderItems);
        writeXCBuildConfiguration(builder, project, renderer.renderItems);
        writeXCConfigurationList(builder, project, renderer.renderItems);
        builder.append(R"delimiter(    };
    rootObject = 7B00740A2A73143F00660B94 /* Project object */;
}
)delimiter");
        SC_WARNING_RESTORE;
        return true;
    }
};
