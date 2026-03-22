// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "BuildWriter.h"

#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringBuilder.h"

struct SC::Build::NativeBuild
{
    struct Variables
    {
        String projectName;
        String projectRoot;
        String targetOS;
        String targetArchitectures;
        String buildSystem;
        String compiler;
        String configuration;
    };

    struct CompilerAdapter
    {
        Toolchain::Family family = Toolchain::HostDefault;

        String executableC;
        String executableCpp;
        String executableLink;
        String executableArchive;

        StringView subcommandC;
        StringView subcommandCpp;
        StringView subcommandLink;
        StringView subcommandArchive;

        String displayName;

        [[nodiscard]] bool isClangLike() const { return family == Toolchain::Clang or family == Toolchain::ZigCC; }
    };

    struct ResolvedSource
    {
        WriterInternal::RenderItem::Type type = WriterInternal::RenderItem::Unknown;

        String displayPath;
        String sourcePath;
        String objectPath;
        String dependencyPath;
        String commandPath;
        String responsePath;

        CompileFlags compileFlags;
    };

    struct ResolvedProject
    {
        const Parameters*    parameters    = nullptr;
        const Workspace*     workspace     = nullptr;
        const Project*       project       = nullptr;
        const Configuration* configuration = nullptr;

        Variables       variables;
        CompilerAdapter adapter;
        CompileFlags    compileFlags;
        LinkFlags       linkFlags;

        String targetDirectory;
        String intermediateDirectory;
        String executablePath;
        String linkCommandPath;
        String linkResponsePath;
        String compileCommandsPath;
        String workspaceCompileCommandsPath;

        Vector<ResolvedSource> sources;
    };

    struct CommandLine
    {
        Vector<String> arguments;

        Result append(StringView value)
        {
            String item = StringEncoding::Utf8;
            SC_TRY(item.assign(value));
            SC_TRY(arguments.push_back(move(item)));
            return Result(true);
        }

        [[nodiscard]] size_t size() const { return arguments.size(); }

        [[nodiscard]] size_t totalCharacters() const
        {
            size_t total = 0;
            for (const String& argument : arguments)
            {
                total += argument.view().sizeInBytes();
            }
            return total;
        }

        template <size_t N>
        Result toViews(StringSpan (&views)[N], Span<const StringSpan>& outViews) const
        {
            SC_TRY_MSG(arguments.size() <= N, "Command line exceeds internal argument limit");
            for (size_t idx = 0; idx < arguments.size(); ++idx)
            {
                views[idx] = arguments[idx].view();
            }
            outViews = {views, arguments.size()};
            return Result(true);
        }
    };

    static Result execute(Action::ConfigureFunction configure, const Action& action, String* outputExecutable)
    {
        SC_TRY_MSG(action.parameters.platform != Platform::Windows,
                   "Native backend on Windows is planned but not implemented yet");

        Definition definition;
        SC_TRY(configure(definition, action.parameters));

        FilePathsResolver filePathsResolver;
        SC_TRY(filePathsResolver.resolve(definition));

        const Workspace* workspace = nullptr;
        SC_TRY(findWorkspace(definition, action.workspaceName, workspace));

        Vector<ResolvedProject> resolvedProjects;
        if (action.allTargets)
        {
            for (const Project& project : workspace->projects)
            {
                const Configuration* configuration = project.getConfiguration(action.configurationName);
                SC_TRY_MSG(configuration != nullptr, "Cannot find requested configuration");
                ResolvedProject resolvedProject;
                SC_TRY(resolveProject(action.parameters, *workspace, project, *configuration, filePathsResolver,
                                      resolvedProject));
                SC_TRY(resolvedProjects.push_back(move(resolvedProject)));
            }
        }
        else
        {
            const Project*       project       = nullptr;
            const Configuration* configuration = nullptr;
            SC_TRY(findProjectConfiguration(*workspace, action.projectName, action.configurationName, project,
                                            configuration));
            ResolvedProject resolvedProject;
            SC_TRY(resolveProject(action.parameters, *workspace, *project, *configuration, filePathsResolver,
                                  resolvedProject));
            SC_TRY(resolvedProjects.push_back(move(resolvedProject)));
        }

        if (action.action == Action::Print)
        {
            SC_TRY_MSG(resolvedProjects.size() == 1, "Print requires selecting a single project");
            SC_TRY(outputExecutable != nullptr);
            return Result(outputExecutable->assign(resolvedProjects[0].executablePath.view()));
        }

        FileSystem fs;
        SC_TRY(fs.init("."));
        SC_TRY(fs.makeDirectoryRecursive(action.parameters.directories.outputsDirectory.view()));
        SC_TRY(fs.makeDirectoryRecursive(action.parameters.directories.intermediatesDirectory.view()));
        SC_TRY(fs.makeDirectoryRecursive(action.parameters.directories.buildCacheDirectory.view()));

        Vector<String> workspaceCompileCommands;
        for (ResolvedProject& resolvedProject : resolvedProjects)
        {
            SC_TRY(writeCompileCommands(fs, resolvedProject, workspaceCompileCommands));
            SC_TRY(buildProject(fs, resolvedProject));
        }

        if (not workspaceCompileCommands.isEmpty())
        {
            SC_TRY(writeCompileCommandsArray(fs, resolvedProjects[0].workspaceCompileCommandsPath.view(),
                                             workspaceCompileCommands.toSpanConst()));
        }

        if (action.action == Action::Run)
        {
            SC_TRY_MSG(resolvedProjects.size() == 1, "Run requires selecting a single project");
            SC_TRY_MSG(resolvedProjects[0].project->targetType != TargetType::StaticLibrary,
                       "Run requires an executable target");
            SC_TRY(runExecutable(resolvedProjects[0].executablePath.view(), action));
        }
        return Result(true);
    }

