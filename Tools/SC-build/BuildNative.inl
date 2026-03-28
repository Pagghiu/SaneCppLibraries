// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Libraries/Async/Async.h"
#include "../../Libraries/Async/Internal/IntrusiveDoubleLinkedList.inl"
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
        [[nodiscard]] bool isMSVCStyle() const { return family == Toolchain::MSVC or family == Toolchain::ClangCL; }
        [[nodiscard]] bool isClangCL() const { return family == Toolchain::ClangCL; }
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

        Vector<const Project*> workspaceDependencies;
        Vector<String>         workspaceDependencyArtifacts;
        Vector<ResolvedSource> sources;
    };

    struct ProjectProgress
    {
        size_t compileStartStep = 1;
        size_t totalSteps       = 1;
        size_t finalStep        = 1;
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

    enum RebuildDecision
    {
        UpToDate,
        MissingOutput,
        MissingCommandFile,
        MissingDependencyFile,
        CommandChanged,
        OutputStatUnavailable,
        DependencyScanFailed,
        InputChanged,
        PreviousBuildStepRan,
    };

    static constexpr size_t MaxParallelCompileJobs = 32;

    struct ParallelCompileLimiter
    {
        static constexpr size_t ReadBufferSize = 4096;

        struct Slot : AsyncProcessExit
        {
            const ResolvedProject* project        = nullptr;
            ResolvedSource*        source         = nullptr;
            bool*                  anyObjectBuilt = nullptr;
            String                 commandString;
            String                 stdOutPath = StringEncoding::Utf8;
            String                 stdErrPath = StringEncoding::Utf8;
            PipeDescriptor         stdOutPipe;
            PipeDescriptor         stdErrPipe;
            AsyncFileRead          asyncStdOut;
            AsyncFileRead          asyncStdErr;
            String                 stdOut          = StringEncoding::Utf8;
            String                 stdErr          = StringEncoding::Utf8;
            int                    exitStatus      = -1;
            bool                   processFinished = false;
            bool                   stdOutFinished  = false;
            bool                   stdErrFinished  = false;
            char                   stdOutBuffer[ReadBufferSize];
            char                   stdErrBuffer[ReadBufferSize];
        };

        AsyncEventLoop                  eventLoop;
        Result                          processResult = Result(true);
        FileSystem*                     fileSystem    = nullptr;
        IntrusiveDoubleLinkedList<Slot> availableSlots;
        Slot                            slots[MaxParallelCompileJobs];

        Result create(FileSystem& fs, size_t maxProcesses)
        {
            fileSystem    = &fs;
            processResult = Result(true);
            if (maxProcesses == 0)
            {
                maxProcesses = 1;
            }
            if (maxProcesses > MaxParallelCompileJobs)
            {
                maxProcesses = MaxParallelCompileJobs;
            }

            for (size_t idx = 0; idx < maxProcesses; ++idx)
            {
                availableSlots.queueBack(slots[idx]);
            }
            return eventLoop.create();
        }

        Result close()
        {
            SC_TRY(eventLoop.run());
            SC_TRY(eventLoop.close());
            return processResult;
        }

        static Result appendChunk(String& output, Span<char> data)
        {
            SC_TRY(StringBuilder::createForAppendingTo(output).append(
                StringView(StringSpan(data, false, StringEncoding::Utf8))));
            return Result(true);
        }

        static Result buildCapturedOutputPath(const ResolvedSource& source, StringView suffix, String& path)
        {
            SC_TRY(path.assign(source.objectPath.view()));
            SC_TRY(StringBuilder::createForAppendingTo(path).append(suffix));
            return Result(true);
        }

        static Result readFileIntoString(StringSpan path, String& output)
        {
            FileDescriptor file;
            SC_TRY(file.open(path, FileOpen::Read));
            return file.readUntilEOF(output);
        }

        Result finishSlotIfDone(Slot& slot)
        {
            if (not(slot.processFinished and slot.stdOutFinished and slot.stdErrFinished))
            {
                return Result(true);
            }

            Result slotResult = Result(true);
            if (slot.project->adapter.isMSVCStyle())
            {
                slotResult = readFileIntoString(slot.stdOutPath.view(), slot.stdOut);
                if (slotResult)
                {
                    slotResult = readFileIntoString(slot.stdErrPath.view(), slot.stdErr);
                }
            }
            if (slot.project->parameters->execution.useCompilerDependencies and slot.project->adapter.isMSVCStyle())
            {
                slotResult =
                    writeWindowsDependencyFile(*fileSystem, *slot.project, *slot.source, slot.stdOut, slot.stdErr);
            }

            if (slotResult)
            {
                if (slot.exitStatus != 0)
                {
                    if (not slot.stdOut.isEmpty())
                    {
                        globalConsole->print(slot.stdOut.view());
                    }
                    if (not slot.stdErr.isEmpty())
                    {
                        globalConsole->printError(slot.stdErr.view());
                        globalConsole->flushStdErr();
                    }
                    slotResult = Result::Error("Native backend command failed");
                }
                else
                {
                    if (slot.project->parameters->execution.verbose and not slot.stdOut.isEmpty())
                    {
                        globalConsole->print(slot.stdOut.view());
                    }
                    if (not slot.stdErr.isEmpty())
                    {
                        globalConsole->printError(slot.stdErr.view());
                        globalConsole->flushStdErr();
                    }
                }
            }

            if (slotResult)
            {
                slotResult = fileSystem->writeString(slot.source->commandPath.view(), slot.commandString.view());
            }
            if (slotResult)
            {
                *slot.anyObjectBuilt = true;
            }
            else if (processResult)
            {
                processResult = slotResult;
            }

            SC_TRY(slot.stdOutPipe.close());
            SC_TRY(slot.stdErrPipe.close());
            if (not slot.stdOutPath.isEmpty())
            {
                SC_TRY(fileSystem->removeFileIfExists(slot.stdOutPath.view()));
            }
            if (not slot.stdErrPath.isEmpty())
            {
                SC_TRY(fileSystem->removeFileIfExists(slot.stdErrPath.view()));
            }
            SC_TRY(slot.commandString.assign({}));
            SC_TRY(slot.stdOutPath.assign({}));
            SC_TRY(slot.stdErrPath.assign({}));
            SC_TRY(slot.stdOut.assign({}));
            SC_TRY(slot.stdErr.assign({}));
            slot.project         = nullptr;
            slot.source          = nullptr;
            slot.anyObjectBuilt  = nullptr;
            slot.exitStatus      = -1;
            slot.processFinished = false;
            slot.stdOutFinished  = false;
            slot.stdErrFinished  = false;
            availableSlots.queueBack(slot);
            return Result(true);
        }

        Result launch(size_t compileStep, size_t totalSteps, const ResolvedProject& project, ResolvedSource& source,
                      CommandLine& commandLine, String& commandString, bool& anyObjectBuilt)
        {
            while (availableSlots.isEmpty())
            {
                SC_TRY(eventLoop.runOnce());
            }
            if (not processResult)
            {
                return processResult;
            }

            Slot& slot          = *availableSlots.dequeueFront();
            slot.project        = &project;
            slot.source         = &source;
            slot.anyObjectBuilt = &anyObjectBuilt;
            SC_TRY(slot.commandString.assign(commandString.view()));
            SC_TRY(slot.stdOut.assign({}));
            SC_TRY(slot.stdErr.assign({}));
            slot.exitStatus      = -1;
            slot.processFinished = false;
            slot.stdOutFinished  = false;
            slot.stdErrFinished  = false;

            SC_TRY(makeParentDirectory(*fileSystem, source.objectPath.view()));
            globalConsole->print("[{}/{}] {} {}\n", compileStep, totalSteps, getCompileStepName(source.type),
                                 source.displayPath.view());
            globalConsole->flush();

            SC_TRY(maybeWriteResponseFile(*fileSystem, commandLine, source.responsePath.view(), project.adapter));

            StringSpan             views[128];
            Span<const StringSpan> args;
            SC_TRY(commandLine.toViews(views, args));

            Process process;
            if (project.adapter.isMSVCStyle())
            {
                SC_TRY(buildCapturedOutputPath(source, ".stdout.tmp", slot.stdOutPath));
                SC_TRY(buildCapturedOutputPath(source, ".stderr.tmp", slot.stdErrPath));
                SC_TRY(fileSystem->removeFileIfExists(slot.stdOutPath.view()));
                SC_TRY(fileSystem->removeFileIfExists(slot.stdErrPath.view()));

                FileOpen openForChild(FileOpen::Write);
                openForChild.inheritable = true;
                FileDescriptor stdOutFile;
                FileDescriptor stdErrFile;
                SC_TRY(stdOutFile.open(slot.stdOutPath.view(), openForChild));
                SC_TRY(stdErrFile.open(slot.stdErrPath.view(), openForChild));
                slot.stdOutFinished = true;
                slot.stdErrFinished = true;
                SC_TRY(process.launch(args, stdOutFile, Process::StdIn(), stdErrFile));
            }
            else
            {
                PipeOptions pipeOptions;
                pipeOptions.blocking         = false;
                pipeOptions.writeInheritable = true;
                SC_TRY(slot.stdOutPipe.createPipe(pipeOptions));
                SC_TRY(slot.stdErrPipe.createPipe(pipeOptions));
                SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(slot.stdOutPipe.readPipe));
                SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(slot.stdErrPipe.readPipe));
                SC_TRY(slot.stdOutPipe.readPipe.get(slot.asyncStdOut.handle, Result::Error("stdout handle")));
                SC_TRY(slot.stdErrPipe.readPipe.get(slot.asyncStdErr.handle, Result::Error("stderr handle")));
                slot.asyncStdOut.buffer   = slot.stdOutBuffer;
                slot.asyncStdErr.buffer   = slot.stdErrBuffer;
                slot.asyncStdOut.callback = [this, &slot](AsyncFileRead::Result& readResult)
                {
                    Span<char> data;
                    Result     callbackResult = readResult.get(data);
                    if (callbackResult and not readResult.completionData.endOfFile)
                    {
                        callbackResult = appendChunk(slot.stdOut, data);
                        if (callbackResult)
                        {
                            readResult.reactivateRequest(true);
                        }
                    }
                    else
                    {
                        slot.stdOutFinished = true;
                    }

                    if (not callbackResult and processResult)
                    {
                        processResult       = callbackResult;
                        slot.stdOutFinished = true;
                    }
                    (void)finishSlotIfDone(slot);
                };
                slot.asyncStdErr.callback = [this, &slot](AsyncFileRead::Result& readResult)
                {
                    Span<char> data;
                    Result     callbackResult = readResult.get(data);
                    if (callbackResult and not readResult.completionData.endOfFile)
                    {
                        callbackResult = appendChunk(slot.stdErr, data);
                        if (callbackResult)
                        {
                            readResult.reactivateRequest(true);
                        }
                    }
                    else
                    {
                        slot.stdErrFinished = true;
                    }

                    if (not callbackResult and processResult)
                    {
                        processResult       = callbackResult;
                        slot.stdErrFinished = true;
                    }
                    (void)finishSlotIfDone(slot);
                };
                SC_TRY(slot.asyncStdOut.start(eventLoop));
                SC_TRY(slot.asyncStdErr.start(eventLoop));
                SC_TRY(process.launch(args, slot.stdOutPipe, Process::StdIn(), slot.stdErrPipe));
            }
            SC_TRY(slot.start(eventLoop, process.handle));
            slot.callback = [this, &slot](AsyncProcessExit::Result& result)
            {
                int    exitStatus     = -1;
                Result callbackResult = result.get(exitStatus);
                if (callbackResult)
                {
                    slot.exitStatus = exitStatus;
                }
                else if (processResult)
                {
                    processResult = callbackResult;
                }
                slot.processFinished = true;
                (void)finishSlotIfDone(slot);
            };
            return Result(true);
        }
    };

    static size_t computeWorkspaceParallelJobs(const Parameters& parameters)
    {
        size_t maxParallelJobs = parameters.execution.maxParallelJobs;
        if (maxParallelJobs == 0)
        {
            maxParallelJobs = Process::getNumberOfProcessors();
        }
        if (maxParallelJobs == 0)
        {
            maxParallelJobs = 1;
        }
        if (maxParallelJobs > MaxParallelCompileJobs)
        {
            maxParallelJobs = MaxParallelCompileJobs;
        }
        return maxParallelJobs;
    }

    static Result findResolvedProjectIndex(const Vector<ResolvedProject>& resolvedProjects, const Project& project,
                                           size_t& outIndex)
    {
        SC_TRY_MSG(
            resolvedProjects.find([&](const ResolvedProject& item) { return item.project == &project; }, &outIndex),
            "Resolved dependency not found");
        return Result(true);
    }

    static Result computeResolvedProjectLevels(const Vector<ResolvedProject>& resolvedProjects, Vector<size_t>& levels)
    {
        for (const ResolvedProject& resolvedProject : resolvedProjects)
        {
            size_t level = 0;
            for (const Project* dependency : resolvedProject.workspaceDependencies)
            {
                size_t dependencyIndex = 0;
                SC_TRY(findResolvedProjectIndex(resolvedProjects, *dependency, dependencyIndex));
                if (levels[dependencyIndex] + 1 > level)
                {
                    level = levels[dependencyIndex] + 1;
                }
            }
            SC_TRY(levels.push_back(level));
        }
        return Result(true);
    }

    static Result computeResolvedProjectProgress(const Vector<ResolvedProject>& resolvedProjects,
                                                 const Vector<size_t>&          projectLevels,
                                                 Vector<ProjectProgress>&       progressByProject)
    {
        size_t maxLevel = 0;
        for (size_t projectLevel : projectLevels)
        {
            if (projectLevel > maxLevel)
            {
                maxLevel = projectLevel;
            }
        }

        size_t nextStep = 1;
        for (size_t idx = 0; idx < resolvedProjects.size(); ++idx)
        {
            SC_TRY(progressByProject.push_back({}));
        }

        for (size_t level = 0; level <= maxLevel; ++level)
        {
            for (size_t idx = 0; idx < resolvedProjects.size(); ++idx)
            {
                if (projectLevels[idx] == level)
                {
                    progressByProject[idx].compileStartStep = nextStep;
                    nextStep += resolvedProjects[idx].sources.size();
                }
            }
            for (size_t idx = 0; idx < resolvedProjects.size(); ++idx)
            {
                if (projectLevels[idx] == level)
                {
                    progressByProject[idx].finalStep = nextStep;
                    nextStep += 1;
                }
            }
        }

        const size_t totalSteps = nextStep - 1;
        for (ProjectProgress& progress : progressByProject)
        {
            progress.totalSteps = totalSteps;
        }
        return Result(true);
    }

    static Result buildProjectCompilePhase(FileSystem& fs, ResolvedProject& resolvedProject, bool& anyObjectBuilt,
                                           const ProjectProgress& progress, size_t maxParallelJobsOverride = 0)
    {
        if (resolvedProject.parameters->execution.verbose)
        {
            printProjectTrace(resolvedProject);
        }
        return buildCompileSteps(fs, resolvedProject, anyObjectBuilt, progress, maxParallelJobsOverride);
    }

    static bool shouldStripLinkedArtifact(const ResolvedProject& resolvedProject)
    {
        if (resolvedProject.project->targetType == TargetType::StaticLibrary)
        {
            return false;
        }
        if (resolvedProject.compileFlags.optimizationLevel != Optimization::Release)
        {
            return false;
        }

        switch (resolvedProject.parameters->platform)
        {
        case Platform::Apple:
        case Platform::Linux: return true;
        case Platform::Unknown:
        case Platform::Windows:
        case Platform::Wasm: return false;
        }
        Assert::unreachable();
    }

    static Result buildStripCommand(const ResolvedProject& resolvedProject, CommandLine& commandLine)
    {
        SC_TRY(commandLine.append("strip"));
        switch (resolvedProject.parameters->platform)
        {
        case Platform::Apple: SC_TRY(commandLine.append("-x")); break;
        case Platform::Linux: SC_TRY(commandLine.append("--strip-unneeded")); break;
        case Platform::Unknown:
        case Platform::Windows:
        case Platform::Wasm: return Result::Error("Strip command is unsupported on this platform");
        }
        SC_TRY(commandLine.append(resolvedProject.executablePath.view()));
        return Result(true);
    }

    static Result buildProjectFinalPhase(FileSystem& fs, ResolvedProject& resolvedProject, bool anyObjectBuilt,
                                         const ProjectProgress& progress)
    {
        CommandLine finalCommand;
        String      finalCommandString = StringEncoding::Utf8;
        SC_TRY(buildFinalCommand(resolvedProject, finalCommand));
        SC_TRY(formatCommandLine(finalCommand, finalCommandString));
        CommandLine stripCommand;
        String      stripCommandString = StringEncoding::Utf8;
        const bool  shouldStrip        = shouldStripLinkedArtifact(resolvedProject);
        if (shouldStrip)
        {
            SC_TRY(buildStripCommand(resolvedProject, stripCommand));
            SC_TRY(formatCommandLine(stripCommand, stripCommandString));
            auto builder = StringBuilder::createForAppendingTo(finalCommandString);
            SC_TRY(builder.append("\n"));
            SC_TRY(builder.append(stripCommandString.view()));
            builder.finalize();
        }
        const RebuildDecision finalDecision =
            evaluateTargetArtifactRebuild(fs, resolvedProject, finalCommandString.view(), anyObjectBuilt);
        if (finalDecision != UpToDate)
        {
            if (resolvedProject.parameters->execution.verbose)
            {
                globalConsole->print("[trace] rebuild {} {} because {}\n", getFinalStepName(*resolvedProject.project),
                                     resolvedProject.project->targetName.view(),
                                     describeRebuildDecision(finalDecision));
            }
            globalConsole->print("[{}/{}] {} {}\n", progress.finalStep, progress.totalSteps,
                                 getFinalStepName(*resolvedProject.project),
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
            if (shouldStrip)
            {
                String stripStdOut = StringEncoding::Utf8;
                String stripStdErr = StringEncoding::Utf8;
                SC_TRY(
                    executeCommand(fs, stripCommand, StringView(), resolvedProject.adapter, stripStdOut, stripStdErr));
                if (resolvedProject.parameters->execution.verbose and not stripStdOut.isEmpty())
                {
                    globalConsole->print(stripStdOut.view());
                }
                if (not stripStdErr.isEmpty())
                {
                    globalConsole->printError(stripStdErr.view());
                    globalConsole->flushStdErr();
                }
            }
            SC_TRY(fs.writeString(resolvedProject.linkCommandPath.view(), finalCommandString.view()));
        }
        else if (resolvedProject.parameters->execution.verbose)
        {
            globalConsole->print("[{}/{}] SKIP {} {} ({})\n", progress.finalStep, progress.totalSteps,
                                 getFinalStepName(*resolvedProject.project), resolvedProject.project->targetName.view(),
                                 describeRebuildDecision(finalDecision));
            globalConsole->print("[trace] no work to do for {}\n", resolvedProject.project->targetName.view());
        }

        return Result(true);
    }

    static Result buildResolvedProjectWave(const Vector<ResolvedProject*>& waveProjects,
                                           const Vector<ResolvedProject>&  resolvedProjects,
                                           const Vector<ProjectProgress>&  progressByProject,
                                           size_t                          workspaceParallelJobs)
    {
        if (waveProjects.isEmpty())
        {
            return Result(true);
        }
        if (waveProjects.size() == 1 or workspaceParallelJobs <= 1)
        {
            FileSystem fs;
            SC_TRY(fs.init("."));
            size_t projectIndex = 0;
            SC_TRY(findResolvedProjectIndex(resolvedProjects, *waveProjects[0]->project, projectIndex));
            return buildProject(fs, *waveProjects[0], progressByProject[projectIndex], workspaceParallelJobs);
        }

        FileSystem fs;
        SC_TRY(fs.init("."));

        Vector<bool> anyObjectBuiltFlags;
        for (size_t idx = 0; idx < waveProjects.size(); ++idx)
        {
            SC_TRY(anyObjectBuiltFlags.push_back(false));
        }

        ParallelCompileLimiter limiter;
        SC_TRY(limiter.create(fs, workspaceParallelJobs));

        for (size_t projectIndex = 0; projectIndex < waveProjects.size(); ++projectIndex)
        {
            ResolvedProject& resolvedProject      = *waveProjects[projectIndex];
            size_t           resolvedProjectIndex = 0;
            SC_TRY(findResolvedProjectIndex(resolvedProjects, *resolvedProject.project, resolvedProjectIndex));
            const ProjectProgress& progress = progressByProject[resolvedProjectIndex];
            if (resolvedProject.parameters->execution.verbose)
            {
                printProjectTrace(resolvedProject);
                globalConsole->print("[trace] workspace compile wave jobs = {}\n", workspaceParallelJobs);
            }

            size_t compileStep = 0;
            for (ResolvedSource& source : resolvedProject.sources)
            {
                compileStep++;

                CommandLine commandLine;
                String      commandString = StringEncoding::Utf8;
                SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
                SC_TRY(formatCommandLine(commandLine, commandString));
                const RebuildDecision compileDecision =
                    evaluateObjectRebuild(fs, resolvedProject, source, commandString.view());
                if (compileDecision == UpToDate)
                {
                    if (resolvedProject.parameters->execution.verbose)
                    {
                        globalConsole->print("[{}/{}] SKIP {} {} ({})\n", progress.compileStartStep + compileStep - 1,
                                             progress.totalSteps, getCompileStepName(source.type),
                                             source.displayPath.view(), describeRebuildDecision(compileDecision));
                    }
                    continue;
                }
                if (resolvedProject.parameters->execution.verbose)
                {
                    globalConsole->print("[trace] rebuild {} {} because {}\n", getCompileStepName(source.type),
                                         source.displayPath.view(), describeRebuildDecision(compileDecision));
                }

                SC_TRY(limiter.launch(progress.compileStartStep + compileStep - 1, progress.totalSteps, resolvedProject,
                                      source, commandLine, commandString, anyObjectBuiltFlags[projectIndex]));
            }
        }
        SC_TRY(limiter.close());

        for (size_t projectIndex = 0; projectIndex < waveProjects.size(); ++projectIndex)
        {
            size_t resolvedProjectIndex = 0;
            SC_TRY(
                findResolvedProjectIndex(resolvedProjects, *waveProjects[projectIndex]->project, resolvedProjectIndex));
            SC_TRY(buildProjectFinalPhase(fs, *waveProjects[projectIndex], anyObjectBuiltFlags[projectIndex],
                                          progressByProject[resolvedProjectIndex]));
        }
        return Result(true);
    }

    static Result buildResolvedProjects(const Parameters& parameters, Vector<ResolvedProject>& resolvedProjects)
    {
        if (resolvedProjects.isEmpty())
        {
            return Result(true);
        }

        const size_t            workspaceParallelJobs = computeWorkspaceParallelJobs(parameters);
        Vector<ProjectProgress> progressByProject;
        if (resolvedProjects.size() == 1 or workspaceParallelJobs <= 1)
        {
            FileSystem fs;
            SC_TRY(fs.init("."));
            Vector<size_t> projectLevels;
            SC_TRY(projectLevels.push_back(0));
            SC_TRY(computeResolvedProjectProgress(resolvedProjects, projectLevels, progressByProject));
            return buildProject(fs, resolvedProjects[0], progressByProject[0], workspaceParallelJobs);
        }

        Vector<size_t> projectLevels;
        SC_TRY(computeResolvedProjectLevels(resolvedProjects, projectLevels));
        SC_TRY(computeResolvedProjectProgress(resolvedProjects, projectLevels, progressByProject));

        size_t maxLevel = 0;
        for (size_t projectLevel : projectLevels)
        {
            if (projectLevel > maxLevel)
            {
                maxLevel = projectLevel;
            }
        }

        Vector<ResolvedProject*> waveProjects;
        for (size_t level = 0; level <= maxLevel; ++level)
        {
            waveProjects.clear();
            for (size_t idx = 0; idx < resolvedProjects.size(); ++idx)
            {
                if (projectLevels[idx] == level)
                {
                    SC_TRY(waveProjects.push_back(&resolvedProjects[idx]));
                }
            }
            SC_TRY(buildResolvedProjectWave(waveProjects, resolvedProjects, progressByProject, workspaceParallelJobs));
        }
        return Result(true);
    }

    static Result execute(Action::ConfigureFunction configure, const Action& action, String* outputExecutable)
    {
        Definition definition;
        SC_TRY(configure(definition, action.parameters));

        FilePathsResolver filePathsResolver;
        SC_TRY(filePathsResolver.resolve(definition));

        const Workspace* workspace = nullptr;
        SC_TRY(findWorkspace(definition, action.workspaceName, workspace));

        Vector<ResolvedProject> resolvedProjects;
        SC_TRY(resolveProjectsInBuildOrder(action, *workspace, filePathsResolver, resolvedProjects));

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
        }
        SC_TRY(buildResolvedProjects(action.parameters, resolvedProjects));

        if (not workspaceCompileCommands.isEmpty())
        {
            SC_TRY(writeCompileCommandsArray(fs, resolvedProjects[0].workspaceCompileCommandsPath.view(),
                                             workspaceCompileCommands.toSpanConst()));
        }

        if (action.action == Action::Run)
        {
            SC_TRY_MSG(resolvedProjects.size() == 1, "Run requires selecting a single project");
            SC_TRY_MSG(resolvedProjects[0].project->targetType == TargetType::ConsoleExecutable or
                           resolvedProjects[0].project->targetType == TargetType::GUIApplication,
                       "Run requires an executable target");
            SC_TRY(runExecutable(resolvedProjects[0].executablePath.view(), action));
        }
        return Result(true);
    }

    static Result buildProject(FileSystem& fs, ResolvedProject& resolvedProject, const ProjectProgress& progress,
                               size_t maxParallelJobsOverride = 0)
    {
        bool anyObjectBuilt = false;
        SC_TRY(buildProjectCompilePhase(fs, resolvedProject, anyObjectBuilt, progress, maxParallelJobsOverride));
        return buildProjectFinalPhase(fs, resolvedProject, anyObjectBuilt, progress);
    }

    static Result buildCompileSteps(FileSystem& fs, ResolvedProject& resolvedProject, bool& anyObjectBuilt,
                                    const ProjectProgress& progress, size_t maxParallelJobsOverride = 0)
    {
        size_t maxParallelJobs = computeMaxParallelCompileJobs(resolvedProject);
        if (maxParallelJobsOverride != 0 and maxParallelJobsOverride < maxParallelJobs)
        {
            maxParallelJobs = maxParallelJobsOverride;
        }
        if (maxParallelJobs <= 1 or resolvedProject.sources.size() <= 1)
        {
            return buildCompileStepsSequential(fs, resolvedProject, anyObjectBuilt, progress);
        }
        return buildCompileStepsParallel(fs, resolvedProject, anyObjectBuilt, progress, maxParallelJobs);
    }

    static Result buildCompileStepsSequential(FileSystem& fs, ResolvedProject& resolvedProject, bool& anyObjectBuilt,
                                              const ProjectProgress& progress)
    {
        size_t compileStep = 0;
        for (ResolvedSource& source : resolvedProject.sources)
        {
            compileStep++;

            CommandLine commandLine;
            String      commandString = StringEncoding::Utf8;
            SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
            SC_TRY(formatCommandLine(commandLine, commandString));
            const RebuildDecision compileDecision =
                evaluateObjectRebuild(fs, resolvedProject, source, commandString.view());
            if (compileDecision == UpToDate)
            {
                if (resolvedProject.parameters->execution.verbose)
                {
                    globalConsole->print("[{}/{}] SKIP {} {} ({})\n", progress.compileStartStep + compileStep - 1,
                                         progress.totalSteps, getCompileStepName(source.type),
                                         source.displayPath.view(), describeRebuildDecision(compileDecision));
                }
                continue;
            }
            if (resolvedProject.parameters->execution.verbose)
            {
                globalConsole->print("[trace] rebuild {} {} because {}\n", getCompileStepName(source.type),
                                     source.displayPath.view(), describeRebuildDecision(compileDecision));
            }

            SC_TRY(makeParentDirectory(fs, source.objectPath.view()));
            globalConsole->print("[{}/{}] {} {}\n", progress.compileStartStep + compileStep - 1, progress.totalSteps,
                                 getCompileStepName(source.type), source.displayPath.view());
            globalConsole->flush();

            String stdOut = StringEncoding::Utf8;
            String stdErr = StringEncoding::Utf8;
            SC_TRY(executeCompileCommand(fs, commandLine, source.responsePath.view(), resolvedProject, source, stdOut,
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
            SC_TRY(fs.writeString(source.commandPath.view(), commandString.view()));
            anyObjectBuilt = true;
        }
        return Result(true);
    }

    static Result buildCompileStepsParallel(FileSystem& fs, ResolvedProject& resolvedProject, bool& anyObjectBuilt,
                                            const ProjectProgress& progress, size_t maxParallelJobs)
    {
        if (resolvedProject.parameters->execution.verbose)
        {
            globalConsole->print("[trace] parallel compile jobs = {}\n", maxParallelJobs);
        }

        ParallelCompileLimiter limiter;
        SC_TRY(limiter.create(fs, maxParallelJobs));

        size_t compileStep = 0;
        for (ResolvedSource& source : resolvedProject.sources)
        {
            compileStep++;

            CommandLine commandLine;
            String      commandString = StringEncoding::Utf8;
            SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
            SC_TRY(formatCommandLine(commandLine, commandString));
            const RebuildDecision compileDecision =
                evaluateObjectRebuild(fs, resolvedProject, source, commandString.view());
            if (compileDecision == UpToDate)
            {
                if (resolvedProject.parameters->execution.verbose)
                {
                    globalConsole->print("[{}/{}] SKIP {} {} ({})\n", progress.compileStartStep + compileStep - 1,
                                         progress.totalSteps, getCompileStepName(source.type),
                                         source.displayPath.view(), describeRebuildDecision(compileDecision));
                }
                continue;
            }
            if (resolvedProject.parameters->execution.verbose)
            {
                globalConsole->print("[trace] rebuild {} {} because {}\n", getCompileStepName(source.type),
                                     source.displayPath.view(), describeRebuildDecision(compileDecision));
            }

            SC_TRY(limiter.launch(progress.compileStartStep + compileStep - 1, progress.totalSteps, resolvedProject,
                                  source, commandLine, commandString, anyObjectBuilt));
        }
        return limiter.close();
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
                                 const CompilerAdapter& adapter, String& stdOut, String& stdErr,
                                 bool printOnFailure = true)
    {
        SC_TRY(maybeWriteResponseFile(fs, commandLine, responsePath, adapter));

        StringSpan             views[128];
        Span<const StringSpan> args;
        SC_TRY(commandLine.toViews(views, args));

        Process process;
        SC_TRY(process.exec(args, stdOut, Process::StdIn(), stdErr));
        if (process.getExitStatus() != 0)
        {
            if (printOnFailure)
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
            }
            return Result::Error("Native backend command failed");
        }
        return Result(true);
    }

    static Result executeCompileCommand(FileSystem& fs, CommandLine& commandLine, StringView responsePath,
                                        const ResolvedProject& resolvedProject, const ResolvedSource& source,
                                        String& stdOut, String& stdErr)
    {
        Result commandResult =
            executeCommand(fs, commandLine, responsePath, resolvedProject.adapter, stdOut, stdErr, false);
        if (resolvedProject.parameters->execution.useCompilerDependencies and resolvedProject.adapter.isMSVCStyle())
        {
            SC_TRY(writeWindowsDependencyFile(fs, resolvedProject, source, stdOut, stdErr));
        }
        if (not commandResult)
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
            return commandResult;
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
        const size_t inlineCommandBytes = commandLine.totalCharacters() + commandLine.size() + 1;
        if (not(commandLine.size() > 48 or commandLine.totalCharacters() > 4096 or
                inlineCommandBytes >= Process::InlineCommandStorageCapacity))
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
        SC_TRY(resolveWorkspaceDependencies(parameters, workspace, project, configuration, resolvedProject));

        SC_TRY(expandConfiguredPath(parameters.directories.outputsDirectory.view(), configuration.outputPath.view(),
                                    resolvedProject.variables, resolvedProject.targetDirectory));
        SC_TRY(expandConfiguredPath(parameters.directories.intermediatesDirectory.view(),
                                    configuration.intermediatesPath.view(), resolvedProject.variables,
                                    resolvedProject.intermediateDirectory));
        String artifactName = StringEncoding::Utf8;
        SC_TRY(computeArtifactName(parameters.platform, project, artifactName));
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
            SC_TRY(StringBuilder::createForAppendingTo(objectRelative)
                       .append(parameters.platform == Platform::Windows ? ".obj"_a8 : ".o"_a8));
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
        if (resolvedProject.adapter.isMSVCStyle())
        {
            SC_TRY(commandLine.append("/nologo"));
            SC_TRY(appendTargeting(commandLine, resolvedProject, true));
            SC_TRY(appendWarningFlags(commandLine, source.compileFlags, usesCppDriver, resolvedProject.adapter));
            SC_TRY(appendCompileFlags(commandLine, resolvedProject, source, usesCppDriver));
            if (resolvedProject.parameters->execution.useCompilerDependencies)
            {
                SC_TRY(commandLine.append("/showIncludes"));
            }
            SC_TRY(commandLine.append(usesCppDriver ? "/TP"_a8 : "/TC"_a8));
            SC_TRY(commandLine.append("/c"));

            String objectFlag = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(objectFlag, "/Fo{}", source.objectPath.view()));
            SC_TRY(commandLine.append(objectFlag.view()));

            String pdbPath = StringEncoding::Utf8;
            SC_TRY(Path::join(pdbPath, {resolvedProject.intermediateDirectory.view(), "native.pdb"}));
            String pdbFlag = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(pdbFlag, "/Fd{}", pdbPath.view()));
            SC_TRY(commandLine.append("/FS"));
            SC_TRY(commandLine.append(pdbFlag.view()));
            SC_TRY(commandLine.append(source.sourcePath.view()));
            return Result(true);
        }
        SC_TRY(appendTargeting(commandLine, resolvedProject, true));
        SC_TRY(appendWarningFlags(commandLine, source.compileFlags, usesCppDriver, resolvedProject.adapter));
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
        if (resolvedProject.adapter.isClangCL() and
            resolvedProject.adapter.executableLink.view() == resolvedProject.adapter.executableCpp.view())
        {
            SC_TRY(commandLine.append(resolvedProject.adapter.executableLink.view()));
            SC_TRY(commandLine.append("/nologo"));
            SC_TRY(appendTargeting(commandLine, resolvedProject, false));
            for (const ResolvedSource& source : resolvedProject.sources)
            {
                SC_TRY(commandLine.append(source.objectPath.view()));
            }
            SC_TRY(commandLine.append("/link"));
            SC_TRY(commandLine.append("/NOLOGO"));
            switch (resolvedProject.project->targetType)
            {
            case TargetType::ConsoleExecutable: SC_TRY(commandLine.append("/SUBSYSTEM:CONSOLE")); break;
            case TargetType::GUIApplication: SC_TRY(commandLine.append("/SUBSYSTEM:WINDOWS")); break;
            case TargetType::SharedLibrary: SC_TRY(commandLine.append("/DLL")); break;
            case TargetType::StaticLibrary: break;
            }
            switch (resolvedProject.compileFlags.optimizationLevel)
            {
            case Optimization::Debug: SC_TRY(commandLine.append("/DEBUG")); break;
            case Optimization::Release:
                SC_TRY(commandLine.append("/OPT:REF"));
                SC_TRY(commandLine.append("/OPT:ICF"));
                break;
            }
            SC_TRY(appendLinkFlags(commandLine, resolvedProject));
            String outFlag = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(outFlag, "/OUT:{}", resolvedProject.executablePath.view()));
            SC_TRY(commandLine.append(outFlag.view()));
            return Result(true);
        }

        if (resolvedProject.adapter.isMSVCStyle())
        {
            SC_TRY(commandLine.append(resolvedProject.adapter.executableLink.view()));
            SC_TRY(commandLine.append("/NOLOGO"));
            SC_TRY(appendTargeting(commandLine, resolvedProject, false));
            switch (resolvedProject.project->targetType)
            {
            case TargetType::ConsoleExecutable: SC_TRY(commandLine.append("/SUBSYSTEM:CONSOLE")); break;
            case TargetType::GUIApplication: SC_TRY(commandLine.append("/SUBSYSTEM:WINDOWS")); break;
            case TargetType::SharedLibrary: SC_TRY(commandLine.append("/DLL")); break;
            case TargetType::StaticLibrary: break;
            }
            switch (resolvedProject.compileFlags.optimizationLevel)
            {
            case Optimization::Debug: SC_TRY(commandLine.append("/DEBUG")); break;
            case Optimization::Release:
                SC_TRY(commandLine.append("/OPT:REF"));
                SC_TRY(commandLine.append("/OPT:ICF"));
                break;
            }
            for (const ResolvedSource& source : resolvedProject.sources)
            {
                SC_TRY(commandLine.append(source.objectPath.view()));
            }
            SC_TRY(appendLinkFlags(commandLine, resolvedProject));
            String outFlag = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(outFlag, "/OUT:{}", resolvedProject.executablePath.view()));
            SC_TRY(commandLine.append(outFlag.view()));
            return Result(true);
        }

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
        if (resolvedProject.project->targetType == TargetType::SharedLibrary)
        {
            if (resolvedProject.parameters->platform == Platform::Apple)
            {
                SC_TRY(commandLine.append("-dynamiclib"));
            }
            else
            {
                SC_TRY(commandLine.append("-shared"));
            }
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
        if (resolvedProject.adapter.isMSVCStyle())
        {
            SC_TRY(commandLine.append("/NOLOGO"));
            String outFlag = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(outFlag, "/OUT:{}", resolvedProject.executablePath.view()));
            SC_TRY(commandLine.append(outFlag.view()));
            for (const ResolvedSource& source : resolvedProject.sources)
            {
                SC_TRY(commandLine.append(source.objectPath.view()));
            }
            return Result(true);
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
        case TargetType::SharedLibrary: return buildLinkCommand(resolvedProject, commandLine);
        case TargetType::StaticLibrary: return buildArchiveCommand(resolvedProject, commandLine);
        }
        Assert::unreachable();
    }

    static Result appendCompileFlags(CommandLine& commandLine, const ResolvedProject& resolvedProject,
                                     const ResolvedSource& source, bool usesCppDriver)
    {
        const CompileFlags& flags = source.compileFlags;
        if (resolvedProject.adapter.isMSVCStyle())
        {
            SC_TRY(commandLine.append("/permissive-"));
            if (usesCppDriver)
            {
                const StringView standardFlag = msvcCppStandardFlag(flags.cppStandard);
                if (not standardFlag.isEmpty())
                {
                    SC_TRY(commandLine.append(standardFlag));
                }
                if (flags.enableExceptions)
                {
                    SC_TRY(commandLine.append("/EHsc"));
                }
                else
                {
                    SC_TRY(commandLine.append("/EHs-c-"));
                }
                SC_TRY(commandLine.append(flags.enableRTTI ? "/GR"_a8 : "/GR-"_a8));
            }

            if (flags.enableCoverage)
            {
                return Result::Error("Windows native coverage is not implemented yet");
            }

            switch (flags.optimizationLevel)
            {
            case Optimization::Debug:
                SC_TRY(commandLine.append("/D_DEBUG=1"));
                SC_TRY(commandLine.append("/Od"));
                SC_TRY(commandLine.append("/Z7"));
                SC_TRY(commandLine.append("/MTd"));
                break;
            case Optimization::Release:
                SC_TRY(commandLine.append("/DNDEBUG=1"));
                SC_TRY(commandLine.append("/O2"));
                SC_TRY(commandLine.append("/Gy"));
                SC_TRY(commandLine.append("/Gw"));
                SC_TRY(commandLine.append("/MT"));
                break;
            }

            if (flags.enableASAN)
            {
                SC_TRY(commandLine.append("/fsanitize=address"));
            }

            for (const String& define : flags.defines)
            {
                String expanded = StringEncoding::Utf8;
                SC_TRY(expandVariables(define.view(), resolvedProject.variables, expanded));
                String option = StringEncoding::Utf8;
                SC_TRY(StringBuilder::format(option, "/D{}", expanded.view()));
                SC_TRY(commandLine.append(option.view()));
            }
            for (const String& includePath : flags.includePaths)
            {
                String absolutePath = StringEncoding::Utf8;
                SC_TRY(resolveBuildPath(resolvedProject.project->rootDirectory.view(), includePath.view(),
                                        resolvedProject.variables, absolutePath));
                String option = StringEncoding::Utf8;
                SC_TRY(StringBuilder::format(option, "/I{}", absolutePath.view()));
                SC_TRY(commandLine.append(option.view()));
            }
            const Vector<String>& extraCompilerFlags = resolvedProject.parameters->toolchain.extraCompilerFlags;
            for (const String& extraCompilerFlag : extraCompilerFlags)
            {
                SC_TRY(commandLine.append(extraCompilerFlag.view()));
            }
            return Result(true);
        }

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
        if (resolvedProject.project->targetType == TargetType::SharedLibrary)
        {
            SC_TRY(commandLine.append("-fPIC"));
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
            SC_TRY(commandLine.append("-ffunction-sections"));
            SC_TRY(commandLine.append("-fdata-sections"));
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

    static Result appendWarningFlags(CommandLine& commandLine, const CompileFlags& flags, bool usesCppDriver,
                                     const CompilerAdapter& adapter)
    {
        if (adapter.isMSVCStyle())
        {
            SC_TRY(commandLine.append("/W4"));
            SC_TRY(commandLine.append("/WX"));
            for (const Warning& warning : flags.warnings)
            {
                if (warning.state == Warning::Disabled and warning.type == Warning::MSVCWarning)
                {
                    String option = StringEncoding::Utf8;
                    SC_TRY(StringBuilder::format(option, "/wd{}", warning.number));
                    SC_TRY(commandLine.append(option.view()));
                }
                else if (warning.state == Warning::Disabled and adapter.isClangCL() and
                         warning.type != Warning::MSVCWarning)
                {
                    String option = StringEncoding::Utf8;
                    SC_TRY(StringBuilder::format(option, "-Wno-{}", warning.name));
                    SC_TRY(commandLine.append(option.view()));
                }
            }
            SC_COMPILER_UNUSED(usesCppDriver);
            return Result(true);
        }

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
        for (const String& workspaceLibrary : resolvedProject.workspaceDependencyArtifacts)
        {
            SC_TRY(commandLine.append(workspaceLibrary.view()));
        }

        if (resolvedProject.adapter.isMSVCStyle())
        {
            for (const String& libraryPath : resolvedProject.linkFlags.libraryPaths)
            {
                String absoluteLibraryPath = StringEncoding::Utf8;
                SC_TRY(resolveBuildPath(resolvedProject.project->rootDirectory.view(), libraryPath.view(),
                                        resolvedProject.variables, absoluteLibraryPath));
                String option = StringEncoding::Utf8;
                SC_TRY(StringBuilder::format(option, "/LIBPATH:{}", absoluteLibraryPath.view()));
                SC_TRY(commandLine.append(option.view()));
            }

            for (const String& library : resolvedProject.linkFlags.libraries)
            {
                const StringView libraryView = library.view();
                if (libraryView.containsCodePoint('/') or libraryView.containsCodePoint('\\') or
                    libraryView.endsWith(".lib") or libraryView.endsWith(".dll") or libraryView.endsWith(".obj"))
                {
                    String absoluteLibrary = StringEncoding::Utf8;
                    SC_TRY(resolveBuildPath(resolvedProject.project->rootDirectory.view(), libraryView,
                                            resolvedProject.variables, absoluteLibrary));
                    SC_TRY(commandLine.append(absoluteLibrary.view()));
                }
                else
                {
                    String option = StringEncoding::Utf8;
                    SC_TRY(StringBuilder::format(option, "{}.lib", libraryView));
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

        const bool isExecutableTarget = resolvedProject.project->targetType == TargetType::ConsoleExecutable or
                                        resolvedProject.project->targetType == TargetType::GUIApplication;
        if (resolvedProject.parameters->platform == Platform::Linux and isExecutableTarget and
            not resolvedProject.project->exportLibraries.isEmpty())
        {
            SC_TRY(commandLine.append("-rdynamic"));
        }

        if (resolvedProject.project->targetType != TargetType::StaticLibrary and
            resolvedProject.compileFlags.optimizationLevel == Optimization::Release)
        {
            if (resolvedProject.parameters->platform == Platform::Apple)
            {
                SC_TRY(commandLine.append("-dead_strip"));
            }
            else if (resolvedProject.parameters->platform == Platform::Linux)
            {
                SC_TRY(commandLine.append("-Wl,--gc-sections"));
            }
        }

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

    static Result resolveProjectsInBuildOrder(const Action& action, const Workspace& workspace,
                                              const FilePathsResolver& filePathsResolver,
                                              Vector<ResolvedProject>& resolvedProjects)
    {
        Vector<const Project*> orderedProjects;
        if (action.allTargets)
        {
            for (const Project& project : workspace.projects)
            {
                SC_TRY(appendProjectBuildOrder(workspace, action.parameters.platform, project, action.configurationName,
                                               orderedProjects));
            }
        }
        else
        {
            const Project*       project       = nullptr;
            const Configuration* configuration = nullptr;
            SC_TRY(findProjectConfiguration(workspace, action.projectName, action.configurationName, project,
                                            configuration));
            SC_COMPILER_UNUSED(configuration);
            SC_TRY(appendProjectBuildOrder(workspace, action.parameters.platform, *project, action.configurationName,
                                           orderedProjects));
        }

        for (const Project* project : orderedProjects)
        {
            const Configuration* configuration = project->getConfiguration(action.configurationName);
            SC_TRY_MSG(configuration != nullptr, "Cannot find requested configuration");
            ResolvedProject resolvedProject;
            SC_TRY(resolveProject(action.parameters, workspace, *project, *configuration, filePathsResolver,
                                  resolvedProject));
            SC_TRY(resolvedProjects.push_back(move(resolvedProject)));
        }
        return Result(true);
    }

    static Result appendProjectBuildOrder(const Workspace& workspace, Platform::Type platform, const Project& project,
                                          StringView configurationName, Vector<const Project*>& orderedProjects)
    {
        Vector<const Project*> stack;
        return appendProjectBuildOrderRecursive(workspace, platform, project, configurationName, stack,
                                                orderedProjects);
    }

    static Result appendProjectBuildOrderRecursive(const Workspace& workspace, Platform::Type platform,
                                                   const Project& project, StringView configurationName,
                                                   Vector<const Project*>& stack,
                                                   Vector<const Project*>& orderedProjects)
    {
        size_t index = 0;
        if (orderedProjects.find([&](const Project* item) { return item == &project; }, &index))
        {
            return Result(true);
        }
        SC_TRY_MSG(not stack.find([&](const Project* item) { return item == &project; }, &index),
                   "Native backend project dependency cycle detected");
        SC_TRY(stack.push_back(&project));

        Vector<const Project*> dependencies;
        SC_TRY(findWorkspaceDependencies(workspace, platform, project, configurationName, dependencies));
        for (const Project* dependency : dependencies)
        {
            SC_TRY(appendProjectBuildOrderRecursive(workspace, platform, *dependency, configurationName, stack,
                                                    orderedProjects));
        }
        (void)stack.pop_back();
        SC_TRY(orderedProjects.push_back(&project));
        return Result(true);
    }

    static Result findWorkspaceDependencies(const Workspace& workspace, Platform::Type platform, const Project& project,
                                            StringView configurationName, Vector<const Project*>& dependencies)
    {
        const Configuration* configuration = project.getConfiguration(configurationName);
        SC_TRY_MSG(configuration != nullptr, "Configuration not found");

        LinkFlags        mergedLinkFlags;
        const LinkFlags* linkOpinions[] = {&configuration->link, &project.link};
        SC_TRY(LinkFlags::merge(linkOpinions, mergedLinkFlags));

        for (const String& library : mergedLinkFlags.libraries)
        {
            const Project* dependency = nullptr;
            if (findWorkspaceStaticLibraryDependency(workspace, platform, library.view(), dependency))
            {
                size_t index = 0;
                if (dependency != &project and
                    not dependencies.find([&](const Project* item) { return item == dependency; }, &index))
                {
                    SC_TRY(dependencies.push_back(dependency));
                }
            }
        }
        return Result(true);
    }

    static Result resolveWorkspaceDependencies(const Parameters& parameters, const Workspace& workspace,
                                               const Project& project, const Configuration& configuration,
                                               ResolvedProject& resolvedProject)
    {
        SC_TRY(findWorkspaceDependencies(workspace, parameters.platform, project, configuration.name.view(),
                                         resolvedProject.workspaceDependencies));

        Vector<String> externalLibraries;
        for (const String& library : resolvedProject.linkFlags.libraries)
        {
            const Project* dependency = nullptr;
            if (findWorkspaceStaticLibraryDependency(workspace, parameters.platform, library.view(), dependency))
            {
                String    dependencyOutputDirectory = StringEncoding::Utf8;
                String    dependencyArtifactName    = StringEncoding::Utf8;
                String    dependencyArtifactPath    = StringEncoding::Utf8;
                Variables dependencyVariables;

                const Configuration* dependencyConfiguration = dependency->getConfiguration(configuration.name.view());
                SC_TRY_MSG(dependencyConfiguration != nullptr, "Dependency configuration not found");
                SC_TRY(fillVariables(parameters, *dependency, *dependencyConfiguration,
                                     resolvedProject.adapter.displayName.view(), dependencyVariables));
                SC_TRY(expandConfiguredPath(parameters.directories.outputsDirectory.view(),
                                            dependencyConfiguration->outputPath.view(), dependencyVariables,
                                            dependencyOutputDirectory));
                SC_TRY(computeArtifactName(parameters.platform, *dependency, dependencyArtifactName));
                SC_TRY(Path::join(dependencyArtifactPath,
                                  {dependencyOutputDirectory.view(), dependencyArtifactName.view()}));
                SC_TRY(resolvedProject.workspaceDependencyArtifacts.push_back(move(dependencyArtifactPath)));
            }
            else
            {
                String externalLibrary = StringEncoding::Utf8;
                SC_TRY(externalLibrary.assign(library.view()));
                SC_TRY(externalLibraries.push_back(move(externalLibrary)));
            }
        }
        resolvedProject.linkFlags.libraries = move(externalLibraries);
        return Result(true);
    }

    static bool findWorkspaceStaticLibraryDependency(const Workspace& workspace, Platform::Type platform,
                                                     StringView libraryReference, const Project*& dependencyProject)
    {
        for (const Project& candidate : workspace.projects)
        {
            if (candidate.targetType != TargetType::StaticLibrary)
            {
                continue;
            }
            if (matchesWorkspaceLibraryReference(platform, candidate, libraryReference))
            {
                dependencyProject = &candidate;
                return true;
            }
        }
        dependencyProject = nullptr;
        return false;
    }

    static bool matchesWorkspaceLibraryReference(Platform::Type platform, const Project& candidate,
                                                 StringView libraryReference)
    {
        if (libraryReference == candidate.name.view() or libraryReference == candidate.targetName.view())
        {
            return true;
        }

        String artifactName = StringEncoding::Utf8;
        if (not computeArtifactName(platform, candidate, artifactName))
        {
            return false;
        }
        if (libraryReference == artifactName.view())
        {
            return true;
        }
        const StringView artifactView = artifactName.view();
        if (artifactView.startsWith("lib") and artifactView.endsWith(".a"))
        {
            const StringView strippedName = artifactView.sliceStart(3).sliceEnd(2);
            if (libraryReference == strippedName)
            {
                return true;
            }
        }
        return false;
    }

    static Result appendTargeting(CommandLine& commandLine, const ResolvedProject& resolvedProject, bool forCompiler)
    {
        if (resolvedProject.adapter.isMSVCStyle())
        {
            StringView targetTriple = resolvedProject.parameters->toolchain.targetTriple.view();
            if (targetTriple.isEmpty() and resolvedProject.adapter.isClangCL())
            {
                switch (resolvedProject.parameters->architecture)
                {
                case Architecture::Intel64: targetTriple = "x86_64-pc-windows-msvc"; break;
                case Architecture::Intel32: targetTriple = "i686-pc-windows-msvc"; break;
                case Architecture::Arm64: targetTriple = "aarch64-pc-windows-msvc"; break;
                case Architecture::Any:
                case Architecture::Wasm: break;
                }
            }
            if (not resolvedProject.parameters->toolchain.sysroot.isEmpty())
            {
                return Result::Error("Windows native sysroot selection is not implemented yet");
            }
            if (not targetTriple.isEmpty())
            {
                if (resolvedProject.adapter.isClangCL())
                {
                    String option = StringEncoding::Utf8;
                    SC_TRY(StringBuilder::format(option, "--target={}", targetTriple));
                    SC_TRY(commandLine.append(option.view()));
                }
                else
                {
                    return Result::Error("MSVC native target triple selection is not implemented yet");
                }
            }
            SC_COMPILER_UNUSED(forCompiler);
            return Result(true);
        }

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

    static constexpr StringView msvcCppStandardFlag(CppStandard::Type type)
    {
        switch (type)
        {
        case CppStandard::CPP11:
        case CppStandard::CPP14: return ""_a8;
        case CppStandard::CPP17: return "/std:c++17"_a8;
        case CppStandard::CPP20: return "/std:c++20"_a8;
        case CppStandard::CPP23: return "/std:c++latest"_a8;
        }
        Assert::unreachable();
    }

    static RebuildDecision evaluateObjectRebuild(FileSystem& fs, const ResolvedProject& resolvedProject,
                                                 const ResolvedSource& source, StringView commandLine)
    {
        if (not fs.existsAndIsFile(source.objectPath.view()))
            return MissingOutput;
        if (not fs.existsAndIsFile(source.commandPath.view()))
            return MissingCommandFile;
        if (resolvedProject.parameters->execution.useCompilerDependencies and
            not fs.existsAndIsFile(source.dependencyPath.view()))
        {
            return MissingDependencyFile;
        }

        String existingCommand = StringEncoding::Utf8;
        if (not fs.read(source.commandPath.view(), existingCommand))
            return MissingCommandFile;
        if (existingCommand.view() != commandLine)
            return CommandChanged;

        FileSystem::FileStat objectStat;
        if (not fs.stat(source.objectPath.view(), objectStat))
            return OutputStatUnavailable;

        Vector<String> dependencies;
        if (resolvedProject.parameters->execution.useCompilerDependencies)
        {
            if (not parseDependencyFile(fs, source.dependencyPath.view(), resolvedProject.project->rootDirectory.view(),
                                        resolvedProject.adapter, dependencies))
            {
                return DependencyScanFailed;
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

        FileSystem::FileStat sourceStat;
        if (not fs.stat(source.sourcePath.view(), sourceStat))
            return InputChanged;
        if (sourceStat.modifiedTime.milliseconds > objectStat.modifiedTime.milliseconds)
            return InputChanged;

        for (const String& dependency : dependencies)
        {
            FileSystem::FileStat dependencyStat;
            if (not fs.stat(dependency.view(), dependencyStat))
                return InputChanged;
            if (dependencyStat.modifiedTime.milliseconds > objectStat.modifiedTime.milliseconds)
                return InputChanged;
        }
        return UpToDate;
    }

    static RebuildDecision evaluateTargetArtifactRebuild(FileSystem& fs, const ResolvedProject& resolvedProject,
                                                         StringView commandLine, bool anyObjectBuilt)
    {
        if (anyObjectBuilt)
            return PreviousBuildStepRan;
        if (not fs.existsAndIsFile(resolvedProject.executablePath.view()))
            return MissingOutput;
        if (not fs.existsAndIsFile(resolvedProject.linkCommandPath.view()))
            return MissingCommandFile;

        String existingCommand = StringEncoding::Utf8;
        if (not fs.read(resolvedProject.linkCommandPath.view(), existingCommand))
            return MissingCommandFile;
        if (existingCommand.view() != commandLine)
            return CommandChanged;

        FileSystem::FileStat executableStat;
        if (not fs.stat(resolvedProject.executablePath.view(), executableStat))
            return OutputStatUnavailable;
        for (const ResolvedSource& source : resolvedProject.sources)
        {
            FileSystem::FileStat objectStat;
            if (not fs.stat(source.objectPath.view(), objectStat))
                return InputChanged;
            if (objectStat.modifiedTime.milliseconds > executableStat.modifiedTime.milliseconds)
                return InputChanged;
        }
        return UpToDate;
    }

    static Result parseDependencyFile(FileSystem& fs, StringView dependencyPath, StringView projectRoot,
                                      const CompilerAdapter& adapter, Vector<String>& dependencies)
    {
        if (adapter.isMSVCStyle())
        {
            return parseWindowsDependencyFile(fs, dependencyPath, projectRoot, dependencies);
        }

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

    static Result parseWindowsDependencyFile(FileSystem& fs, StringView dependencyPath, StringView projectRoot,
                                             Vector<String>& dependencies)
    {
        String contents = StringEncoding::Utf8;
        SC_TRY(fs.read(dependencyPath, contents));

        const StringView dependencyData = contents.view();
        const char*      bytes          = dependencyData.bytesWithoutTerminator();
        size_t           lineStart      = 0;
        for (size_t idx = 0; idx <= dependencyData.sizeInBytes(); ++idx)
        {
            if (idx < dependencyData.sizeInBytes() and bytes[idx] != '\n')
            {
                continue;
            }

            StringView line = dependencyData.sliceStartEnd(lineStart, idx).trimEndAnyOf({'\r'});
            lineStart       = idx + 1;
            line            = line.trimWhiteSpaces();
            if (line.isEmpty())
            {
                continue;
            }

            String dependency = StringEncoding::Utf8;
            SC_TRY(dependency.assign(line));
            SC_TRY(normalizeDependencyPath(projectRoot, dependency));
            SC_TRY(dependencies.push_back(move(dependency)));
        }
        return Result(true);
    }

    static Result filterWindowsDependencyOutput(const ResolvedProject& resolvedProject, StringView outputView,
                                                Vector<String>& dependencies, String& filteredOutput)
    {
        const StringView includePrefix = "Note: including file:"_a8;
        const char*      bytes         = outputView.bytesWithoutTerminator();
        size_t           lineStart     = 0;
        for (size_t idx = 0; idx <= outputView.sizeInBytes(); ++idx)
        {
            if (idx < outputView.sizeInBytes() and bytes[idx] != '\n')
            {
                continue;
            }

            StringView line = outputView.sliceStartEnd(lineStart, idx).trimEndAnyOf({'\r'});
            lineStart       = idx + 1;

            StringView includePath;
            if (line.trimStartAnyOf({' ', '\t'}).splitAfter(includePrefix, includePath))
            {
                includePath = includePath.trimWhiteSpaces();
                if (not includePath.isEmpty())
                {
                    String dependency = StringEncoding::Utf8;
                    SC_TRY(dependency.assign(includePath));
                    SC_TRY(normalizeDependencyPath(resolvedProject.project->rootDirectory.view(), dependency));
                    SC_TRY(dependencies.push_back(move(dependency)));
                }
                continue;
            }

            if (line.trimWhiteSpaces().isEmpty())
            {
                continue;
            }

            if (not line.isEmpty())
            {
                SC_TRY(StringBuilder::createForAppendingTo(filteredOutput).append(line));
            }
            SC_TRY(StringBuilder::createForAppendingTo(filteredOutput).append("\n"));
        }
        return Result(true);
    }

    static Result writeWindowsDependencyFile(FileSystem& fs, const ResolvedProject& resolvedProject,
                                             const ResolvedSource& source, String& stdOut, String& stdErr)
    {
        Vector<String> dependencies;

        String sourceDependency = StringEncoding::Utf8;
        SC_TRY(sourceDependency.assign(source.sourcePath.view()));
        SC_TRY(dependencies.push_back(move(sourceDependency)));

        String filteredStdOut = StringEncoding::Utf8;
        String filteredStdErr = StringEncoding::Utf8;
        SC_TRY(filterWindowsDependencyOutput(resolvedProject, stdOut.view(), dependencies, filteredStdOut));
        SC_TRY(filterWindowsDependencyOutput(resolvedProject, stdErr.view(), dependencies, filteredStdErr));

        String dependencyContents = StringEncoding::Utf8;
        for (const String& dependency : dependencies)
        {
            SC_TRY(StringBuilder::createForAppendingTo(dependencyContents).append(dependency.view()));
            SC_TRY(StringBuilder::createForAppendingTo(dependencyContents).append("\n"));
        }
        SC_TRY(makeParentDirectory(fs, source.dependencyPath.view()));
        SC_TRY(fs.writeString(source.dependencyPath.view(), dependencyContents.view()));
        stdOut = move(filteredStdOut);
        stdErr = move(filteredStdErr);
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
        SC_COMPILER_UNUSED(architecture);

        adapter.family = toolchain.family;
        if (adapter.family == Toolchain::HostDefault)
        {
            if (platform == Platform::Windows)
            {
                adapter.family = Toolchain::MSVC;
            }
#if SC_PLATFORM_WINDOWS
            else
            {
                adapter.family = Toolchain::MSVC;
            }
#elif SC_PLATFORM_APPLE
            else
            {
                adapter.family = Toolchain::Clang;
            }
#else
            else if (probeExecutable("clang++"))
            {
                adapter.family = Toolchain::Clang;
            }
            else
            {
                adapter.family = Toolchain::GCC;
            }
#endif
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
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), "cl", adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), adapter.executableC.view(), adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), "link", adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), "lib", adapter.executableArchive));
            SC_TRY(adapter.displayName.assign("msvc"));
            break;
        case Toolchain::ClangCL:
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), "clang-cl", adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), adapter.executableC.view(), adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), "lib", adapter.executableArchive));
            SC_TRY(adapter.displayName.assign("clang-cl"));
            break;
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

    static Result computeArtifactName(Platform::Type platform, const Project& project, String& output)
    {
        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable:
        case TargetType::GUIApplication:
            if (platform == Platform::Windows)
            {
                SC_TRY(StringBuilder::format(output, "{}.exe", project.targetName.view()));
            }
            else
            {
                SC_TRY(output.assign(project.targetName.view()));
            }
            return Result(true);
        case TargetType::SharedLibrary:
            if (platform == Platform::Windows)
            {
                SC_TRY(StringBuilder::format(output, "{}.dll", project.targetName.view()));
            }
            else if (platform == Platform::Apple)
            {
                SC_TRY(StringBuilder::format(output, "{}.dylib", project.targetName.view()));
            }
            else
            {
                SC_TRY(StringBuilder::format(output, "{}.so", project.targetName.view()));
            }
            return Result(true);
        case TargetType::StaticLibrary:
            if (platform == Platform::Windows)
            {
                SC_TRY(StringBuilder::format(output, "{}.lib", project.targetName.view()));
                return Result(true);
            }
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

    static size_t computeMaxParallelCompileJobs(const ResolvedProject& resolvedProject)
    {
        size_t maxParallelJobs = resolvedProject.parameters->execution.maxParallelJobs;
        if (maxParallelJobs == 0)
        {
            maxParallelJobs = Process::getNumberOfProcessors();
        }
        if (maxParallelJobs == 0)
        {
            maxParallelJobs = 1;
        }
        if (maxParallelJobs > MaxParallelCompileJobs)
        {
            maxParallelJobs = MaxParallelCompileJobs;
        }
        return maxParallelJobs;
    }

    static constexpr StringView describeRebuildDecision(RebuildDecision decision)
    {
        switch (decision)
        {
        case UpToDate: return "up to date"_a8;
        case MissingOutput: return "output missing"_a8;
        case MissingCommandFile: return "command fingerprint missing"_a8;
        case MissingDependencyFile: return "dependency file missing"_a8;
        case CommandChanged: return "command line changed"_a8;
        case OutputStatUnavailable: return "output metadata unavailable"_a8;
        case DependencyScanFailed: return "dependency scan unavailable"_a8;
        case InputChanged: return "input changed"_a8;
        case PreviousBuildStepRan: return "previous step rebuilt inputs"_a8;
        }
        Assert::unreachable();
    }

    static constexpr StringView getFinalStepName(const Project& project)
    {
        switch (project.targetType)
        {
        case TargetType::ConsoleExecutable:
        case TargetType::GUIApplication: return "LINK"_a8;
        case TargetType::SharedLibrary: return "LINK"_a8;
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

    static void printProjectTrace(const ResolvedProject& resolvedProject)
    {
        globalConsole->print("[trace] project {} toolchain={} c={} cpp={} link={} ar={}\n",
                             resolvedProject.project->targetName.view(), resolvedProject.adapter.displayName.view(),
                             resolvedProject.adapter.executableC.view(), resolvedProject.adapter.executableCpp.view(),
                             resolvedProject.adapter.executableLink.view(),
                             resolvedProject.adapter.executableArchive.view());
        if (not resolvedProject.parameters->toolchain.targetTriple.isEmpty() or
            not resolvedProject.parameters->toolchain.sysroot.isEmpty())
        {
            globalConsole->print("[trace] target triple={} sysroot={}\n",
                                 resolvedProject.parameters->toolchain.targetTriple.view(),
                                 resolvedProject.parameters->toolchain.sysroot.view());
        }
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
        if (process.exec({executable, "--version"}, output) and process.getExitStatus() == 0)
        {
            return true;
        }
        return process.exec({executable}, output) and process.getExitStatus() == 0;
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
