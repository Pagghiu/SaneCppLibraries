// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystem/Path.h"
#include "../../Hashing/Hashing.h"
#include "../../Strings/StringBuilder.h"
#include "../Build.h"
#include "BuildWriter.h"

struct SC::Build::ProjectWriter::WriterVisualStudio
{
    const Definition&          definition;
    const DefinitionCompiler&  definitionCompiler;
    const Directories&         directories;
    const RelativeDirectories& relativeDirectories;

    Generator::Type generator;

    Hashing hashing;
    String  projectGuid;

    WriterVisualStudio(const Definition& definition, const DefinitionCompiler& definitionCompiler,
                       const Directories& directories, const RelativeDirectories& relativeDirectories,
                       Generator::Type generator)
        : definition(definition), definitionCompiler(definitionCompiler), directories(directories),
          relativeDirectories(relativeDirectories), generator(generator)
    {}

    [[nodiscard]] static bool generateGuidFor(const StringView name, Hashing& hashing, String& projectGuid)
    {
        SC_TRY(hashing.setType(Hashing::TypeSHA1));
        SC_TRY(hashing.add(name.toBytesSpan()));
        SC_TRY(hashing.add("_Guid"_a8.toBytesSpan()));
        Hashing::Result res;
        SC_TRY(hashing.getHash(res));
        String hexString;
        SC_TRY(StringBuilder(hexString).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
        StringBuilder guidBuilder(projectGuid);
        SC_TRY(guidBuilder.append("{"));
        SC_TRY(guidBuilder.append(hexString.view().sliceStartEnd(0, 8)));
        SC_TRY(guidBuilder.append("-"));
        SC_TRY(guidBuilder.append(hexString.view().sliceStartEnd(8, 12)));
        SC_TRY(guidBuilder.append("-"));
        SC_TRY(guidBuilder.append(hexString.view().sliceStartEnd(12, 16)));
        SC_TRY(guidBuilder.append("-"));
        SC_TRY(guidBuilder.append(hexString.view().sliceStartEnd(16, 20)));
        SC_TRY(guidBuilder.append("-"));
        SC_TRY(guidBuilder.append(hexString.view().sliceStartEnd(20, 32)));
        SC_TRY(guidBuilder.append("}"));
        return true;
    }

    [[nodiscard]] bool writeConfiguration(StringBuilder& builder, const Configuration& configuration,
                                          StringView platform)
    {
        return builder.append("    <ProjectConfiguration Include=\"{}|{}\">\n"
                              "      <Configuration>{}</Configuration>\n"
                              "      <Platform>{}</Platform>\n"
                              "    </ProjectConfiguration>\n",
                              configuration.name.view(), platform, configuration.name.view(), platform);
    }

    template <typename Lambda>
    [[nodiscard]] static Result forArchitecture(StringBuilder& builder, const Project& project, Lambda lambda)
    {
        for (const auto& config : project.configurations)
        {
            switch (config.architecture)
            {
            case Architecture::Any:
                SC_TRY(lambda(builder, project, config, "ARM64"));
                SC_TRY(lambda(builder, project, config, "Win32"));
                SC_TRY(lambda(builder, project, config, "x64"));
                break;
            case Architecture::Intel32: {
                SC_TRY(lambda(builder, project, config, "Win32"));
                break;
            }
            case Architecture::Intel64: {
                SC_TRY(lambda(builder, project, config, "x64"));
                break;
            }
            case Architecture::Arm64: {
                SC_TRY(lambda(builder, project, config, "ARM64"));
                break;
            }
            case Architecture::Wasm: {
                return Result::Error("Visual Studio: Unsupported Wasm configuration");
            }
            }
        }
        return Result(true);
    }

    [[nodiscard]] Result writeConfigurations(StringBuilder& builder, const Project& project)
    {
        return forArchitecture(
            builder, project,
            [this](StringBuilder& builder, const Project&, const Configuration& configuration, StringView platform)
            { return writeConfiguration(builder, configuration, platform); });
    }

    [[nodiscard]] Result writeGlobals(StringBuilder& builder, const Project& project)
    {
        // TODO: Generate GUID
        // c701ae36-fa88-4674-a16f-298fa8444aa5
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("  <PropertyGroup Label=\"Globals\">\n"
                       "    <VCProjectVersion>16.0</VCProjectVersion>\n"
                       "    <Keyword>Win32Proj</Keyword>\n"
                       "    <ProjectGuid>{}</ProjectGuid>\n"
                       "    <RootNamespace>{}</RootNamespace>\n"
                       "    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n"
                       "  </PropertyGroup>\n",
                       projectGuid, project.name);
        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    [[nodiscard]] bool writeConfigurationProperty(StringBuilder& builder, const Configuration& configuration,
                                                  StringView architecture)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;

        StringView platformToolset = configuration.visualStudio.platformToolset;
        if (platformToolset.isEmpty())
        {
            if (generator == Build::Generator::VisualStudio2022)
            {
                platformToolset = "v143";
            }
            else
            {
                platformToolset = "v142";
            }
        }
        builder.append(
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='{}|{}'\" Label=\"Configuration\">\n",
            configuration.name, architecture);

        switch (configuration.compile.optimizationLevel)
        {
        case Optimization::Debug:
            builder.append("    <ConfigurationType>Application</ConfigurationType>\n"
                           "    <UseDebugLibraries>true</UseDebugLibraries>\n"
                           "    <PlatformToolset>{}</PlatformToolset>\n"
                           "    <CharacterSet>Unicode</CharacterSet>\n",
                           platformToolset);
            break;
        case Optimization::Release:
            builder.append("    <ConfigurationType>Application</ConfigurationType>\n"
                           "    <UseDebugLibraries>false</UseDebugLibraries>\n"
                           "    <PlatformToolset>{}</PlatformToolset>\n"
                           "    <WholeProgramOptimization>true</WholeProgramOptimization>\n"
                           "    <CharacterSet>Unicode</CharacterSet>\n",
                           platformToolset);
            break;
        }

#if 0
        // TODO: Temporarily disabled as ASAN crashes at startup in VS2022 17.7.1
        if(configuration.compile.hasValue<Compile::enableASAN>(true))
        {
            // VS ASAN is unsupported on ARM64 and needs manual flags / libs with ClangCL toolset
            if(architecture != "ARM64" and platformToolset != "ClangCL")
            {
                builder.append("    <EnableASAN>true</EnableASAN>\n");
            }
        }
#endif
        builder.append("  </PropertyGroup>\n");
        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    [[nodiscard]] bool writeConfigurationsProperties(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        return forArchitecture(
            builder, project,
            [this](StringBuilder& builder, const Project&, const Configuration& configuration, StringView platform)
            { return writeConfigurationProperty(builder, configuration, platform); });
        SC_COMPILER_WARNING_POP;
    }

    [[nodiscard]] bool writePropertySheet(StringBuilder& builder, const Configuration& configuration,
                                          StringView architecture)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append(
            "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='{}|{}'\">\n"
            "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" "
            "Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" "
            "Label=\"LocalAppDataPlatform\" />\n"
            "  </ImportGroup>\n",
            configuration.name.view(), architecture);
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writePropertySheets(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        return forArchitecture(
            builder, project,
            [this](StringBuilder& builder, const Project&, const Configuration& configuration, StringView platform)
            { return writePropertySheet(builder, configuration, platform); });
        SC_COMPILER_WARNING_POP;
    }

    [[nodiscard]] bool writePropertyGroup(StringBuilder& builder, const Project& project,
                                          const Configuration& configuration, StringView architecture)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='{}|{}'\">\n",
                       configuration.name.view(), architecture);
        if (not project.targetName.view().isEmpty())
        {
            builder.append("    <TargetName>{}</TargetName>\n", project.targetName);
        }

        if (not configuration.outputPath.isEmpty())
        {
            builder.append("    <OutDir>");
            WriterInternal::appendPrefixIfRelativeMSVC("$(ProjectDir)", builder, configuration.outputPath.view(),
                                                       relativeDirectories.relativeProjectsToOutputs.view());
            appendVariable(builder, configuration.outputPath.view());
            if (not configuration.outputPath.view().endsWithAnyOf({'\\'}))
            {
                builder.append("\\");
            }
            builder.append("</OutDir>\n");
        }
        if (not configuration.intermediatesPath.isEmpty())
        {
            builder.append("    <IntDir>");
            WriterInternal::appendPrefixIfRelativeMSVC("$(ProjectDir)", builder, configuration.intermediatesPath.view(),
                                                       relativeDirectories.relativeProjectsToIntermediates.view());
            appendVariable(builder, configuration.intermediatesPath.view());
            if (not configuration.outputPath.view().endsWithAnyOf({'\\'}))
            {
                builder.append("\\");
            }
            builder.append("</IntDir>\n");
        }

        if (not configuration.compile.includePaths.isEmpty() or not project.compile.includePaths.isEmpty())
        {
            builder.append("    <IncludePath>");
            for (const String& it : configuration.compile.includePaths)
            {
                SC_TRY(appendProjectRelative(builder, it.view()));
                builder.append(";");
            }
            for (const String& it : project.compile.includePaths)
            {
                SC_TRY(appendProjectRelative(builder, it.view()));
                builder.append(";");
            }
            builder.append("$(IncludePath)</IncludePath>\n");
        }
        builder.append("  </PropertyGroup>\n");

        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writePropertyGroups(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        return forArchitecture(builder, project,
                               [this](StringBuilder& builder, const Project& project,
                                      const Configuration& configuration, StringView platform)
                               { return writePropertyGroup(builder, project, configuration, platform); });
        SC_COMPILER_WARNING_POP;
    }

    [[nodiscard]] bool writeItemDefinitionGroup(StringBuilder& builder, const Project& project,
                                                const Configuration& configuration, StringView architecture)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='{}|{}'\">\n",
                       configuration.name.view(), architecture);
        builder.append("    <ClCompile>\n");
        builder.append("      <WarningLevel>Level4</WarningLevel>\n");
        builder.append("      <SDLCheck>true</SDLCheck>\n");