    static Result buildProject(FileSystem& fs, ResolvedProject& resolvedProject)
    {
        size_t compileStep    = 0;
        bool   anyObjectBuilt = false;
        for (ResolvedSource& source : resolvedProject.sources)
        {
            compileStep++;

            CommandLine commandLine;
            String      commandString = StringEncoding::Utf8;
            SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
            SC_TRY(formatCommandLine(commandLine, commandString));
            if (not shouldRebuildObject(fs, resolvedProject, source, commandString.view()))
            {
                continue;
            }

            SC_TRY(makeParentDirectory(fs, source.objectPath.view()));
            globalConsole->print("[{}/{}] {} {}\n", compileStep, resolvedProject.sources.size() + 1,
                                 getCompileStepName(source.type), source.displayPath.view());
            globalConsole->flush();

            String stdOut = StringEncoding::Utf8;
            String stdErr = StringEncoding::Utf8;
            SC_TRY(
                executeCommand(fs, commandLine, source.responsePath.view(), resolvedProject.adapter, stdOut, stdErr));
            if (resolvedProject.parameters->execution.verbose and not stdOut.isEmpty())
            {
                globalConsole->print(stdOut.view());
            }
            if (not stdErr.isEmpty())
            {
                globalConsole->printError(stdErr.view());
                globalConsole->flushStdErr();
            }
            SC_TRY(fs.writeString(source.commandPath.view(), commandString.view()));
            anyObjectBuilt = true;
        }

        CommandLine finalCommand;
        String      finalCommandString = StringEncoding::Utf8;
        SC_TRY(buildFinalCommand(resolvedProject, finalCommand));
        SC_TRY(formatCommandLine(finalCommand, finalCommandString));
        if (shouldRebuildTargetArtifact(fs, resolvedProject, finalCommandString.view(), anyObjectBuilt))
        {
            globalConsole->print("[{}/{}] {} {}\n", resolvedProject.sources.size() + 1,
                                 resolvedProject.sources.size() + 1, getFinalStepName(*resolvedProject.project),
                                 resolvedProject.project->targetName.view());
            globalConsole->flush();

            String stdOut = StringEncoding::Utf8;
            String stdErr = StringEncoding::Utf8;
            SC_TRY(makeParentDirectory(fs, resolvedProject.executablePath.view()));
            SC_TRY(executeCommand(fs, finalCommand, finalResponsePath(resolvedProject), resolvedProject.adapter, stdOut,
                                  stdErr));
            if (resolvedProject.parameters->execution.verbose and not stdOut.isEmpty())
            {
                globalConsole->print(stdOut.view());
            }
            if (not stdErr.isEmpty())
            {
                globalConsole->printError(stdErr.view());
                globalConsole->flushStdErr();
            }
            SC_TRY(fs.writeString(resolvedProject.linkCommandPath.view(), finalCommandString.view()));
        }

        return Result(true);
    }

    static Result runExecutable(StringView executablePath, const Action& action)
    {
        Process    process;
        StringSpan arguments[64];
        size_t     numArguments = 1;
        arguments[0]            = StringView(executablePath).trimWhiteSpaces();
        globalConsole->print("COMMAND = {}\n", arguments[0]);
        for (size_t idx = 0; idx < action.additionalArguments.sizeInElements(); ++idx)
        {
            if (numArguments == sizeof(arguments) / sizeof(arguments[0]))
            {
                globalConsole->printLine("Exceeded max number of arguments that can be passed to the executable");
                break;
            }
            arguments[numArguments] = action.additionalArguments[idx];
            globalConsole->print("ARGS[{}] = {}\n", idx, arguments[numArguments]);
            numArguments++;
        }
        globalConsole->flush();
        SC_TRY(process.exec({arguments, numArguments}));
        SC_TRY_MSG(process.getExitStatus() == 0, "Run exited with non zero status");
        return Result(true);
    }

    static Result executeCommand(FileSystem& fs, CommandLine& commandLine, StringView responsePath,
                                 const CompilerAdapter& adapter, String& stdOut, String& stdErr)
    {
        SC_TRY(maybeWriteResponseFile(fs, commandLine, responsePath, adapter));

        StringSpan             views[128];
        Span<const StringSpan> args;
        SC_TRY(commandLine.toViews(views, args));

        Process process;
        SC_TRY(process.exec(args, stdOut, Process::StdIn(), stdErr));
        if (process.getExitStatus() != 0)
        {
            if (not stdOut.isEmpty())
            {
                globalConsole->print(stdOut.view());
            }
            if (not stdErr.isEmpty())
            {
                globalConsole->printError(stdErr.view());
                globalConsole->flushStdErr();
            }
            return Result::Error("Native backend command failed");
        }
        return Result(true);
    }

