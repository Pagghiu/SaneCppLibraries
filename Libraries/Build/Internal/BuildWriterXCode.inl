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
    const Definition&          definition;
    const DefinitionCompiler&  definitionCompiler;
    const Directories&         directories;
    const RelativeDirectories& relativeDirectories;

    Hashing hashing;

    WriterXCode(const Definition& definition, const DefinitionCompiler& definitionCompiler,
                const Directories& directories, const RelativeDirectories& relativeDirectories)
        : definition(definition), definitionCompiler(definitionCompiler), directories(directories),
          relativeDirectories(relativeDirectories)
    {}
    using RenderItem  = WriterInternal::RenderItem;
    using RenderGroup = WriterInternal::RenderGroup;
    using Renderer    = WriterInternal::Renderer;

    [[nodiscard]] bool prepare(const Project& project, Renderer& renderer)
    {
        SC_TRY(fillXCodeFiles(directories.projectsDirectory.view(), project, renderer.renderItems));
        SC_TRY(fillXCodeFrameworks(project, renderer.renderItems));
        SC_TRY(fillXCodeConfigurations(project, renderer.renderItems));
        SC_TRY(fillFileGroups(renderer.rootGroup, renderer.renderItems));
        SC_TRY(fillProductGroup(project, renderer.rootGroup));
        SC_TRY(fillFrameworkGroup(project, renderer.rootGroup, renderer.renderItems));
        SC_TRY(fillResourcesGroup(project, renderer.rootGroup, renderer.renderItems));
        return true;
    }

    [[nodiscard]] bool computeReferenceHash(StringView name, String& hash)
    {
        SC_TRY(hashing.setType(Hashing::TypeSHA1));
        SC_TRY(hashing.add("reference_"_a8.toBytesSpan()));
        SC_TRY(hashing.add(name.toBytesSpan()));
        Hashing::Result res;
        SC_TRY(hashing.getHash(res));
        SmallString<64> tmpHash = StringEncoding::Ascii;
        SC_TRY(StringBuilder(tmpHash).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
        return hash.assign(tmpHash.view().sliceStartLength(0, 24));
    }

    [[nodiscard]] bool computeBuildHash(StringView name, String& hash)
    {
        SC_TRY(hashing.setType(Hashing::TypeSHA1));
        SC_TRY(hashing.add("build_"_a8.toBytesSpan()));
        SC_TRY(hashing.add(name.toBytesSpan()));
        Hashing::Result res;
        SC_TRY(hashing.getHash(res));
        SmallString<64> tmpHash = StringEncoding::Ascii;
        SC_TRY(StringBuilder(tmpHash).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
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
            if (it.type == RenderItem::Framework or it.type == RenderItem::SystemLibrary)
            {
                auto framework = frameworksGroup->children.getOrCreate(it.name);
                SC_TRY(framework != nullptr);
                SC_TRY(framework->name.assign(it.name.view()));
                SC_TRY(framework->referenceHash.assign(it.referenceHash.view()));
            }
        }
        return true;
    }

    [[nodiscard]] bool fillResourcesGroup(const Project& project, RenderGroup& group, Vector<RenderItem>& xcodeFiles)
    {
        if (project.targetType == TargetType::ConsoleExecutable)
        {
            return true;
        }

        auto resourcesGroup = group.children.getOrCreate("Resources"_a8);
        SC_TRY(resourcesGroup != nullptr);
        SC_TRY(resourcesGroup->name.assign("Resources"));
        SC_TRY(resourcesGroup->referenceHash.assign("7A3A0EF12979DAEB00AE0312"));

        auto entitlement = resourcesGroup->children.getOrCreate("7B5A4A5A2C20D35E00EB8229");
        SC_TRY(entitlement != nullptr);
        SC_TRY(StringBuilder(entitlement->name).format("{0}.entitlements", project.name.view()));
        SC_TRY(entitlement->referenceHash.assign("7B5A4A5A2C20D35E00EB8229"));

        auto storyboard = resourcesGroup->children.getOrCreate("7B375FE92C2F16B1007D27E7");
        SC_TRY(storyboard != nullptr);
        SC_TRY(StringBuilder(storyboard->name).format("{0}.storyboard", project.name.view()));
        SC_TRY(storyboard->referenceHash.assign("7B375FE92C2F16B1007D27E7"));

        auto xcasset = resourcesGroup->children.getOrCreate("7A4F78E229662D25000D7EE4");
        SC_TRY(xcasset != nullptr);
        SC_TRY(StringBuilder(xcasset->name).format("{0}.xcassets", project.name.view()));
        SC_TRY(xcasset->referenceHash.assign("7A4F78E229662D25000D7EE4"));

        RenderItem xcodeFile;
        SC_TRY(StringBuilder(xcodeFile.name).format("{0}.xcassets", project.name.view()));
        xcodeFile.type          = RenderItem::XCAsset;
        xcodeFile.buildHash     = "7BEC30AF2C31BCF000961B17";
        xcodeFile.referenceHash = "7A4F78E229662D25000D7EE4";
        xcodeFile.referencePath = "Resources";
        SC_TRY(StringBuilder(xcodeFile.path).format("{0}.xcassets", project.name.view()));
        return xcodeFiles.push_back(xcodeFile);
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
            if (file.type == RenderItem::CppFile or file.type == RenderItem::CFile or
                file.type == RenderItem::ObjCFile or file.type == RenderItem::ObjCppFile)
            {
                builder.append("        {} /* {} in Sources */ = {{isa = PBXBuildFile; fileRef = {} /* {} */; }};\n",
                               file.buildHash, file.name, file.referenceHash, file.name);
            }
            else if (file.type == RenderItem::Framework or file.type == RenderItem::SystemLibrary)
            {

                String platformFilters;
                if (not file.platformFilters.isEmpty())
                {
                    platformFilters = "platformFilters = (";
                    StringBuilder sb(platformFilters, StringBuilder::DoNotClear);
                    for (size_t idx = 0; idx < file.platformFilters.size(); ++idx)
                    {
                        sb.append(file.platformFilters[idx].view());
                        sb.append(", ");
                    }
                    sb.append(");");
                }
                builder.append(
                    "        {0} /* {1} in Frameworks */ = {{isa = PBXBuildFile; fileRef = {2} /* {1} */;{3} }};\n",
                    file.buildHash, file.name, file.referenceHash, platformFilters);
            }
            else if (file.type == RenderItem::XCAsset)
            {
                builder.append("        {} /* {} in Resources */ = {{isa = PBXBuildFile; fileRef = {} /* {} */; }};\n",
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
        StringView productType;
        StringView productExtension;
        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable: productType = "compiled.mach-o.executable"; break;
        case TargetType::GUIApplication:
            productType      = "wrapper.application";
            productExtension = ".app";
            break;
        }

        builder.append(R"delimiter(
        7B0074122A73143F00660B94 /* {0}{1} */ = {{isa = PBXFileReference; explicitFileType = "{2}"; includeInIndex = 0; path = "{0}{1}"; sourceTree = BUILT_PRODUCTS_DIR; }};)delimiter",
                       project.targetName.view(), productExtension, productType);

        // Entitlements
        if (project.targetType != TargetType::ConsoleExecutable)
        {
            builder.append(R"delimiter(
        7B5A4A5A2C20D35E00EB8229 /* {0}.entitlements */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = text.plist.entitlements; path = {0}.entitlements; sourceTree = "<group>"; }};)delimiter",
                           project.name.view());
            builder.append(R"delimiter(
        7B375FE92C2F16B1007D27E7 /* {0}.storyboard */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = file.storyboard; path = {0}.storyboard; sourceTree = "<group>"; }};)delimiter",
                           project.name.view());
        }

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
            else if (file.type == RenderItem::CFile)
            {
                builder.append(
                    "\n        {} /* {} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = "
                    "sourcecode.c.c; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; }};",
                    file.referenceHash, file.name, file.name, file.path);
            }
            else if (file.type == RenderItem::ObjCFile)
            {
                builder.append(
                    "\n        {} /* {} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = "
                    "sourcecode.m.m; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; }};",
                    file.referenceHash, file.name, file.name, file.path);
            }

            else if (file.type == RenderItem::ObjCppFile)
            {
                builder.append(
                    "\n        {} /* {} */ = {{isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = "
                    "sourcecode.mm.mm; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; }};",
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

            else if (file.type == RenderItem::SystemLibrary)
            {
                builder.append(
                    "\n        {} /* {} */ = {{isa = PBXFileReference; lastKnownFileType = "
                    "sourcecode.text-based-dylib-definition; name = \"{}\"; path = \"{}\"; sourceTree = SDKROOT; }};",
                    file.referenceHash, file.name, file.name, file.path);
            }
            else if (file.type == RenderItem::XCAsset)
            {
                builder.append("\n        {} /* {} */ = {{isa = PBXFileReference; lastKnownFileType = "
                               "folder.assetcatalog; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; }};",
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
            if (it.type == RenderItem::Framework or it.type == RenderItem::SystemLibrary)
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
        StringView productType;
        StringView productExtension;
        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable: productType = "com.apple.product-type.tool"; break;
        case TargetType::GUIApplication:
            productType      = "com.apple.product-type.application";
            productExtension = ".app";
            break;
        }

        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXNativeTarget section */
        7B0074112A73143F00660B94 /* {0} */ = {{
            isa = PBXNativeTarget;
            buildConfigurationList = 7B0074192A73143F00660B94 /* Build configuration list for PBXNativeTarget "{0}" */;
            buildPhases = (
                7B00740E2A73143F00660B94 /* Sources */,
                7B00740F2A73143F00660B94 /* Frameworks */,
                7B0074102A73143F00660B94 /* CopyFiles */,
                7B6078112B3CEF9400680265 /* ShellScript */,
				7BEC30B42C31C33D00961B17 /* Resources */,
            );
            buildRules = (
            );
            dependencies = (
            );
            name = {0};
            productName = {0};
            productReference = 7B0074122A73143F00660B94 /* {0}{1} */;
            productType = "{2}";
        }};
/* End PBXNativeTarget section */
)delimiter",
                       project.targetName.view(), productExtension, productType);
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

    [[nodiscard]] bool writePBXResourcesBuildPhase(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
/* Begin PBXResourcesBuildPhase section */
		7BEC30B42C31C33D00961B17 /* Resources */ = {{
			isa = PBXResourcesBuildPhase;
			buildActionMask = 2147483647;
			files = (
				7BEC30AF2C31BCF000961B17 /* {0}.xcassets in Resources */,
			);
			runOnlyForDeploymentPostprocessing = 0;
		}};
/* End PBXResourcesBuildPhase section */
)delimiter",
                       project.name.view());
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
            if (file.type == RenderItem::CppFile or file.type == RenderItem::CFile or
                file.type == RenderItem::ObjCppFile or file.type == RenderItem::ObjCFile)
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
    [[nodiscard]] bool writeincludes(StringBuilder& builder, const Project& project)
    {
        if (not project.files.compile.includePaths.isEmpty())
        {
            SC_TRY(builder.append("\n                       HEADER_SEARCH_PATHS = ("));
            for (const String& it : project.files.compile.includePaths)
            {
                // TODO: Escape double quotes for include paths
                if (Path::isAbsolute(it.view(), Path::AsNative))
                {
                    String relative;
                    SC_TRY(Path::relativeFromTo(directories.projectsDirectory.view(), it.view(), relative,
                                                Path::AsNative));
                    SC_TRY(builder.append("\n                       \"$(PROJECT_DIR)/{}\",", relative));
                }
                else
                {
                    // Relative to project root but expressed as relative to project dir
                    SC_TRY(builder.append("\n                       \"$(PROJECT_DIR)/{}/{}\",",
                                          relativeDirectories.relativeProjectsToProjectRoot, it.view()));
                }
            }
            SC_TRY(builder.append("\n                       );"));
        }
        return true;
    }

    [[nodiscard]] bool writeDefines(StringBuilder& builder, const Project& project, const Configuration& configuration)
    {
        bool opened = false;
        if (not project.files.compile.defines.isEmpty() or not configuration.compile.defines.isEmpty())
        {
            opened = true;
            SC_TRY(builder.append("\n                       GCC_PREPROCESSOR_DEFINITIONS = ("));
        }
        for (const String& it : project.files.compile.defines)
        {
            SC_TRY(builder.append("\n                       \""));
            SC_TRY(appendVariable(builder, it.view())); // TODO: Escape double quotes
            SC_TRY(builder.append("\","));
        }

        for (const String& it : configuration.compile.defines)
        {
            SC_TRY(builder.append("\n                       \""));
            SC_TRY(appendVariable(builder, it.view())); // TODO: Escape double quotes
            SC_TRY(builder.append("\","));
        }
        if (opened)
        {
            SC_TRY(builder.append("\n                       \"$(inherited)\","));
            SC_TRY(builder.append("\n                       );"));
        }

        return true;
    }

    [[nodiscard]] bool writeCommonOptions(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(
                       ALWAYS_SEARCH_USER_PATHS = NO;
                       ASSETCATALOG_COMPILER_GENERATE_SWIFT_ASSET_SYMBOL_EXTENSIONS = NO;
                       CLANG_ANALYZER_NONNULL = YES;
                       CLANG_ANALYZER_NUMBER_OBJECT_CONVERSION = YES_AGGRESSIVE;
                       CLANG_CXX_LANGUAGE_STANDARD = "c++14";
                       CURRENT_PROJECT_VERSION = 1;)delimiter");

        if (project.targetType == TargetType::GUIApplication)
        {
            builder.append(R"delimiter(
                       ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon;
                       ASSETCATALOG_COMPILER_GLOBAL_ACCENT_COLOR_NAME = AccentColor;
                       ASSETCATALOG_NOTICES = NO;
                       ASSETCATALOG_WARNINGS = NO;
                       CODE_SIGN_ENTITLEMENTS = {0}.entitlements;
                       CODE_SIGN_STYLE = Automatic;
                       GENERATE_INFOPLIST_FILE = YES;
                       INFOPLIST_KEY_NSHumanReadableCopyright = "";
                       INFOPLIST_KEY_UIRequiresFullScreen = NO;
                       INFOPLIST_KEY_UISupportedInterfaceOrientations = "UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown";
                       INFOPLIST_KEY_UILaunchStoryboardName = {0};
                       LD_RUNPATH_SEARCH_PATHS = (
                           "$(inherited)",
                           "@executable_path/../Frameworks",
                       );)delimiter",
                           project.name.view());
        }

        builder.append(R"delimiter(
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
                       CLANG_WARN_DUPLICATE_METHOD_MATCH = YES;
                       CLANG_WARN_EMPTY_BODY = YES;
                       CLANG_WARN_ENUM_CONVERSION = YES;
                       CLANG_WARN_EXIT_TIME_DESTRUCTORS = YES;
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
                       DEAD_CODE_STRIPPING = YES;
                       ENABLE_STRICT_OBJC_MSGSEND = YES;
                       ENABLE_USER_SCRIPT_SANDBOXING = NO;
                       GCC_C_LANGUAGE_STANDARD = gnu11;
                       GCC_NO_COMMON_BLOCKS = YES;
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
                       MACOSX_DEPLOYMENT_TARGET = 13.0;
                       IPHONEOS_DEPLOYMENT_TARGET = 14.3;
                       MARKETING_VERSION = 1.0;
                       MTL_ENABLE_DEBUG_INFO = NO;
                       MTL_FAST_MATH = YES;
                       SDKROOT = macosx;)delimiter");
        SC_COMPILER_WARNING_POP;
        return true;
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

        writeCommonOptions(builder, project);

        const Configuration* configuration = project.getConfiguration(xcodeObject.name.view());
        SC_TRY(configuration != nullptr);
        builder.append("\n                       CONFIGURATION_BUILD_DIR = \"");
        WriterInternal::appendPrefixIfRelativePosix("$(PROJECT_DIR)", builder, configuration->outputPath.view(),
                                                    relativeDirectories.relativeProjectsToOutputs.view());
        appendVariable(builder, configuration->outputPath.view());
        builder.append("\";");
        builder.append("\n                       SYMROOT = \"");
        WriterInternal::appendPrefixIfRelativePosix("$(PROJECT_DIR)", builder, configuration->intermediatesPath.view(),
                                                    relativeDirectories.relativeProjectsToIntermediates.view());
        appendVariable(builder, configuration->intermediatesPath.view());
        builder.append("\";");
        if (configuration->compile.enableRTTI)
        {
            builder.append("\n                       GCC_ENABLE_CPP_RTTI = YES;");
        }
        else
        {
            builder.append("\n                       GCC_ENABLE_CPP_RTTI = NO;");
        }
        if (configuration->compile.enableExceptions)
        {
            builder.append("\n                       GCC_ENABLE_CPP_EXCEPTIONS = YES;");
        }
        else
        {
            builder.append("\n                       GCC_ENABLE_CPP_EXCEPTIONS = NO;");
        }
        builder.append(R"delimiter(
                       OTHER_CFLAGS = (
                         "$(inherited)",
                         "-gen-cdb-fragment-path",
                         "\"$(SYMROOT)/CompilationDatabase\"",
                       );)delimiter");

        if (not resolve(project.files.compile, configuration->compile, &CompileFlags::enableStdCpp))
        {
            builder.append(R"delimiter(
                       OTHER_CPLUSPLUSFLAGS = (
                         "$(OTHER_CFLAGS)",
                         "-nostdinc++",
                       );)delimiter");
        }
        if (not resolve(project.link, configuration->link, &LinkFlags::enableStdCpp))
        {
            builder.append("\n                       OTHER_LDFLAGS = \"-nostdlib++\";");
        }
        switch (configuration->compile.optimizationLevel)
        {
        case Optimization::Debug:
            builder.append(R"delimiter(
                           COPY_PHASE_STRIP = NO;
                           ONLY_ACTIVE_ARCH = YES;
                           DEBUG_INFORMATION_FORMAT = dwarf;
                           ENABLE_TESTABILITY = YES;
                           GCC_DYNAMIC_NO_PIC = NO;
                           GCC_OPTIMIZATION_LEVEL = 0;)delimiter");
            break;
        case Optimization::Release:
            builder.append(R"delimiter(
                           COPY_PHASE_STRIP = YES;
                           DEBUG_INFORMATION_FORMAT = "dwarf-with-dsym";
                           ENABLE_NS_ASSERTIONS = NO;)delimiter");
            break;
        }

        writeDefines(builder, project, *configuration);
        writeincludes(builder, project);
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
        {0} /* {1} */ = {{
            isa = XCBuildConfiguration;
            buildSettings = {{
                CODE_SIGN_STYLE = Automatic;
                DEAD_CODE_STRIPPING = YES;
                PRODUCT_NAME = "$(TARGET_NAME)";
                INFOPLIST_KEY_NSHumanReadableCopyright = "";
                PRODUCT_BUNDLE_IDENTIFIER = "{2}";
                SUPPORTED_PLATFORMS = "iphoneos iphonesimulator macosx";
                SUPPORTS_MACCATALYST = NO;
                SUPPORTS_MAC_DESIGNED_FOR_IPHONE_IPAD = NO;
                TARGETED_DEVICE_FAMILY = "1,2";
            }};
            name = {1};
        }};)delimiter",
                               configuration.buildHash.view(), configuration.name.view(), project.name.view());
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

    [[nodiscard]] bool fillXCodeFiles(StringView projectDirectory, const Project& project,
                                      Vector<RenderItem>& outputFiles)
    {
        SC_TRY(WriterInternal::getPathsRelativeTo(projectDirectory, definitionCompiler, project, outputFiles));
        for (auto& it : outputFiles)
        {
            SC_TRY(computeReferenceHash(it.name.view(), it.referenceHash));
            SC_TRY(computeBuildHash(it.name.view(), it.buildHash));
        }
        return true;
    }

    [[nodiscard]] bool fillFrameworks(Vector<RenderItem>& xcodeObjects, const Vector<String>& frameworks,
                                      StringView platformFilter, bool framework)
    {
        for (const String& it : frameworks)
        {
            RenderItem xcodeFile;
            if (framework)
            {
                xcodeFile.name = Path::basename(it.view(), Path::AsPosix);
                SC_TRY(StringBuilder(xcodeFile.name, StringBuilder::DoNotClear).append(".framework"));
                xcodeFile.type = RenderItem::Framework;
                SC_TRY(Path::join(xcodeFile.path, {"System/Library/Frameworks", xcodeFile.name.view()}, "/"));
            }
            else
            {
                xcodeFile.name = "lib";
                StringBuilder sb(xcodeFile.name, StringBuilder::DoNotClear);
                SC_TRY(sb.append(Path::basename(it.view(), Path::AsPosix)));
                SC_TRY(sb.append(".tbd"));
                xcodeFile.type = RenderItem::SystemLibrary;
                SC_TRY(Path::join(xcodeFile.path, {"usr/lib", xcodeFile.name.view()}, "/"));
            }
            SC_TRY(computeBuildHash(xcodeFile.name.view(), xcodeFile.buildHash));
            SC_TRY(computeReferenceHash(xcodeFile.name.view(), xcodeFile.referenceHash));

            // TODO: De-hardcode thse ones
            if (not platformFilter.isEmpty())
            {
                SC_TRY(xcodeFile.platformFilters.push_back(platformFilter));
            }
            SC_TRY(xcodeObjects.push_back(move(xcodeFile)));
        }
        return Result(true);
    }

    [[nodiscard]] bool fillXCodeFrameworks(const Project& project, Vector<RenderItem>& xcodeObjects)
    {
        SC_TRY(fillFrameworks(xcodeObjects, project.link.frameworks, StringView(), true));
        SC_TRY(fillFrameworks(xcodeObjects, project.link.frameworksMacOS, "macos", true));
        SC_TRY(fillFrameworks(xcodeObjects, project.link.frameworksIOS, "ios", true));
        // TODO: Must differentiate between regular link libraries and "system link libraries" (.tbd)
        SC_TRY(fillFrameworks(xcodeObjects, project.link.libraries, StringView(), false));
        return Result(true);
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

        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable: break;
        case TargetType::GUIApplication: writePBXResourcesBuildPhase(builder, project); break;
        }

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
                                     StringView filename)
    {
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

    Result writeEntitlements(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_UNUSED(project);
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.app-sandbox</key>
    <false/>
    <key>com.apple.security.files.user-selected.read-only</key>
    <true/>
</dict>
</plist>
)delimiter");
        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    Result writeStoryboard(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_UNUSED(project);
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(R"delimiter(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
        <document type="com.apple.InterfaceBuilder3.CocoaTouch.Storyboard.XIB" version="3.0" toolsVersion="13122.16" targetRuntime="iOS.CocoaTouch" propertyAccessControl="none" useAutolayout="YES" launchScreen="YES" useTraitCollections="YES" useSafeAreas="YES" colorMatched="YES" initialViewController="01J-lp-oVM">
            <dependencies>
                <plugIn identifier="com.apple.InterfaceBuilder.IBCocoaTouchPlugin" version="13104.12"/>
                <capability name="Safe area layout guides" minToolsVersion="9.0"/>
                <capability name="documents saved in the Xcode 8 format" minToolsVersion="8.0"/>
            </dependencies>
            <scenes>
                <!--View Controller-->
                <scene sceneID="EHf-IW-A2E">
                    <objects>
                        <viewController id="01J-lp-oVM" sceneMemberID="viewController">
                            <view key="view" contentMode="scaleToFill" id="Ze5-6b-2t3">
                                <rect key="frame" x="0.0" y="0.0" width="375" height="667"/>
                                <autoresizingMask key="autoresizingMask" widthSizable="YES" heightSizable="YES"/>
                                <color key="backgroundColor" xcode11CocoaTouchSystemColor="systemBackgroundColor" cocoaTouchSystemColor="whiteColor"/>
                                <viewLayoutGuide key="safeArea" id="6Tk-OE-BBY"/>
                            </view>
                        </viewController>
                        <placeholder placeholderIdentifier="IBFirstResponder" id="iYj-Kq-Ea1" userLabel="First Responder" sceneMemberID="firstResponder"/>
                    </objects>
                    <point key="canvasLocation" x="53" y="375"/>
                </scene>
            </scenes>
        </document>
)delimiter");
        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    Result writeAssets(FileSystem& fs, const Project& project)
    {
        SmallString<255> buffer;
        StringBuilder    sb(buffer);
        SC_TRY(sb.format("{0}.xcassets", project.name));
        if (fs.existsAndIsDirectory(buffer.view()))
        {
            SC_TRY(fs.removeDirectoryRecursive(buffer.view()));
        }
        else
        {
            SC_TRY(fs.makeDirectory(buffer.view()));
        }
        SC_TRY(sb.format("{0}.xcassets/AccentColor.colorset", project.name));
        SC_TRY(fs.makeDirectoryRecursive(buffer.view()));
        SC_TRY(sb.format("{0}.xcassets/AccentColor.colorset/Contents.json", project.name));
        SC_TRY(fs.writeString(buffer.view(), R"delimiter(
{
  "colors" : [
    {
      "idiom" : "universal"
    }
  ],
  "info" : {
    "author" : "xcode",
    "version" : 1
  }
}
)delimiter"));
        SC_TRY(sb.format("{0}.xcassets/AppIcon.appiconset", project.name));
        SC_TRY(fs.makeDirectoryRecursive(buffer.view()));
        SC_TRY(sb.format("{0}.xcassets/AppIcon.appiconset/Contents.json", project.name));
        SC_TRY(fs.writeString(buffer.view(),
                              R"delimiter({
  "images" : [
    {
      "filename" : "AppIcon.svg",
      "idiom" : "universal",
      "platform" : "ios",
      "size" : "1024x1024"
    },
    {
      "filename" : "AppIcon.svg",
      "idiom" : "mac",
      "scale" : "2x",
      "size" : "512x512"
    }
  ],
  "info" : {
    "author" : "xcode",
    "version" : 1
  }
}
)delimiter"));
        SC_TRY(sb.format("{0}.xcassets/Contents.json", project.name));
        SC_TRY(fs.writeString(buffer.view(), R"delimiter(
{
    "info" : {
      "author" : "xcode",
      "version" : 1
    }
})delimiter"));

        SC_TRY(sb.format("{0}.xcassets/AppIcon.appiconset/AppIcon.svg", project.name));
        String fullIconPath;
        SC_TRY(Path::join(fullIconPath, {project.rootDirectory.view(), project.iconPath.view()}));
        SC_TRY(fs.copyFile(fullIconPath.view(), buffer.view()));

        return Result(true);
    }

    [[nodiscard]] bool appendVariable(StringBuilder& builder, StringView text)
    {
        const StringView relativeRoot = relativeDirectories.projectRootRelativeToProjects.view();

        const StringBuilder::ReplacePair replacements[] = {
            {"$(PROJECT_DIR)", "$(PROJECT_DIR)"},                    // Same
            {"$(PROJECT_ROOT)", relativeRoot},                       //
            {"$(CONFIGURATION)", "$(CONFIGURATION)"},                // Same
            {"$(PROJECT_NAME)", "$(PROJECT_NAME)"},                  // Same
            {"$(TARGET_OS)", "$(PLATFORM_DISPLAY_NAME)"},            //
            {"$(TARGET_OS_VERSION)", "$(MACOSX_DEPLOYMENT_TARGET)"}, //
            {"$(TARGET_ARCHITECTURES)", "$(ARCHS)"},                 //
            {"$(BUILD_SYSTEM)", "xcode"},                            //
            {"$(COMPILER)", "clang"},                                //
            {"$(COMPILER_VERSION)", "15"},                           // TODO: Detect apple-clang version
            {"\"", "\\\\\\\""},                                      // Escape double quotes
        };
        return builder.appendReplaceMultiple(text, {replacements, sizeof(replacements) / sizeof(replacements[0])});
    }
};