        //  TODO: This can be refactored
        if (not configuration.compile.defines.isEmpty() or not project.compile.defines.isEmpty())
        {
            builder.append("    <PreprocessorDefinitions>");
            for (const String& it : configuration.compile.defines)
            {
                SC_TRY(appendVariable(builder, it.view()));
                builder.append(";");
            }
            for (const String& it : project.compile.defines)
            {
                SC_TRY(appendVariable(builder, it.view()));
                builder.append(";");
            }
            builder.append("%(PreprocessorDefinitions)</PreprocessorDefinitions>\n");
        }

        builder.append("      <ConformanceMode>true</ConformanceMode>\n");
        builder.append("      <ExceptionHandling>false</ExceptionHandling>\n");
        builder.append("      <UseFullPaths>false</UseFullPaths>\n");
        builder.append("      <TreatWarningAsError>true</TreatWarningAsError>\n");
        if (configuration.compile.enableExceptions)
        {
            builder.append("      <ExceptionHandling>true</ExceptionHandling>\n");
        }
        else
        {
            builder.append("      <ExceptionHandling>false</ExceptionHandling>\n");
        }
        if (configuration.compile.enableRTTI)
        {
            builder.append("      <RuntimeTypeInfo>true</RuntimeTypeInfo>\n");
        }
        else
        {
            builder.append("      <RuntimeTypeInfo>false</RuntimeTypeInfo>\n");
        }
        switch (configuration.compile.optimizationLevel)
        {
        case Optimization::Debug: builder.append("      <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>\n"); break;
        case Optimization::Release: builder.append("      <RuntimeLibrary>MultiThreaded</RuntimeLibrary>\n"); break;
        }