    static Result maybeWriteResponseFile(FileSystem& fs, CommandLine& commandLine, StringView responsePath,
                                         const CompilerAdapter& adapter)
    {
        if (responsePath.isEmpty())
            return Result(true);
        if (adapter.family == Toolchain::ZigCC)
            return Result(true);
        if (not(commandLine.size() > 48 or commandLine.totalCharacters() > 4096))
            return Result(true);

        String contents = StringEncoding::Utf8;
        for (size_t idx = 1; idx < commandLine.arguments.size(); ++idx)
        {
            if (idx > 1)
            {
                SC_TRY(StringBuilder::createForAppendingTo(contents).append("\n"));
            }
            SC_TRY(appendQuoted(contents, commandLine.arguments[idx].view()));
        }
        SC_TRY(makeParentDirectory(fs, responsePath));
        SC_TRY(fs.writeString(responsePath, contents.view()));

        String executable = StringEncoding::Utf8;
        SC_TRY(executable.assign(commandLine.arguments[0].view()));
        CommandLine reduced;
        SC_TRY(reduced.append(executable.view()));
        if (commandLine.arguments.size() > 1)
        {
            const StringView secondArgument = commandLine.arguments[1].view();
            if (secondArgument == adapter.subcommandC or secondArgument == adapter.subcommandCpp or
                secondArgument == adapter.subcommandLink)
            {
                SC_TRY(reduced.append(secondArgument));
            }
        }
        String responseArgument = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(responseArgument, "@{}", responsePath));
        SC_TRY(reduced.append(responseArgument.view()));
        commandLine = move(reduced);
        return Result(true);
    }

    static Result resolveProject(const Parameters& parameters, const Workspace& workspace, const Project& project,
                                 const Configuration& configuration, const FilePathsResolver& filePathsResolver,
                                 ResolvedProject& resolvedProject)
    {
        resolvedProject.parameters    = &parameters;
        resolvedProject.workspace     = &workspace;
        resolvedProject.project       = &project;
        resolvedProject.configuration = &configuration;

        SC_TRY(resolveCompilerAdapter(parameters.toolchain, parameters.platform, parameters.architecture,
                                      resolvedProject.adapter));
        SC_TRY(fillVariables(parameters, project, configuration, resolvedProject.adapter.displayName.view(),
                             resolvedProject.variables));

        const CompileFlags* compileOpinions[] = {&configuration.compile, &project.files.compile};
        SC_TRY(CompileFlags::merge(compileOpinions, resolvedProject.compileFlags));

        const LinkFlags* linkOpinions[] = {&configuration.link, &project.link};
        SC_TRY(LinkFlags::merge(linkOpinions, resolvedProject.linkFlags));

        SC_TRY(expandConfiguredPath(parameters.directories.outputsDirectory.view(), configuration.outputPath.view(),
                                    resolvedProject.variables, resolvedProject.targetDirectory));
        SC_TRY(expandConfiguredPath(parameters.directories.intermediatesDirectory.view(),
                                    configuration.intermediatesPath.view(), resolvedProject.variables,
                                    resolvedProject.intermediateDirectory));
        String artifactName = StringEncoding::Utf8;
        SC_TRY(computeArtifactName(project, artifactName));
        SC_TRY(
            Path::join(resolvedProject.executablePath, {resolvedProject.targetDirectory.view(), artifactName.view()}));
        SC_TRY(Path::join(resolvedProject.linkCommandPath,
                          {resolvedProject.intermediateDirectory.view(), "link.command"}));
        SC_TRY(
            Path::join(resolvedProject.linkResponsePath, {resolvedProject.intermediateDirectory.view(), "link.rsp"}));
        SC_TRY(Path::join(resolvedProject.compileCommandsPath,
                          {resolvedProject.intermediateDirectory.view(), "compile_commands.json"}));
        SC_TRY(Path::join(
            resolvedProject.workspaceCompileCommandsPath,
            {parameters.directories.intermediatesDirectory.view(), workspace.name.view(), "compile_commands.json"}));

        Vector<WriterInternal::RenderItem> renderItems;
        SC_TRY(WriterInternal::renderProject(project.rootDirectory.view(), project, filePathsResolver, renderItems));
        for (const WriterInternal::RenderItem& renderItem : renderItems)
        {
            if (renderItem.type != WriterInternal::RenderItem::CppFile and
                renderItem.type != WriterInternal::RenderItem::CFile and
                renderItem.type != WriterInternal::RenderItem::ObjCFile and
                renderItem.type != WriterInternal::RenderItem::ObjCppFile)
            {
                continue;
            }

            ResolvedSource source;
            source.type = renderItem.type;
            SC_TRY(source.displayPath.assign(renderItem.referencePath.view()));

            const CompileFlags* opinions[] = {
                renderItem.compileFlags != nullptr ? renderItem.compileFlags : &project.files.compile,
                &configuration.compile,
                &project.files.compile,
            };
            SC_TRY(CompileFlags::merge(opinions, source.compileFlags));

            SC_TRY(Path::join(source.sourcePath, {project.rootDirectory.view(), renderItem.referencePath.view()}));

            String objectRelative = StringEncoding::Utf8;
            SC_TRY(objectRelative.assign(renderItem.referencePath.view()));
            SC_TRY(StringBuilder::createForAppendingTo(objectRelative).append(".o"));
            SC_TRY(
                Path::join(source.objectPath, {resolvedProject.intermediateDirectory.view(), objectRelative.view()}));
            SC_TRY(StringBuilder::format(source.dependencyPath, "{}.d", source.objectPath.view()));
            SC_TRY(StringBuilder::format(source.commandPath, "{}.command", source.objectPath.view()));
            SC_TRY(StringBuilder::format(source.responsePath, "{}.rsp", source.objectPath.view()));
            SC_TRY(resolvedProject.sources.push_back(move(source)));
        }
        return Result(true);
    }

    static Result buildCompileCommand(const ResolvedProject& resolvedProject, const ResolvedSource& source,
                                      CommandLine& commandLine)
    {
        const bool usesCppDriver =
            source.type == WriterInternal::RenderItem::CppFile or source.type == WriterInternal::RenderItem::ObjCppFile;
        const StringView executable =
            usesCppDriver ? resolvedProject.adapter.executableCpp.view() : resolvedProject.adapter.executableC.view();
        const StringView subcommand =
            usesCppDriver ? resolvedProject.adapter.subcommandCpp : resolvedProject.adapter.subcommandC;

        SC_TRY(commandLine.append(executable));
        if (not subcommand.isEmpty())
        {
            SC_TRY(commandLine.append(subcommand));
        }
        SC_TRY(appendTargeting(commandLine, resolvedProject, true));
        SC_TRY(appendWarningFlags(commandLine, source.compileFlags, usesCppDriver));
        SC_TRY(appendCompileFlags(commandLine, resolvedProject, source, usesCppDriver));
        SC_TRY(commandLine.append("-c"));
        SC_TRY(commandLine.append(source.sourcePath.view()));
        SC_TRY(commandLine.append("-o"));
        SC_TRY(commandLine.append(source.objectPath.view()));
        if (resolvedProject.parameters->execution.useCompilerDependencies)
        {
            SC_TRY(commandLine.append("-MMD"));
            SC_TRY(commandLine.append("-MF"));
            SC_TRY(commandLine.append(source.dependencyPath.view()));
            SC_TRY(commandLine.append("-MT"));
            SC_TRY(commandLine.append(source.objectPath.view()));
        }
        return Result(true);
    }

    static Result buildLinkCommand(const ResolvedProject& resolvedProject, CommandLine& commandLine)
    {
        SC_TRY(commandLine.append(resolvedProject.adapter.executableLink.view()));
        if (not resolvedProject.adapter.subcommandLink.isEmpty())
        {
            SC_TRY(commandLine.append(resolvedProject.adapter.subcommandLink));
        }
        SC_TRY(appendTargeting(commandLine, resolvedProject, false));
        if (resolvedProject.compileFlags.enableCoverage)
        {
            SC_TRY(commandLine.append("-fprofile-instr-generate"));
            SC_TRY(commandLine.append("-fcoverage-mapping"));
        }
        if (resolvedProject.compileFlags.enableASAN or resolvedProject.linkFlags.enableASAN)
        {
            SC_TRY(commandLine.append("-fsanitize=address,undefined"));
        }
        if (not resolvedProject.compileFlags.enableStdCpp and resolvedProject.adapter.isClangLike() and
            resolvedProject.parameters->platform != Platform::Linux)
        {
            SC_TRY(commandLine.append("-nostdlib++"));
        }
        for (const ResolvedSource& source : resolvedProject.sources)
        {
            SC_TRY(commandLine.append(source.objectPath.view()));
        }
        SC_TRY(appendLinkFlags(commandLine, resolvedProject));
        SC_TRY(commandLine.append("-o"));
        SC_TRY(commandLine.append(resolvedProject.executablePath.view()));
        return Result(true);
    }

    static Result buildArchiveCommand(const ResolvedProject& resolvedProject, CommandLine& commandLine)
    {
        SC_TRY(commandLine.append(resolvedProject.adapter.executableArchive.view()));
        if (not resolvedProject.adapter.subcommandArchive.isEmpty())
        {
            SC_TRY(commandLine.append(resolvedProject.adapter.subcommandArchive));
        }
        SC_TRY(commandLine.append("rcs"));
        SC_TRY(commandLine.append(resolvedProject.executablePath.view()));
        for (const ResolvedSource& source : resolvedProject.sources)
        {
            SC_TRY(commandLine.append(source.objectPath.view()));
        }
        return Result(true);
    }

    static Result buildFinalCommand(const ResolvedProject& resolvedProject, CommandLine& commandLine)
    {
        switch (resolvedProject.project->targetType)
        {
        case TargetType::ConsoleExecutable:
        case TargetType::GUIApplication: return buildLinkCommand(resolvedProject, commandLine);
        case TargetType::StaticLibrary: return buildArchiveCommand(resolvedProject, commandLine);
        }
        Assert::unreachable();
    }

    static Result appendCompileFlags(CommandLine& commandLine, const ResolvedProject& resolvedProject,
                                     const ResolvedSource& source, bool usesCppDriver)
    {
        const CompileFlags& flags = source.compileFlags;
        if (usesCppDriver)
        {
            String standard = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(standard, "-std={}", CppStandard::toString(flags.cppStandard)));
            SC_TRY(commandLine.append(standard.view()));
            if (not flags.enableRTTI)
            {
                SC_TRY(commandLine.append("-fno-rtti"));
            }
            if (not flags.enableExceptions)
            {
                SC_TRY(commandLine.append("-fno-exceptions"));
            }
            SC_TRY(commandLine.append("-fvisibility-inlines-hidden"));
            if (not flags.enableStdCpp and resolvedProject.adapter.isClangLike())
            {
                SC_TRY(commandLine.append("-nostdinc++"));
            }
        }

        SC_TRY(commandLine.append("-fvisibility=hidden"));
        SC_TRY(commandLine.append("-fstrict-aliasing"));
        switch (flags.optimizationLevel)
        {
        case Optimization::Debug:
            SC_TRY(commandLine.append("-D_DEBUG=1"));
            SC_TRY(commandLine.append("-g"));
            SC_TRY(commandLine.append("-ggdb"));
            SC_TRY(commandLine.append("-O0"));
            break;
        case Optimization::Release:
            SC_TRY(commandLine.append("-DNDEBUG=1"));
            SC_TRY(commandLine.append("-O3"));
            break;
        }

        if (flags.enableASAN)
        {
            SC_TRY(commandLine.append("-fsanitize=address,undefined"));
            SC_TRY(commandLine.append("-fno-sanitize=enum,return,float-divide-by-zero,function,vptr"));
        }
        if (flags.enableCoverage)
        {
            SC_TRY(commandLine.append("-fprofile-instr-generate"));
            SC_TRY(commandLine.append("-fcoverage-mapping"));
        }

        for (const String& define : flags.defines)
        {
            String expanded = StringEncoding::Utf8;
            SC_TRY(expandVariables(define.view(), resolvedProject.variables, expanded));
            String option = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(option, "-D{}", expanded.view()));
            SC_TRY(commandLine.append(option.view()));
        }
        for (const String& includePath : flags.includePaths)
        {
            String absolutePath = StringEncoding::Utf8;
            SC_TRY(resolveBuildPath(resolvedProject.project->rootDirectory.view(), includePath.view(),
                                    resolvedProject.variables, absolutePath));
            String option = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(option, "-I{}", absolutePath.view()));
            SC_TRY(commandLine.append(option.view()));
        }
        const Vector<String>& extraCompilerFlags = resolvedProject.parameters->toolchain.extraCompilerFlags;
        for (const String& extraCompilerFlag : extraCompilerFlags)
        {
            SC_TRY(commandLine.append(extraCompilerFlag.view()));
        }
        return Result(true);
    }

    static Result appendWarningFlags(CommandLine& commandLine, const CompileFlags& flags, bool usesCppDriver)
    {
        if (usesCppDriver)
        {
            SC_TRY(commandLine.append("-Wnon-virtual-dtor"));
            SC_TRY(commandLine.append("-Woverloaded-virtual"));
        }
        SC_TRY(commandLine.append("-Werror"));
        SC_TRY(commandLine.append("-Werror=return-type"));
        SC_TRY(commandLine.append("-Wunreachable-code"));
        SC_TRY(commandLine.append("-Wmissing-braces"));
        SC_TRY(commandLine.append("-Wparentheses"));
        SC_TRY(commandLine.append("-Wswitch"));
        SC_TRY(commandLine.append("-Wunused-function"));
        SC_TRY(commandLine.append("-Wunused-label"));
        SC_TRY(commandLine.append("-Wunused-parameter"));
        SC_TRY(commandLine.append("-Wunused-variable"));
        SC_TRY(commandLine.append("-Wunused-value"));
        SC_TRY(commandLine.append("-Wempty-body"));
        SC_TRY(commandLine.append("-Wuninitialized"));
        SC_TRY(commandLine.append("-Wunknown-pragmas"));
        SC_TRY(commandLine.append("-Wenum-conversion"));
        SC_TRY(commandLine.append("-Werror=float-conversion"));
        SC_TRY(commandLine.append("-Werror=implicit-fallthrough"));
        for (const Warning& warning : flags.warnings)
        {
            if (warning.state == Warning::Disabled and warning.type != Warning::MSVCWarning)
            {
                String option = StringEncoding::Utf8;
                SC_TRY(StringBuilder::format(option, "-Wno-{}", warning.name));
                SC_TRY(commandLine.append(option.view()));
            }
        }
        return Result(true);
    }

    static Result appendLinkFlags(CommandLine& commandLine, const ResolvedProject& resolvedProject)
    {
        for (const String& libraryPath : resolvedProject.linkFlags.libraryPaths)
        {
            String absoluteLibraryPath = StringEncoding::Utf8;
            SC_TRY(resolveBuildPath(resolvedProject.project->rootDirectory.view(), libraryPath.view(),
                                    resolvedProject.variables, absoluteLibraryPath));
            String option = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(option, "-L{}", absoluteLibraryPath.view()));
            SC_TRY(commandLine.append(option.view()));
        }

        if (resolvedProject.parameters->platform == Platform::Apple)
        {
            for (const String& framework : resolvedProject.linkFlags.frameworks)
            {
                SC_TRY(commandLine.append("-framework"));
                SC_TRY(commandLine.append(framework.view()));
            }
            for (const String& framework : resolvedProject.linkFlags.frameworksMacOS)
            {
                SC_TRY(commandLine.append("-framework"));
                SC_TRY(commandLine.append(framework.view()));
            }
        }

        for (const String& library : resolvedProject.linkFlags.libraries)
        {
            const StringView libraryView = library.view();
            if (libraryView.containsCodePoint('/') or libraryView.endsWith(".a") or libraryView.endsWith(".so") or
                libraryView.endsWith(".dylib"))
            {
                String absoluteLibrary = StringEncoding::Utf8;
                SC_TRY(resolveBuildPath(resolvedProject.project->rootDirectory.view(), libraryView,
                                        resolvedProject.variables, absoluteLibrary));
                SC_TRY(commandLine.append(absoluteLibrary.view()));
            }
            else
            {
                String option = StringEncoding::Utf8;
                SC_TRY(StringBuilder::format(option, "-l{}", libraryView));
                SC_TRY(commandLine.append(option.view()));
            }
        }

        const Vector<String>& extraLinkerFlags = resolvedProject.parameters->toolchain.extraLinkerFlags;
        for (const String& extraLinkerFlag : extraLinkerFlags)
        {
            SC_TRY(commandLine.append(extraLinkerFlag.view()));
        }
        return Result(true);
    }

    static Result appendTargeting(CommandLine& commandLine, const ResolvedProject& resolvedProject, bool forCompiler)
    {
        StringView targetTriple = resolvedProject.parameters->toolchain.targetTriple.view();
        if (targetTriple.isEmpty() and resolvedProject.parameters->platform == Platform::Apple and
            resolvedProject.parameters->architecture != Architecture::Any and resolvedProject.adapter.isClangLike())
        {
            switch (resolvedProject.parameters->architecture)
            {
            case Architecture::Arm64: targetTriple = "arm64-apple-macos11"; break;
            case Architecture::Intel64: targetTriple = "x86_64-apple-macos11"; break;
            case Architecture::Intel32:
            case Architecture::Any:
            case Architecture::Wasm: break;
            }
        }
        if (not targetTriple.isEmpty())
        {
            SC_TRY(commandLine.append("-target"));
            SC_TRY(commandLine.append(targetTriple));
        }

        if (not resolvedProject.parameters->toolchain.sysroot.isEmpty())
        {
            if (resolvedProject.parameters->platform == Platform::Apple)
            {
                SC_TRY(commandLine.append("-isysroot"));
                SC_TRY(commandLine.append(resolvedProject.parameters->toolchain.sysroot.view()));
            }
            else
            {
                SC_TRY(commandLine.append("--sysroot"));
                SC_TRY(commandLine.append(resolvedProject.parameters->toolchain.sysroot.view()));
            }
        }
        SC_COMPILER_UNUSED(forCompiler);
        return Result(true);
    }

    static bool shouldRebuildObject(FileSystem& fs, const ResolvedProject& resolvedProject,
                                    const ResolvedSource& source, StringView commandLine)
    {
        if (not fs.existsAndIsFile(source.objectPath.view()))
            return true;
        if (not fs.existsAndIsFile(source.commandPath.view()))
            return true;
        if (resolvedProject.parameters->execution.useCompilerDependencies and
            not fs.existsAndIsFile(source.dependencyPath.view()))
        {
            return true;
        }

        String existingCommand = StringEncoding::Utf8;
        if (not fs.read(source.commandPath.view(), existingCommand))
            return true;
        if (existingCommand.view() != commandLine)
            return true;

        FileSystem::FileStat objectStat;
        if (not fs.stat(source.objectPath.view(), objectStat))
            return true;

        Vector<String> dependencies;
        if (resolvedProject.parameters->execution.useCompilerDependencies)
        {
            if (not parseDependencyFile(fs, source.dependencyPath.view(), resolvedProject.project->rootDirectory.view(),
                                        dependencies))
            {
                return true;
            }
        }
        else
        {
            String path = StringEncoding::Utf8;
            if (path.assign(source.sourcePath.view()))
            {
                (void)dependencies.push_back(move(path));
            }
        }

        for (const String& dependency : dependencies)
        {
            FileSystem::FileStat dependencyStat;
            if (not fs.stat(dependency.view(), dependencyStat))
                return true;
            if (dependencyStat.modifiedTime.milliseconds > objectStat.modifiedTime.milliseconds)
                return true;
        }
        return false;
    }

    static bool shouldRebuildTargetArtifact(FileSystem& fs, const ResolvedProject& resolvedProject,
                                            StringView commandLine, bool anyObjectBuilt)
    {
        if (anyObjectBuilt)
            return true;
        if (not fs.existsAndIsFile(resolvedProject.executablePath.view()))
            return true;
        if (not fs.existsAndIsFile(resolvedProject.linkCommandPath.view()))
            return true;

        String existingCommand = StringEncoding::Utf8;
        if (not fs.read(resolvedProject.linkCommandPath.view(), existingCommand))
            return true;
        if (existingCommand.view() != commandLine)
            return true;

        FileSystem::FileStat executableStat;
        if (not fs.stat(resolvedProject.executablePath.view(), executableStat))
            return true;
        for (const ResolvedSource& source : resolvedProject.sources)
        {
            FileSystem::FileStat objectStat;
            if (not fs.stat(source.objectPath.view(), objectStat))
                return true;
            if (objectStat.modifiedTime.milliseconds > executableStat.modifiedTime.milliseconds)
                return true;
        }
        return false;
    }

    static Result parseDependencyFile(FileSystem& fs, StringView dependencyPath, StringView projectRoot,
                                      Vector<String>& dependencies)
    {
        String contents = StringEncoding::Utf8;
        SC_TRY(fs.read(dependencyPath, contents));

        StringView dependencyData;
        SC_TRY_MSG(StringView(contents.view()).splitAfter(":", dependencyData), "Malformed dependency file");

        const char*  dependencyBytes = dependencyData.bytesWithoutTerminator();
        const size_t dependencySize  = dependencyData.sizeInBytes();

        String current = StringEncoding::Utf8;
        for (size_t idx = 0; idx < dependencySize; ++idx)
        {
            const char c = dependencyBytes[idx];
            if (c == '\\')
            {
                if (idx + 1 >= dependencySize)
                    break;
                const char next = dependencyBytes[idx + 1];
                if (next == '\n' or next == '\r')
                {
                    idx++;
                    continue;
                }
                const char nextCharacter[] = {next, 0};
                SC_TRY(StringBuilder::createForAppendingTo(current).append(
                    StringView::fromNullTerminated(nextCharacter, StringEncoding::Utf8)));
                idx++;
                continue;
            }
            if (c == ' ' or c == '\n' or c == '\r' or c == '\t')
            {
                if (not current.isEmpty())
                {
                    SC_TRY(normalizeDependencyPath(projectRoot, current));
                    SC_TRY(dependencies.push_back(move(current)));
                    current = "";
                }
                continue;
            }
            const char character[] = {c, 0};
            SC_TRY(StringBuilder::createForAppendingTo(current).append(
                StringView::fromNullTerminated(character, StringEncoding::Utf8)));
        }
        if (not current.isEmpty())
        {
            SC_TRY(normalizeDependencyPath(projectRoot, current));
            SC_TRY(dependencies.push_back(move(current)));
        }
        return Result(true);
    }

    static Result normalizeDependencyPath(StringView projectRoot, String& dependencyPath)
    {
        if (Path::isAbsolute(dependencyPath.view(), Path::AsNative))
            return Result(true);
        String normalized = StringEncoding::Utf8;
        SC_TRY(Path::join(normalized, {projectRoot, dependencyPath.view()}));
        dependencyPath = move(normalized);
        return Result(true);
    }

    static Result writeCompileCommands(FileSystem& fs, const ResolvedProject& resolvedProject,
                                       Vector<String>& workspaceEntries)
    {
        Vector<String> projectEntries;
        for (const ResolvedSource& source : resolvedProject.sources)
        {
            CommandLine commandLine;
            String      commandString = StringEncoding::Utf8;
            SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
            SC_TRY(formatCommandLine(commandLine, commandString));

            String escapedDirectory = StringEncoding::Utf8;
            String escapedCommand   = StringEncoding::Utf8;
            String escapedFile      = StringEncoding::Utf8;
            String escapedOutput    = StringEncoding::Utf8;
            SC_TRY(appendJsonEscaped(escapedDirectory, resolvedProject.project->rootDirectory.view()));
            SC_TRY(appendJsonEscaped(escapedCommand, commandString.view()));
            SC_TRY(appendJsonEscaped(escapedFile, source.sourcePath.view()));
            SC_TRY(appendJsonEscaped(escapedOutput, source.objectPath.view()));

            String entry = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(
                entry, "{{\"directory\":\"{}\",\"command\":\"{}\",\"file\":\"{}\",\"output\":\"{}\"}}",
                escapedDirectory.view(), escapedCommand.view(), escapedFile.view(), escapedOutput.view()));
            SC_TRY(projectEntries.push_back(entry));
            SC_TRY(workspaceEntries.push_back(move(entry)));
        }

        SC_TRY(writeCompileCommandsArray(fs, resolvedProject.compileCommandsPath.view(), projectEntries.toSpanConst()));
        return Result(true);
    }

    static Result writeCompileCommandsArray(FileSystem& fs, StringView path, Span<const String> entries)
    {
        String json = StringEncoding::Utf8;
        SC_TRY(StringBuilder::createForAppendingTo(json).append("[\n"));
        for (size_t idx = 0; idx < entries.sizeInElements(); ++idx)
        {
            SC_TRY(StringBuilder::createForAppendingTo(json).append(entries[idx].view()));
            if (idx + 1 < entries.sizeInElements())
            {
                SC_TRY(StringBuilder::createForAppendingTo(json).append(",\n"));
            }
        }
        SC_TRY(StringBuilder::createForAppendingTo(json).append("\n]\n"));
        SC_TRY(makeParentDirectory(fs, path));
        SC_TRY(fs.writeString(path, json.view()));
        return Result(true);
    }

    static Result fillVariables(const Parameters& parameters, const Project& project,
                                const Configuration& configuration, StringView compilerName, Variables& variables)
    {
        SC_TRY(variables.projectName.assign(project.name.view()));
        SC_TRY(variables.projectRoot.assign(project.rootDirectory.view()));
        SC_TRY(variables.targetOS.assign(platformName(parameters.platform)));
        SC_TRY(variables.targetArchitectures.assign(architectureName(parameters.architecture)));
        SC_TRY(variables.buildSystem.assign(Generator::toString(parameters.generator)));
        SC_TRY(variables.compiler.assign(compilerName));
        SC_TRY(variables.configuration.assign(configuration.name.view()));
        return Result(true);
    }

    static Result resolveCompilerAdapter(const Toolchain& toolchain, Platform::Type platform,
                                         Architecture::Type architecture, CompilerAdapter& adapter)
    {
        SC_COMPILER_UNUSED(platform);
        SC_COMPILER_UNUSED(architecture);

        adapter.family = toolchain.family;
        if (adapter.family == Toolchain::HostDefault)
        {
            if (HostPlatform == SC::Platform::Apple)
            {
                adapter.family = Toolchain::Clang;
            }
            else if (probeExecutable("clang++"))
            {
                adapter.family = Toolchain::Clang;
            }
            else
            {
                adapter.family = Toolchain::GCC;
            }
        }

        switch (adapter.family)
        {
        case Toolchain::Clang:
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), "clang", adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), "clang++", adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), "ar", adapter.executableArchive));
            SC_TRY(adapter.displayName.assign("clang"));
            break;
        case Toolchain::GCC:
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), "gcc", adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), "g++", adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), "ar", adapter.executableArchive));
            SC_TRY(adapter.displayName.assign("gcc"));
            break;
        case Toolchain::ZigCC:
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), "zig", adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), adapter.executableC.view(), adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), adapter.executableC.view(), adapter.executableArchive));
            adapter.subcommandC       = "cc";
            adapter.subcommandCpp     = "c++";
            adapter.subcommandLink    = "c++";
            adapter.subcommandArchive = "ar";
            SC_TRY(adapter.displayName.assign("zigcc"));
            break;
        case Toolchain::CustomDriver:
            SC_TRY_MSG(not toolchain.compilerC.isEmpty(), "CustomDriver requires compilerC");
            SC_TRY_MSG(not toolchain.compilerCpp.isEmpty(), "CustomDriver requires compilerCpp");
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), StringView(), adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), StringView(), adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), "ar", adapter.executableArchive));
            SC_TRY(adapter.displayName.assign(Path::basename(adapter.executableCpp.view(), Path::AsNative)));
            break;
        case Toolchain::MSVC:
        case Toolchain::ClangCL: return Result::Error("MSVC-style Native backend is planned but not implemented yet");
        case Toolchain::HostDefault: return Result::Error("Unexpected HostDefault toolchain");
        }
        return Result(true);
    }

    static Result expandConfiguredPath(StringView baseDirectory, StringView configuredPath, const Variables& variables,
                                       String& output)
    {
        String expanded = StringEncoding::Utf8;
        SC_TRY(expandVariables(configuredPath, variables, expanded));
        if (Path::isAbsolute(expanded.view(), Path::AsNative))
        {
            SC_TRY(output.assign(expanded.view()));
            return Result(true);
        }
        SC_TRY(Path::join(output, {baseDirectory, expanded.view()}));
        return Result(true);
    }

    static Result resolveBuildPath(StringView projectRoot, StringView configuredPath, const Variables& variables,
                                   String& output)
    {
        String expanded = StringEncoding::Utf8;
        SC_TRY(expandVariables(configuredPath, variables, expanded));
        if (Path::isAbsolute(expanded.view(), Path::AsNative))
        {
            SC_TRY(output.assign(expanded.view()));
            return Result(true);
        }
        SC_TRY(Path::join(output, {projectRoot, expanded.view()}));
        return Result(true);
    }

    static Result computeArtifactName(const Project& project, String& output)
    {
        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable:
        case TargetType::GUIApplication: SC_TRY(output.assign(project.targetName.view())); return Result(true);
        case TargetType::StaticLibrary:
            if (StringView(project.targetName.view()).startsWith("lib"))
            {
                SC_TRY(StringBuilder::format(output, "{}.a", project.targetName.view()));
                return Result(true);
            }
            SC_TRY(StringBuilder::format(output, "lib{}.a", project.targetName.view()));
            return Result(true);
        }
        Assert::unreachable();
    }

    static constexpr StringView getFinalStepName(const Project& project)
    {
        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable:
        case TargetType::GUIApplication: return "LINK"_a8;
        case TargetType::StaticLibrary: return "AR"_a8;
        }
        Assert::unreachable();
    }

    static StringView finalResponsePath(const ResolvedProject& resolvedProject)
    {
        if (resolvedProject.project->targetType == TargetType::StaticLibrary)
            return StringView();
        return resolvedProject.linkResponsePath.view();
    }

    static Result expandVariables(StringView source, const Variables& variables, String& output)
    {
        auto                             builder         = StringBuilder::create(output);
        const ProjectWriter::ReplacePair substitutions[] = {
            {"$(PROJECT_NAME)", variables.projectName.view()},
            {"$(PROJECT_ROOT)", variables.projectRoot.view()},
            {"$(TARGET_OS)", variables.targetOS.view()},
            {"$(TARGET_ARCHITECTURES)", variables.targetArchitectures.view()},
            {"$(BUILD_SYSTEM)", variables.buildSystem.view()},
            {"$(COMPILER)", variables.compiler.view()},
            {"$(CONFIGURATION)", variables.configuration.view()},
        };
        SC_TRY(ProjectWriter::appendReplaceMultiple(builder, source, substitutions));
        builder.finalize();
        return Result(true);
    }

    static Result formatCommandLine(const CommandLine& commandLine, String& output)
    {
        for (size_t idx = 0; idx < commandLine.arguments.size(); ++idx)
        {
            if (idx > 0)
            {
                SC_TRY(StringBuilder::createForAppendingTo(output).append(" "));
            }
            SC_TRY(appendQuoted(output, commandLine.arguments[idx].view()));
        }
        return Result(true);
    }

    static Result appendQuoted(String& output, StringView value)
    {
        const bool needsQuotes =
            value.containsCodePoint(' ') or value.containsCodePoint('\t') or value.containsCodePoint('"');
        if (not needsQuotes)
        {
            SC_TRY(StringBuilder::createForAppendingTo(output).append(value));
            return Result(true);
        }
        SC_TRY(StringBuilder::createForAppendingTo(output).append("\""));
        const char* bytes = value.bytesWithoutTerminator();
        for (size_t idx = 0; idx < value.sizeInBytes(); ++idx)
        {
            const char c = bytes[idx];
            if (c == '"' or c == '\\')
            {
                SC_TRY(StringBuilder::createForAppendingTo(output).append("\\"));
            }
            const char character[] = {c, 0};
            SC_TRY(StringBuilder::createForAppendingTo(output).append(
                StringView::fromNullTerminated(character, StringEncoding::Utf8)));
        }
        SC_TRY(StringBuilder::createForAppendingTo(output).append("\""));
        return Result(true);
    }

    static Result appendJsonEscaped(String& output, StringView value)
    {
        const char* bytes = value.bytesWithoutTerminator();
        for (size_t idx = 0; idx < value.sizeInBytes(); ++idx)
        {
            const char c = bytes[idx];
            if (c == '\\')
            {
                SC_TRY(StringBuilder::createForAppendingTo(output).append("\\\\"));
            }
            else if (c == '"')
            {
                SC_TRY(StringBuilder::createForAppendingTo(output).append("\\\""));
            }
            else if (c == '\n')
            {
                SC_TRY(StringBuilder::createForAppendingTo(output).append("\\n"));
            }
            else
            {
                const char character[] = {c, 0};
                SC_TRY(StringBuilder::createForAppendingTo(output).append(
                    StringView::fromNullTerminated(character, StringEncoding::Utf8)));
            }
        }
        return Result(true);
    }

    static Result makeParentDirectory(FileSystem& fs, StringView filePath)
    {
        StringView directory = Path::dirname(filePath, Path::AsNative);
        if (directory.isEmpty())
            return Result(true);
        return fs.makeDirectoryRecursive(directory);
    }

    static Result resolveExecutable(StringView configured, StringView fallback, String& output)
    {
        if (not configured.isEmpty())
        {
            SC_TRY(output.assign(configured));
            return Result(true);
        }
        SC_TRY_MSG(not fallback.isEmpty(), "Missing compiler executable");
        SC_TRY(output.assign(fallback));
        return Result(true);
    }

    static bool probeExecutable(StringView executable)
    {
        Process process;
        String  output = StringEncoding::Utf8;
        return process.exec({executable, "--version"}, output) and process.getExitStatus() == 0;
    }

    static Result findWorkspace(const Definition& definition, StringView workspaceName, const Workspace*& workspace)
    {
        size_t index = 0;
        SC_TRY_MSG(
            definition.workspaces.find([&](const Workspace& item) { return item.name == workspaceName; }, &index),
            "Workspace not found");
        workspace = &definition.workspaces[index];
        return Result(true);
    }

    static Result findProjectConfiguration(const Workspace& workspace, StringView projectName,
                                           StringView configurationName, const Project*& project,
                                           const Configuration*& configuration)
    {
        size_t index = 0;
        SC_TRY_MSG(workspace.projects.find([&](const Project& item) { return item.name == projectName; }, &index),
                   "Project not found");
        project       = &workspace.projects[index];
        configuration = project->getConfiguration(configurationName);
        SC_TRY_MSG(configuration != nullptr, "Configuration not found");
        return Result(true);
    }

    static constexpr StringView getCompileStepName(WriterInternal::RenderItem::Type type)
    {
        switch (type)
        {
        case WriterInternal::RenderItem::CFile: return "CC";
        case WriterInternal::RenderItem::ObjCFile: return "OBJC";
        case WriterInternal::RenderItem::ObjCppFile: return "OBJCXX";
        case WriterInternal::RenderItem::CppFile: return "CXX";
        default: return "COMPILE";
        }
    }

    static constexpr StringView platformName(Platform::Type platform)
    {
        switch (platform)
        {
        case Platform::Apple: return "macOS";
        case Platform::Linux: return "linux";
        case Platform::Windows: return "windows";
        case Platform::Wasm: return "wasm";
        case Platform::Unknown: return "unknown";
        }
        Assert::unreachable();
    }

    static constexpr StringView architectureName(Architecture::Type architecture)
    {
        switch (architecture)
        {
        case Architecture::Intel32: return "x86";
        case Architecture::Intel64: return "x86_64";
        case Architecture::Arm64: return "arm64";
        case Architecture::Wasm: return "wasm32";
        case Architecture::Any:
            switch (HostInstructionSet)
            {
            case InstructionSet::Intel32: return "x86";
            case InstructionSet::Intel64: return "x86_64";
            case InstructionSet::ARM64: return "arm64";
            }
            Assert::unreachable();
        }
        Assert::unreachable();
    }
};