        builder.append("      <MultiProcessorCompilation>true</MultiProcessorCompilation>\n");
        builder.append("    </ClCompile>\n");
        builder.append("    <Link>\n");
        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable: builder.append("      <SubSystem>Console</SubSystem>\n"); break;
        case TargetType::GUIApplication: builder.append("      <SubSystem>Windows</SubSystem>\n"); break;
        }

        switch (configuration.compile.optimizationLevel)
        {
        case Optimization::Debug:
            builder.append("      <GenerateDebugInformation>true</GenerateDebugInformation>\n");
            break;
        case Optimization::Release: break;
        }
        builder.append("    </Link>\n");
        builder.append("  </ItemDefinitionGroup>\n");

        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeItemDefinitionGroups(StringBuilder& builder, const Project& project)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        return forArchitecture(builder, project,
                               [this](StringBuilder& builder, const Project& project,
                                      const Configuration& configuration, StringView platform)
                               { return writeItemDefinitionGroup(builder, project, configuration, platform); });
        SC_COMPILER_WARNING_POP;
    }
    using RenderItem  = WriterInternal::RenderItem;
    using RenderGroup = WriterInternal::RenderGroup;
    using Renderer    = WriterInternal::Renderer;

    [[nodiscard]] bool writeSourceFiles(StringBuilder& builder, const Project& project, Vector<RenderItem>& files)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        SC_COMPILER_UNUSED(project);
        builder.append("  <ItemGroup>\n");
        for (auto& it : files)
        {
            if (it.type == WriterInternal::RenderItem::CppFile or it.type == WriterInternal::RenderItem::CFile)
            {
                builder.append("    <ClCompile Include=\"{}\" />\n", it.path);
            }
        }
        builder.append("  </ItemGroup>\n");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeHeaderFiles(StringBuilder& builder, Vector<RenderItem>& files)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("  <ItemGroup>\n");
        for (auto& it : files)
        {
            if (it.type == WriterInternal::RenderItem::HeaderFile)
            {
                builder.append("    <ClInclude Include=\"{}\" />\n", it.path);
            }
        }
        builder.append("  </ItemGroup>\n");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeInlineFiles(StringBuilder& builder, Vector<RenderItem>& files)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("  <ItemGroup>\n");
        for (auto& it : files)
        {
            if (it.type == WriterInternal::RenderItem::InlineFile)
            {
                builder.append("    <None Include=\"{}\" />\n", it.path);
            }
        }
        builder.append("  </ItemGroup>\n");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeNatvisFiles(StringBuilder& builder, Vector<RenderItem>& files)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("  <ItemGroup>\n");
        for (auto& it : files)
        {
            if (it.type == WriterInternal::RenderItem::DebugVisualizerFile)
            {
                builder.append("    <Natvis Include=\"{}\" />\n", it.path);
            }
        }
        builder.append("  </ItemGroup>\n");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] Result prepare(const Project& project, Renderer& renderer)
    {
        renderer.renderItems.clear();
        SC_TRY(fillVisualStudioFiles(directories.projectsDirectory.view(), project, renderer.renderItems));
        return Result(true);
    }

    [[nodiscard]] Result fillVisualStudioFiles(StringView projectDirectory, const Project& project,
                                               Vector<RenderItem>& outputFiles)
    {
        SC_TRY(WriterInternal::getPathsRelativeTo(projectDirectory, definitionCompiler, project, outputFiles));
        return Result(true);
    }

    // Project
    [[nodiscard]] Result writeProject(StringBuilder& builder, const Project& project, Renderer& renderer)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
        builder.append(
            "<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n");
        builder.append("  <ItemGroup Label=\"ProjectConfigurations\">\n");
        SC_TRY(writeConfigurations(builder, project));
        builder.append("  </ItemGroup>\n");

        SC_TRY(writeGlobals(builder, project));
        builder.append("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n");
        SC_TRY(writeConfigurationsProperties(builder, project));
        SC_TRY(writePropertySheets(builder, project));
        builder.append("  <PropertyGroup Label=\"UserMacros\" />\n");

        builder.append("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n"
                       "  <ImportGroup Label=\"ExtensionSettings\">\n"
                       "  </ImportGroup>\n"
                       "  <ImportGroup Label=\"Shared\">\n"
                       "  </ImportGroup>\n");

        SC_TRY(writePropertyGroups(builder, project));
        SC_TRY(writeItemDefinitionGroups(builder, project));
        SC_TRY(writeSourceFiles(builder, project, renderer.renderItems));
        SC_TRY(writeHeaderFiles(builder, renderer.renderItems));
        SC_TRY(writeInlineFiles(builder, renderer.renderItems));
        SC_TRY(writeNatvisFiles(builder, renderer.renderItems));

        builder.append("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n"
                       "  <ImportGroup Label=\"ExtensionTargets\">\n"
                       "  </ImportGroup>\n"
                       "</Project>\n");
        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    // Solution
    [[nodiscard]] static bool writeSolution(StringBuilder& builder, Span<const Project> projects,
                                            const Span<const String> projectGuids)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("Microsoft Visual Studio Solution File, Format Version 12.00\n"
                       "# Visual Studio Version 17\n"
                       "VisualStudioVersion = 17.4.32916.344\n"
                       "MinimumVisualStudioVersion = 10.0.40219.1\n");
        String prjName;
        for (size_t idx = 0; idx < projects.sizeInElements(); ++idx)
        {
            const Project&   project     = projects[idx];
            const StringView projectGuid = projectGuids[idx].view();
            SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.vcxproj", project.name));

            builder.append("Project(\"{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}\") = \"{}\", \"{}\", \"{}\"\n"
                           "EndProject\n",
                           project.name, Path::basename(prjName.view(), Path::AsPosix), projectGuid);
        }

        builder.append("Global\n");
        builder.append("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n");

        for (size_t idx = 0; idx < projects.sizeInElements(); ++idx)
        {
            const Project& project = projects[idx];

            forArchitecture(builder, project,
                            [](StringBuilder& builder, const Project& project, const Configuration& configuration,
                               StringView platform)

                            {
                                SC_COMPILER_UNUSED(project);
                                builder.append("\t\t{}|{} = {}|{}\n", configuration.name, platform, configuration.name,
                                               platform);
                                return true;
                            });
        }

        builder.append("\tEndGlobalSection\n");

        builder.append("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n");

        for (size_t idx = 0; idx < projects.sizeInElements(); ++idx)
        {
            const Project&   project     = projects[idx];
            const StringView projectGuid = projectGuids[idx].view();
            forArchitecture(builder, project,
                            [&projectGuid](StringBuilder& builder, const Project& project,
                                           const Configuration& configuration, StringView platform)

                            {
                                SC_COMPILER_UNUSED(project);
                                builder.append("\t\t{}.{}|{}.ActiveCfg = {}|{}\n", projectGuid, configuration.name,
                                               platform, configuration.name, platform);
                                builder.append("\t\t{}.{}|{}.Build.0 = {}|{}\n", projectGuid, configuration.name,
                                               platform, configuration.name, platform);
                                return true;
                            });
        }
        builder.append("\tEndGlobalSection\n");

        builder.append("\tGlobalSection(SolutionProperties) = preSolution\n"
                       "\t\tHideSolutionNode = FALSE\n"
                       "\tEndGlobalSection\n"
                       "\tGlobalSection(ExtensibilityGlobals) = postSolution\n"
                       "\t\tSolutionGuid = {{2AC4A6F0-76E3-49A8-BFAF-FE2DBD0D9D02}}\n"
                       "\tEndGlobalSection\n");

        builder.append("EndGlobal");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    // Filters
    [[nodiscard]] bool fillFileGroups(RenderGroup& group, const Vector<RenderItem>& xcodeFiles)
    {
        SC_TRY(group.referenceHash.assign("None"));
        SC_TRY(group.name.assign("/"));
        for (const auto& file : xcodeFiles)
        {
            StringViewTokenizer tokenizer(file.referencePath.view());
            RenderGroup*        current = &group;
            while (tokenizer.tokenizeNext('/', StringViewTokenizer::SkipEmpty))
            {
                if (tokenizer.isFinished())
                    break;
                current = current->children.getOrCreate(tokenizer.component);
                if (current->name.isEmpty())
                {
                    current->name = Path::removeStartingSeparator(tokenizer.processed);
                    SC_TRY(generateGuidFor(tokenizer.processed, hashing, current->referenceHash));
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool writeFileFilters(StringBuilder& builder, Renderer& renderer)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        builder.append("  <ItemGroup>\n");
        for (auto& it : renderer.renderItems)
        {
            const StringView dir = Path::removeStartingSeparator(Path::dirname(it.referencePath.view(), Path::AsPosix));
            if (it.type == WriterInternal::RenderItem::HeaderFile)
            {
                builder.append("    <ClInclude Include=\"{}\">\n", it.path);
                builder.append("      <Filter>");
                builder.appendReplaceAll(dir, "/", "\\");
                builder.append("</Filter>\n");
                builder.append("    </ClInclude>\n");
            }
        }
        builder.append("  </ItemGroup>\n");
        builder.append("  <ItemGroup>\n");
        for (auto& it : renderer.renderItems)
        {
            const StringView dir = Path::removeStartingSeparator(Path::dirname(it.referencePath.view(), Path::AsPosix));
            if (it.type == WriterInternal::RenderItem::CppFile or it.type == WriterInternal::RenderItem::CFile)
            {
                builder.append("    <ClCompile Include=\"{}\">\n", it.path);
                builder.append("      <Filter>");
                builder.appendReplaceAll(dir, "/", "\\");
                builder.append("</Filter>\n");
                builder.append("    </ClCompile>\n");
            }
        }
        builder.append("  </ItemGroup>\n");
        builder.append("  <ItemGroup>\n");
        for (auto& it : renderer.renderItems)
        {
            const StringView dir = Path::removeStartingSeparator(Path::dirname(it.referencePath.view(), Path::AsPosix));
            if (it.type == WriterInternal::RenderItem::InlineFile)
            {
                builder.append("    <None Include=\"{}\">\n", it.path);
                builder.append("      <Filter>");
                builder.appendReplaceAll(dir, "/", "\\");
                builder.append("</Filter>\n");
                builder.append("    </None>\n");
            }
        }
        builder.append("  </ItemGroup>\n");
        builder.append("  <ItemGroup>\n");
        for (auto& it : renderer.renderItems)
        {
            const StringView dir = Path::removeStartingSeparator(Path::dirname(it.referencePath.view(), Path::AsPosix));
            if (it.type == WriterInternal::RenderItem::DebugVisualizerFile)
            {
                builder.append("    <Natvis Include=\"{}\">\n", it.path);
                builder.append("      <Filter>");
                builder.appendReplaceAll(dir, "/", "\\");
                builder.append("</Filter>\n");
                builder.append("    </Natvis>\n");
            }
        }
        builder.append("  </ItemGroup>\n");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeFiltersFolder(StringBuilder& builder, const RenderGroup& folder)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        for (auto& it : folder.children)
        {
            builder.append("    <Filter Include=\"");
            builder.appendReplaceAll(it.value.name.view(), "/", "\\");
            builder.append("\">\n");
            builder.append("      <UniqueIdentifier>{}</UniqueIdentifier>\n", it.value.referenceHash);
            builder.append("    </Filter>\n");
        }
        for (auto& it : folder.children)
        {
            SC_TRY(writeFiltersFolder(builder, it.value));
        }
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool writeFilters(StringBuilder& builder, Renderer& renderer)
    {
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        SC_TRY(fillFileGroups(renderer.rootGroup, renderer.renderItems));
        builder.append("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
        builder.append(
            "<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n");
        builder.append("  <ItemGroup>\n");
        writeFiltersFolder(builder, renderer.rootGroup);
        builder.append("  </ItemGroup>\n");
        writeFileFilters(builder, renderer);
        builder.append("</Project>");
        SC_COMPILER_WARNING_POP;
        return true;
    }

    [[nodiscard]] bool appendProjectRelative(StringBuilder& builder, StringView text)
    {
        if (Path::isAbsolute(text, Path::AsNative))
        {
            String relative;
            SC_TRY(Path::relativeFromTo(directories.projectsDirectory.view(), text, relative, Path::AsNative,
                                        Path::AsWindows));
            SC_TRY(builder.append("$(ProjectDir){}\\", relative));
            return true;
        }
        else
        {
            SC_TRY(builder.append("$(ProjectDir){}\\", relativeDirectories.relativeProjectsToProjectRoot));
            return appendVariable(builder, text);
        }
    }

    [[nodiscard]] bool appendVariable(StringBuilder& builder, StringView text)
    {
        const StringView relativeRoot = relativeDirectories.projectRootRelativeToProjects.view();

        const StringBuilder::ReplacePair replacements[] = {
            {"/", "\\"},                                                 //
            {"$(PROJECT_DIR)\\", "$(ProjectDir)"},                       //
            {"$(PROJECT_ROOT)", relativeRoot},                           //
            {"$(CONFIGURATION)", "$(Configuration)"},                    //
            {"$(PROJECT_NAME)", "$(ProjectName)"},                       //
            {"$(TARGET_OS)", "windows"},                                 // $(SDKIdentifier)
            {"$(TARGET_OS_VERSION)", "$(WindowsTargetPlatformVersion)"}, //
            {"$(TARGET_ARCHITECTURES)", "$(PlatformTarget)"},            //
            {"$(BUILD_SYSTEM)", "msbuild"},                              //
            {"$(COMPILER)", "msvc"},                                     //
            {"$(COMPILER_VERSION)", "17"},                               // TODO: Detect MSVC version
        };
        return builder.appendReplaceMultiple(text, {replacements, sizeof(replacements) / sizeof(replacements[0])});
    }
};
