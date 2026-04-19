// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Libraries/Async/Async.h"
#include "../../Libraries/Async/Internal/IntrusiveDoubleLinkedList.inl"
#include "BuildWriter.h"

#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringBuilder.h"
#include "../../Libraries/Time/Time.h"

#include "BuildNativeOutput.inl"

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

        [[nodiscard]] bool isClangLike() const { return family == Toolchain::Clang or family == Toolchain::LLVMMingw; }
        [[nodiscard]] bool isMSVCStyle() const { return family == Toolchain::MSVC or family == Toolchain::ClangCL; }
        [[nodiscard]] bool isClangCL() const { return family == Toolchain::ClangCL; }
    };

    struct ResolvedTargetContext
    {
        Machine buildMachine;
        Machine hostMachine;
        Machine targetMachine;
    };

    struct ResolvedRunner
    {
        enum Mode
        {
            Direct,
            Wrapped,
        };

        Mode mode = Direct;

        String         executable;
        Vector<String> arguments;
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

        Variables             variables;
        ResolvedTargetContext targetContext;
        CompilerAdapter       adapter;
        CompileFlags          compileFlags;
        LinkFlags             linkFlags;

        String targetDirectory;
        String intermediateDirectory;
        String executablePath;
        String linkCommandPath;
        String linkResponsePath;
        String compileCommandsPath;
        String workspaceCompileCommandsPath;
        String exportedSymbolsPath;
        String exportedSymbolsLinkerPath;
        String resolvedSysroot;

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
            NativeJobRecord        job;
            String                 stdOutPath = StringEncoding::Utf8;
            String                 stdErrPath = StringEncoding::Utf8;
            PipeDescriptor         stdOutPipe;
            PipeDescriptor         stdErrPipe;
            AsyncFileRead          asyncStdOut;
            AsyncFileRead          asyncStdErr;
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
        NativeBuildReporter*            reporter      = nullptr;
        IntrusiveDoubleLinkedList<Slot> availableSlots;
        Slot                            slots[MaxParallelCompileJobs];

        Result create(FileSystem& fs, NativeBuildReporter& buildReporter, size_t maxProcesses)
        {
            fileSystem    = &fs;
            reporter      = &buildReporter;
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
                slotResult = readFileIntoString(slot.stdOutPath.view(), slot.job.stdOut);
                if (slotResult)
                {
                    slotResult = readFileIntoString(slot.stdErrPath.view(), slot.job.stdErr);
                }
            }
            if (slotResult)
            {
                slot.job.exitStatus = slot.exitStatus;
                slotResult = finalizeCompileJob(*fileSystem, *slot.project, *slot.source, slot.job.command.view(),
                                                *slot.anyObjectBuilt, slot.job, *reporter);
            }
            if (not slotResult and processResult)
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
            SC_TRY(slot.stdOutPath.assign({}));
            SC_TRY(slot.stdErrPath.assign({}));
            SC_TRY(slot.job.clear());
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
                      CommandLine& commandLine, String& commandString, bool& anyObjectBuilt, bool& launched)
        {
            launched = false;
            while (availableSlots.isEmpty())
            {
                SC_TRY(eventLoop.runOnce());
            }
            if (not processResult)
            {
                return processResult;
            }
            if (reporter->shouldStopScheduling())
            {
                return Result(true);
            }

            Slot& slot          = *availableSlots.dequeueFront();
            slot.project        = &project;
            slot.source         = &source;
            slot.anyObjectBuilt = &anyObjectBuilt;
            SC_TRY(slot.job.clear());
            slot.job.step       = compileStep;
            slot.job.totalSteps = totalSteps;
            slot.job.kind       = NativeJobKind::Compile;
            slot.job.status     = NativeJobStatus::Succeeded;
            SC_TRY(slot.job.stepName.assign(getCompileStepName(source.type)));
            SC_TRY(slot.job.label.assign(source.displayPath.view()));
            SC_TRY(slot.job.command.assign(commandString.view()));
            slot.exitStatus      = -1;
            slot.processFinished = false;
            slot.stdOutFinished  = false;
            slot.stdErrFinished  = false;

            SC_TRY(makeParentDirectory(*fileSystem, source.objectPath.view()));
            SC_TRY(reporter->printStepStarted(slot.job));

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
                        callbackResult = appendChunk(slot.job.stdOut, data);
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
                        callbackResult = appendChunk(slot.job.stdErr, data);
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
            launched = true;
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
                                           const ProjectProgress& progress, NativeBuildReporter& reporter,
                                           bool& compilePhaseCompleted, size_t maxParallelJobsOverride = 0)
    {
        if (reporter.isVerbose())
        {
            printProjectTrace(resolvedProject);
        }
        return buildCompileSteps(fs, resolvedProject, anyObjectBuilt, progress, reporter, compilePhaseCompleted,
                                 maxParallelJobsOverride);
    }

    static bool shouldStripLinkedArtifact(const ResolvedProject& resolvedProject)
    {
        if (resolvedProject.project->targetType == TargetType::StaticLibrary)
        {
            return false;
        }
        if (not resolvedProject.linkFlags.enableDeadCodeStripping)
        {
            return false;
        }

        switch (targetPlatform(resolvedProject.targetContext))
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
        switch (targetPlatform(resolvedProject.targetContext))
        {
        case Platform::Apple:
            SC_TRY(commandLine.append("strip"));
            SC_TRY(commandLine.append("-x"));
            break;
        case Platform::Linux:
            if (WriterInternal::shouldPreserveExportedSymbols(*resolvedProject.project, resolvedProject.linkFlags))
            {
                SC_TRY(commandLine.append("objcopy"));
                SC_TRY(commandLine.append("--strip-unneeded"));
                String option = StringEncoding::Utf8;
                SC_TRY(StringBuilder::format(option, "--keep-symbols={}", resolvedProject.exportedSymbolsPath.view()));
                SC_TRY(commandLine.append(option.view()));
            }
            else
            {
                SC_TRY(commandLine.append("strip"));
                SC_TRY(commandLine.append("--strip-unneeded"));
            }
            break;
        case Platform::Unknown:
        case Platform::Windows:
        case Platform::Wasm: return Result::Error("Strip command is unsupported on this platform");
        }
        SC_TRY(commandLine.append(resolvedProject.executablePath.view()));
        return Result(true);
    }

    static Result appendUniqueSymbolLines(StringView text, Vector<String>& symbols)
    {
        StringViewTokenizer tokenizer(text);
        while (tokenizer.tokenizeNextLine())
        {
            const StringView line = tokenizer.component.trimWhiteSpaces();
            if (not line.isEmpty())
            {
                String symbol = StringEncoding::Utf8;
                SC_TRY(symbol.assign(line));
                SC_TRY(symbols.push_back(move(symbol)));
            }
        }
        return Result(true);
    }

    static Result writeExportedSymbolsFiles(FileSystem& fs, const ResolvedProject& resolvedProject, String& signature)
    {
        Vector<String> symbols;
        for (const ResolvedSource& source : resolvedProject.sources)
        {
            if (not WriterInternal::isRenderItemInExportSet(*resolvedProject.project, source.displayPath.view()))
            {
                continue;
            }
            if (not fs.existsAndIsFile(source.objectPath.view()))
            {
                continue;
            }

            CommandLine nmCommand;
            SC_TRY(nmCommand.append("nm"));
            switch (targetPlatform(resolvedProject.targetContext))
            {
            case Platform::Apple: SC_TRY(nmCommand.append("-gjU")); break;
            case Platform::Linux:
                SC_TRY(nmCommand.append("-g"));
                SC_TRY(nmCommand.append("--defined-only"));
                SC_TRY(nmCommand.append("-j"));
                break;
            case Platform::Unknown:
            case Platform::Windows:
            case Platform::Wasm: return Result::Error("Exported symbols preservation is unsupported on this platform");
            }
            SC_TRY(nmCommand.append(source.objectPath.view()));

            NativeJobRecord nmJob;
            SC_TRY(initializeJobRecord(0, 0, NativeJobKind::Link, "NM"_a8, source.displayPath.view(), StringView(),
                                       StringView(), nmJob));
            SC_TRY(runCapturedCommand(fs, nmCommand, StringView(), resolvedProject.adapter, nmJob));
            SC_TRY_MSG(nmJob.status == NativeJobStatus::Succeeded, "Native backend command failed");
            SC_TRY(appendUniqueSymbolLines(nmJob.stdOut.view(), symbols));
        }

        Algorithms::bubbleSort(symbols.begin(), symbols.end(), [](const String& left, const String& right)
                               { return left.view().compare(right.view()) == StringView::Comparison::Smaller; });

        String exportedSymbols = StringEncoding::Utf8;
        {
            auto   builder       = StringBuilder::create(exportedSymbols);
            size_t uniqueSymbols = 0;
            for (size_t index = 0; index < symbols.size(); ++index)
            {
                if (index > 0 and symbols[index - 1].view() == symbols[index].view())
                {
                    continue;
                }
                if (uniqueSymbols > 0)
                {
                    SC_TRY(builder.append("\n"));
                }
                SC_TRY(builder.append(symbols[index].view()));
                uniqueSymbols++;
            }
            builder.finalize();
        }

        SC_TRY(makeParentDirectory(fs, resolvedProject.exportedSymbolsPath.view()));
        SC_TRY(fs.writeString(resolvedProject.exportedSymbolsPath.view(), exportedSymbols.view()));

        String linkerSymbols = StringEncoding::Utf8;
        if (targetPlatform(resolvedProject.targetContext) == Platform::Linux)
        {
            auto builder = StringBuilder::create(linkerSymbols);
            SC_TRY(builder.append("{\n"));
            StringViewTokenizer tokenizer(exportedSymbols.view());
            while (tokenizer.tokenizeNextLine())
            {
                const StringView line = tokenizer.component.trimWhiteSpaces();
                if (not line.isEmpty())
                {
                    SC_TRY(builder.append("  "));
                    SC_TRY(builder.append(line));
                    SC_TRY(builder.append(";\n"));
                }
            }
            SC_TRY(builder.append("};\n"));
            builder.finalize();
            SC_TRY(fs.writeString(resolvedProject.exportedSymbolsLinkerPath.view(), linkerSymbols.view()));
        }

        SC_TRY(signature.assign(exportedSymbols.view()));
        if (targetPlatform(resolvedProject.targetContext) == Platform::Linux)
        {
            auto builder = StringBuilder::createForAppendingTo(signature);
            SC_TRY(builder.append("\n# dynamic-list\n"));
            SC_TRY(builder.append(linkerSymbols.view()));
            builder.finalize();
        }
        return Result(true);
    }

    static Result initializeJobRecord(size_t step, size_t totalSteps, NativeJobKind::Type kind, StringView stepName,
                                      StringView label, StringView command, StringView rebuildReason,
                                      NativeJobRecord& job)
    {
        SC_TRY(job.clear());
        job.step       = step;
        job.totalSteps = totalSteps;
        job.kind       = kind;
        job.status     = NativeJobStatus::Succeeded;
        SC_TRY(job.stepName.assign(stepName));
        SC_TRY(job.label.assign(label));
        SC_TRY(job.command.assign(command));
        SC_TRY(job.rebuildReason.assign(rebuildReason));
        return Result(true);
    }

    static Result runCapturedCommand(FileSystem& fs, CommandLine& commandLine, StringView responsePath,
                                     const CompilerAdapter& adapter, NativeJobRecord& job)
    {
        SC_TRY(maybeWriteResponseFile(fs, commandLine, responsePath, adapter));

        StringSpan             views[128];
        Span<const StringSpan> args;
        SC_TRY(commandLine.toViews(views, args));

        Process process;
        SC_TRY(process.exec(args, job.stdOut, Process::StdIn(), job.stdErr));
        job.exitStatus = process.getExitStatus();
        job.status     = job.exitStatus == 0 ? NativeJobStatus::Succeeded : NativeJobStatus::Failed;
        return Result(true);
    }

    static Result finalizeCompileJob(FileSystem& fs, const ResolvedProject& resolvedProject,
                                     const ResolvedSource& source, StringView commandString, bool& anyObjectBuilt,
                                     NativeJobRecord& job, NativeBuildReporter& reporter)
    {
        if (resolvedProject.parameters->execution.useCompilerDependencies and resolvedProject.adapter.isMSVCStyle())
        {
            SC_TRY(writeWindowsDependencyFile(fs, resolvedProject, source, job.stdOut, job.stdErr));
        }

        if (job.status == NativeJobStatus::Succeeded)
        {
            SC_TRY(fs.writeString(source.commandPath.view(), commandString));
            anyObjectBuilt = true;
        }
        return reporter.recordCompleted(job);
    }

    static size_t countPendingProjectSteps(const ResolvedProject& resolvedProject, size_t nextSourceIndex,
                                           bool includeFinalStep = true)
    {
        size_t count = 0;
        if (nextSourceIndex < resolvedProject.sources.size())
        {
            count += resolvedProject.sources.size() - nextSourceIndex;
        }
        if (includeFinalStep)
        {
            count += 1;
        }
        return count;
    }

    static Result buildProjectFinalPhase(FileSystem& fs, ResolvedProject& resolvedProject, bool anyObjectBuilt,
                                         const ProjectProgress& progress, NativeBuildReporter& reporter,
                                         bool& finalPhaseCompleted)
    {
        finalPhaseCompleted             = false;
        String exportedSymbolsSignature = StringEncoding::Utf8;
        if (WriterInternal::shouldPreserveExportedSymbols(*resolvedProject.project, resolvedProject.linkFlags) and
            (targetPlatform(resolvedProject.targetContext) == Platform::Apple or
             targetPlatform(resolvedProject.targetContext) == Platform::Linux))
        {
            SC_TRY(writeExportedSymbolsFiles(fs, resolvedProject, exportedSymbolsSignature));
        }

        CommandLine finalCommand;
        String      finalCommandString = StringEncoding::Utf8;
        SC_TRY(buildFinalCommand(resolvedProject, finalCommand));
        SC_TRY(formatCommandLine(finalCommand, finalCommandString));
        if (not exportedSymbolsSignature.isEmpty())
        {
            auto builder = StringBuilder::createForAppendingTo(finalCommandString);
            SC_TRY(builder.append("\n# exported symbols\n"));
            SC_TRY(builder.append(exportedSymbolsSignature.view()));
            builder.finalize();
        }
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
            NativeJobRecord finalJob;
            SC_TRY(initializeJobRecord(
                progress.finalStep, progress.totalSteps,
                resolvedProject.project->targetType == TargetType::StaticLibrary ? NativeJobKind::Archive
                                                                                 : NativeJobKind::Link,
                getFinalStepName(*resolvedProject.project), resolvedProject.project->targetName.view(),
                finalCommandString.view(), describeRebuildDecision(finalDecision), finalJob));
            SC_TRY(reporter.printRebuildTrace(finalJob.stepName.view(), finalJob.label.view(),
                                              finalJob.rebuildReason.view()));
            SC_TRY(reporter.printStepStarted(finalJob));
            SC_TRY(makeParentDirectory(fs, resolvedProject.executablePath.view()));
            SC_TRY(runCapturedCommand(fs, finalCommand, finalResponsePath(resolvedProject), resolvedProject.adapter,
                                      finalJob));
            SC_TRY(reporter.recordCompleted(finalJob));
            if (reporter.shouldStopScheduling())
            {
                return Result(true);
            }
            if (shouldStrip)
            {
                NativeJobRecord stripJob;
                SC_TRY(initializeJobRecord(progress.finalStep, progress.totalSteps, NativeJobKind::Strip, "STRIP"_a8,
                                           resolvedProject.project->targetName.view(), stripCommandString.view(),
                                           "post-link strip"_a8, stripJob));
                SC_TRY(reporter.printStepStarted(stripJob));
                SC_TRY(runCapturedCommand(fs, stripCommand, StringView(), resolvedProject.adapter, stripJob));
                SC_TRY(reporter.recordCompleted(stripJob));
                if (reporter.shouldStopScheduling())
                {
                    return Result(true);
                }
            }
            SC_TRY(fs.writeString(resolvedProject.linkCommandPath.view(), finalCommandString.view()));
        }
        else
        {
            SC_TRY(reporter.recordSkipped(
                progress.finalStep, progress.totalSteps, getFinalStepName(*resolvedProject.project),
                resolvedProject.project->targetName.view(), describeRebuildDecision(finalDecision)));
            if (reporter.isVerbose())
            {
                globalConsole->print("[trace] no work to do for {}\n", resolvedProject.project->targetName.view());
            }
        }

        finalPhaseCompleted = not reporter.shouldStopScheduling();
        return Result(true);
    }

    static Result buildResolvedProjectWave(const Vector<ResolvedProject*>& waveProjects,
                                           const Vector<ResolvedProject>&  resolvedProjects,
                                           const Vector<ProjectProgress>&  progressByProject,
                                           size_t workspaceParallelJobs, NativeBuildReporter& reporter)
    {
        if (waveProjects.isEmpty())
        {
            return Result(true);
        }
        if (workspaceParallelJobs <= 1)
        {
            FileSystem fs;
            SC_TRY(fs.init("."));
            for (size_t waveIndex = 0; waveIndex < waveProjects.size(); ++waveIndex)
            {
                size_t projectIndex = 0;
                SC_TRY(findResolvedProjectIndex(resolvedProjects, *waveProjects[waveIndex]->project, projectIndex));
                SC_TRY(buildProject(fs, *waveProjects[waveIndex], progressByProject[projectIndex], reporter,
                                    workspaceParallelJobs));
                if (reporter.shouldStopScheduling())
                {
                    size_t cancelled = 0;
                    for (size_t idx = waveIndex + 1; idx < waveProjects.size(); ++idx)
                    {
                        cancelled += countPendingProjectSteps(*waveProjects[idx], 0);
                    }
                    reporter.recordCancelled(cancelled);
                    return Result(true);
                }
            }
            return Result(true);
        }

        FileSystem fs;
        SC_TRY(fs.init("."));

        Vector<bool> anyObjectBuiltFlags;
        for (size_t idx = 0; idx < waveProjects.size(); ++idx)
        {
            SC_TRY(anyObjectBuiltFlags.push_back(false));
        }

        ParallelCompileLimiter limiter;
        SC_TRY(limiter.create(fs, reporter, workspaceParallelJobs));

        auto countRemainingWaveSteps = [&](size_t projectIndex, size_t sourceIndex) -> size_t
        {
            size_t cancelled = 0;
            for (size_t idx = projectIndex; idx < waveProjects.size(); ++idx)
            {
                const ResolvedProject& pendingProject = *waveProjects[idx];
                const size_t           nextSource     = idx == projectIndex ? sourceIndex : 0;
                cancelled += countPendingProjectSteps(pendingProject, nextSource);
            }
            return cancelled;
        };

        bool countedWaveCancellation = false;

        for (size_t projectIndex = 0; projectIndex < waveProjects.size(); ++projectIndex)
        {
            ResolvedProject& resolvedProject      = *waveProjects[projectIndex];
            size_t           resolvedProjectIndex = 0;
            SC_TRY(findResolvedProjectIndex(resolvedProjects, *resolvedProject.project, resolvedProjectIndex));
            const ProjectProgress& progress = progressByProject[resolvedProjectIndex];
            if (reporter.isVerbose())
            {
                printProjectTrace(resolvedProject);
                globalConsole->print("[trace] workspace compile wave jobs = {}\n", workspaceParallelJobs);
            }

            size_t compileStep = 0;
            for (ResolvedSource& source : resolvedProject.sources)
            {
                if (reporter.shouldStopScheduling())
                {
                    reporter.recordCancelled(countRemainingWaveSteps(projectIndex, compileStep));
                    countedWaveCancellation = true;
                    break;
                }
                compileStep++;

                CommandLine commandLine;
                String      commandString = StringEncoding::Utf8;
                SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
                SC_TRY(formatCommandLine(commandLine, commandString));
                const RebuildDecision compileDecision =
                    evaluateObjectRebuild(fs, resolvedProject, source, commandString.view());
                if (compileDecision == UpToDate)
                {
                    SC_TRY(reporter.recordSkipped(progress.compileStartStep + compileStep - 1, progress.totalSteps,
                                                  getCompileStepName(source.type), source.displayPath.view(),
                                                  describeRebuildDecision(compileDecision)));
                    continue;
                }
                SC_TRY(reporter.printRebuildTrace(getCompileStepName(source.type), source.displayPath.view(),
                                                  describeRebuildDecision(compileDecision)));

                bool launched = false;
                SC_TRY(limiter.launch(progress.compileStartStep + compileStep - 1, progress.totalSteps, resolvedProject,
                                      source, commandLine, commandString, anyObjectBuiltFlags[projectIndex], launched));
                if (not launched)
                {
                    reporter.recordCancelled(countRemainingWaveSteps(projectIndex, compileStep - 1));
                    countedWaveCancellation = true;
                    break;
                }
            }

            if (reporter.shouldStopScheduling())
            {
                break;
            }
        }
        SC_TRY(limiter.close());
        if (reporter.shouldStopScheduling())
        {
            if (not countedWaveCancellation)
            {
                reporter.recordCancelled(waveProjects.size());
            }
            return Result(true);
        }

        for (size_t projectIndex = 0; projectIndex < waveProjects.size(); ++projectIndex)
        {
            size_t resolvedProjectIndex = 0;
            SC_TRY(
                findResolvedProjectIndex(resolvedProjects, *waveProjects[projectIndex]->project, resolvedProjectIndex));
            bool finalPhaseCompleted = false;
            SC_TRY(buildProjectFinalPhase(fs, *waveProjects[projectIndex], anyObjectBuiltFlags[projectIndex],
                                          progressByProject[resolvedProjectIndex], reporter, finalPhaseCompleted));
            if (not finalPhaseCompleted)
            {
                reporter.recordCancelled(waveProjects.size() - projectIndex - 1);
                return Result(true);
            }
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
        NativeBuildReporter     reporter;
        if (resolvedProjects.size() == 1)
        {
            FileSystem fs;
            SC_TRY(fs.init("."));
            Vector<size_t> projectLevels;
            SC_TRY(projectLevels.push_back(0));
            SC_TRY(computeResolvedProjectProgress(resolvedProjects, projectLevels, progressByProject));
            SC_TRY(reporter.begin(parameters.execution, progressByProject[0].totalSteps));
            SC_TRY(buildProject(fs, resolvedProjects[0], progressByProject[0], reporter, workspaceParallelJobs));
            SC_TRY(reporter.flushDeferredFailures());
            SC_TRY(reporter.printFinalSummary());
            if (reporter.shouldStopScheduling())
            {
                return Result::Error("Native backend command failed");
            }
            return Result(true);
        }

        Vector<size_t> projectLevels;
        SC_TRY(computeResolvedProjectLevels(resolvedProjects, projectLevels));
        SC_TRY(computeResolvedProjectProgress(resolvedProjects, projectLevels, progressByProject));
        SC_TRY(reporter.begin(parameters.execution, progressByProject[0].totalSteps));

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
            SC_TRY(buildResolvedProjectWave(waveProjects, resolvedProjects, progressByProject, workspaceParallelJobs,
                                            reporter));
            if (reporter.shouldStopScheduling())
            {
                size_t cancelled = 0;
                for (size_t idx = 0; idx < resolvedProjects.size(); ++idx)
                {
                    if (projectLevels[idx] > level)
                    {
                        cancelled += countPendingProjectSteps(resolvedProjects[idx], 0);
                    }
                }
                reporter.recordCancelled(cancelled);
                break;
            }
        }
        SC_TRY(reporter.flushDeferredFailures());
        SC_TRY(reporter.printFinalSummary());
        if (reporter.shouldStopScheduling())
        {
            return Result::Error("Native backend command failed");
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
            SC_TRY(runExecutable(resolvedProjects[0].executablePath.view(), action,
                                 resolvedProjects[0].project->targetType));
        }
        return Result(true);
    }

    static Result buildProject(FileSystem& fs, ResolvedProject& resolvedProject, const ProjectProgress& progress,
                               NativeBuildReporter& reporter, size_t maxParallelJobsOverride = 0)
    {
        bool anyObjectBuilt        = false;
        bool compilePhaseCompleted = false;
        SC_TRY(buildProjectCompilePhase(fs, resolvedProject, anyObjectBuilt, progress, reporter, compilePhaseCompleted,
                                        maxParallelJobsOverride));
        if (not compilePhaseCompleted)
        {
            return Result(true);
        }

        bool finalPhaseCompleted = false;
        SC_TRY(buildProjectFinalPhase(fs, resolvedProject, anyObjectBuilt, progress, reporter, finalPhaseCompleted));
        return Result(true);
    }

    static Result buildCompileSteps(FileSystem& fs, ResolvedProject& resolvedProject, bool& anyObjectBuilt,
                                    const ProjectProgress& progress, NativeBuildReporter& reporter,
                                    bool& compilePhaseCompleted, size_t maxParallelJobsOverride = 0)
    {
        size_t maxParallelJobs = computeMaxParallelCompileJobs(resolvedProject);
        if (maxParallelJobsOverride != 0 and maxParallelJobsOverride < maxParallelJobs)
        {
            maxParallelJobs = maxParallelJobsOverride;
        }
        if (maxParallelJobs <= 1 or resolvedProject.sources.size() <= 1)
        {
            return buildCompileStepsSequential(fs, resolvedProject, anyObjectBuilt, progress, reporter,
                                               compilePhaseCompleted);
        }
        return buildCompileStepsParallel(fs, resolvedProject, anyObjectBuilt, progress, reporter, compilePhaseCompleted,
                                         maxParallelJobs);
    }

    static Result buildCompileStepsSequential(FileSystem& fs, ResolvedProject& resolvedProject, bool& anyObjectBuilt,
                                              const ProjectProgress& progress, NativeBuildReporter& reporter,
                                              bool& compilePhaseCompleted)
    {
        compilePhaseCompleted = true;
        size_t compileStep    = 0;
        for (ResolvedSource& source : resolvedProject.sources)
        {
            if (reporter.shouldStopScheduling())
            {
                reporter.recordCancelled(countPendingProjectSteps(resolvedProject, compileStep));
                compilePhaseCompleted = false;
                return Result(true);
            }
            compileStep++;

            CommandLine commandLine;
            String      commandString = StringEncoding::Utf8;
            SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
            SC_TRY(formatCommandLine(commandLine, commandString));
            const RebuildDecision compileDecision =
                evaluateObjectRebuild(fs, resolvedProject, source, commandString.view());
            if (compileDecision == UpToDate)
            {
                SC_TRY(reporter.recordSkipped(progress.compileStartStep + compileStep - 1, progress.totalSteps,
                                              getCompileStepName(source.type), source.displayPath.view(),
                                              describeRebuildDecision(compileDecision)));
                continue;
            }
            SC_TRY(reporter.printRebuildTrace(getCompileStepName(source.type), source.displayPath.view(),
                                              describeRebuildDecision(compileDecision)));

            SC_TRY(makeParentDirectory(fs, source.objectPath.view()));
            NativeJobRecord job;
            SC_TRY(initializeJobRecord(progress.compileStartStep + compileStep - 1, progress.totalSteps,
                                       NativeJobKind::Compile, getCompileStepName(source.type),
                                       source.displayPath.view(), commandString.view(),
                                       describeRebuildDecision(compileDecision), job));
            SC_TRY(reporter.printStepStarted(job));
            SC_TRY(runCapturedCommand(fs, commandLine, source.responsePath.view(), resolvedProject.adapter, job));
            SC_TRY(
                finalizeCompileJob(fs, resolvedProject, source, commandString.view(), anyObjectBuilt, job, reporter));
            if (reporter.shouldStopScheduling())
            {
                reporter.recordCancelled(countPendingProjectSteps(resolvedProject, compileStep));
                compilePhaseCompleted = false;
                return Result(true);
            }
        }
        return Result(true);
    }

    static Result buildCompileStepsParallel(FileSystem& fs, ResolvedProject& resolvedProject, bool& anyObjectBuilt,
                                            const ProjectProgress& progress, NativeBuildReporter& reporter,
                                            bool& compilePhaseCompleted, size_t maxParallelJobs)
    {
        compilePhaseCompleted = true;
        if (reporter.isVerbose())
        {
            globalConsole->print("[trace] parallel compile jobs = {}\n", maxParallelJobs);
        }

        ParallelCompileLimiter limiter;
        SC_TRY(limiter.create(fs, reporter, maxParallelJobs));

        size_t compileStep                = 0;
        bool   countedProjectCancellation = false;
        for (ResolvedSource& source : resolvedProject.sources)
        {
            if (reporter.shouldStopScheduling())
            {
                reporter.recordCancelled(countPendingProjectSteps(resolvedProject, compileStep));
                countedProjectCancellation = true;
                compilePhaseCompleted      = false;
                break;
            }
            compileStep++;

            CommandLine commandLine;
            String      commandString = StringEncoding::Utf8;
            SC_TRY(buildCompileCommand(resolvedProject, source, commandLine));
            SC_TRY(formatCommandLine(commandLine, commandString));
            const RebuildDecision compileDecision =
                evaluateObjectRebuild(fs, resolvedProject, source, commandString.view());
            if (compileDecision == UpToDate)
            {
                SC_TRY(reporter.recordSkipped(progress.compileStartStep + compileStep - 1, progress.totalSteps,
                                              getCompileStepName(source.type), source.displayPath.view(),
                                              describeRebuildDecision(compileDecision)));
                continue;
            }
            SC_TRY(reporter.printRebuildTrace(getCompileStepName(source.type), source.displayPath.view(),
                                              describeRebuildDecision(compileDecision)));

            bool launched = false;
            SC_TRY(limiter.launch(progress.compileStartStep + compileStep - 1, progress.totalSteps, resolvedProject,
                                  source, commandLine, commandString, anyObjectBuilt, launched));
            if (not launched)
            {
                reporter.recordCancelled(countPendingProjectSteps(resolvedProject, compileStep - 1));
                countedProjectCancellation = true;
                compilePhaseCompleted      = false;
                break;
            }
        }
        SC_TRY(limiter.close());
        if (reporter.shouldStopScheduling())
        {
            compilePhaseCompleted = false;
            if (not countedProjectCancellation)
            {
                reporter.recordCancelled(1);
            }
        }
        return Result(true);
    }

    static Result runExecutable(StringView executablePath, const Action& action, TargetType::Type targetType)
    {
        ResolvedTargetContext targetContext;
        SC_TRY(resolveTargetContext(action.parameters, targetContext));

        ResolvedRunner runner;
        SC_TRY(resolveRunner(action.parameters, targetContext, runner));

        Process    process;
        StringSpan arguments[96];
        size_t     numArguments     = 0;
        String     normalizedTarget = StringEncoding::Utf8;
        String     wineCommand      = StringEncoding::Utf8;
        SC_TRY(Path::normalize(normalizedTarget, StringView(executablePath).trimWhiteSpaces(), Path::AsNative));
        if (runner.mode == ResolvedRunner::Wrapped and StringView(runner.executable.view()).containsString("/wine"))
        {
            SC_TRY_MSG(bundledWineHasWindowsLoader(runner.executable.view(), targetArchitecture(targetContext)),
                       "Wine runner does not provide a Windows loader for the selected target architecture");
            String     prefixDirectory = StringEncoding::Utf8;
            StringView architectureName;
            switch (targetArchitecture(targetContext))
            {
            case Architecture::Intel64: architectureName = "x86_64"; break;
            case Architecture::Arm64: architectureName = "arm64"; break;
            case Architecture::Intel32: architectureName = "x86"; break;
            case Architecture::Wasm:
            case Architecture::Any: architectureName = "unknown"; break;
            }
            SC_TRY(StringBuilder::format(prefixDirectory, "{}/wine-prefix-{}",
                                         action.parameters.directories.buildCacheDirectory, architectureName));
            FileSystem fs;
            SC_TRY(fs.init("."));
            SC_TRY(fs.makeDirectoryRecursive(prefixDirectory.view()));
            SC_TRY(prepareWinePrefixHeadless(runner.executable.view(), prefixDirectory.view(), targetContext));
            SC_TRY(configureWineProcess(process, prefixDirectory.view(), targetContext));
            globalConsole->print("WINEPREFIX = {}\n", prefixDirectory.view());

            String workingDirectory = StringEncoding::Utf8;
            SC_TRY(workingDirectory.assign(Path::dirname(normalizedTarget.view(), Path::AsNative)));
            if (not workingDirectory.isEmpty())
            {
                SC_TRY(process.setWorkingDirectory(workingDirectory.view()));
            }

            if (targetType == TargetType::ConsoleExecutable and shouldPreferWineConsole(targetContext))
            {
                String wineConsole = StringEncoding::Utf8;
                SC_TRY(wineConsole.assign(Path::dirname(runner.executable.view(), Path::AsNative)));
                SC_TRY(Path::append(wineConsole, {"wineconsole"}, Path::AsNative));
                if (pathExists(wineConsole.view()))
                {
                    SC_TRY(runner.executable.assign(wineConsole.view()));
                    SC_TRY(runner.arguments.insert(0, {"--backend=curses"}));
                }
            }

            SC_TRY(convertPosixPathToWindowsPath(normalizedTarget.view(), wineCommand));
        }

        if (runner.mode == ResolvedRunner::Wrapped)
        {
            if (runner.executable.isEmpty())
            {
                SC_TRY_MSG(not runner.arguments.isEmpty(), "Missing wrapped runner command");
                arguments[numArguments] = runner.arguments[0].view();
                numArguments++;
                for (size_t idx = 1; idx < runner.arguments.size(); ++idx)
                {
                    if (numArguments == sizeof(arguments) / sizeof(arguments[0]))
                    {
                        globalConsole->printLine("Exceeded max number of arguments that can be passed to the runner");
                        break;
                    }
                    arguments[numArguments] = runner.arguments[idx].view();
                    globalConsole->print("RUNNER_ARGS[{}] = {}\n", idx - 1, arguments[numArguments]);
                    numArguments++;
                }
                globalConsole->print("RUNNER = {}\n", arguments[0]);
            }
            else
            {
                arguments[numArguments] = runner.executable.view();
                globalConsole->print("RUNNER = {}\n", arguments[numArguments]);
                numArguments++;
                for (size_t idx = 0; idx < runner.arguments.size(); ++idx)
                {
                    if (numArguments == sizeof(arguments) / sizeof(arguments[0]))
                    {
                        globalConsole->printLine("Exceeded max number of arguments that can be passed to the runner");
                        break;
                    }
                    arguments[numArguments] = runner.arguments[idx].view();
                    globalConsole->print("RUNNER_ARGS[{}] = {}\n", idx, arguments[numArguments]);
                    numArguments++;
                }
            }
        }

        if (not wineCommand.isEmpty())
        {
            arguments[numArguments] = "cmd";
            globalConsole->print("RUNNER_ARGS[{}] = {}\n", numArguments > 0 ? numArguments - 1 : 0,
                                 arguments[numArguments]);
            numArguments++;
            arguments[numArguments] = "/c";
            globalConsole->print("RUNNER_ARGS[{}] = {}\n", numArguments > 0 ? numArguments - 1 : 0,
                                 arguments[numArguments]);
            numArguments++;
            arguments[numArguments] = wineCommand.view();
            globalConsole->print("COMMAND = {}\n", normalizedTarget.view());
            globalConsole->print("COMMAND_WIN = {}\n", arguments[numArguments]);
            numArguments++;
        }
        else
        {
            arguments[numArguments] = normalizedTarget.view();
            globalConsole->print("COMMAND = {}\n", arguments[numArguments]);
            numArguments++;
        }
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

    static Result maybeWriteResponseFile(FileSystem& fs, CommandLine& commandLine, StringView responsePath,
                                         const CompilerAdapter& adapter)
    {
        if (responsePath.isEmpty())
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
        resolvedProject.parameters      = &parameters;
        resolvedProject.workspace       = &workspace;
        resolvedProject.project         = &project;
        resolvedProject.configuration   = &configuration;
        resolvedProject.resolvedSysroot = String(StringEncoding::Utf8);
        if (not parameters.toolchain.sysroot.isEmpty())
        {
            SC_TRY(resolvedProject.resolvedSysroot.assign(parameters.toolchain.sysroot.view()));
        }

        SC_TRY(resolveTargetContext(parameters, resolvedProject.targetContext));
        SC_TRY(resolveCompilerAdapter(parameters, resolvedProject.targetContext, resolvedProject.adapter));
        if (resolvedProject.resolvedSysroot.isEmpty() and shouldUsePackagedLinuxSysroot(resolvedProject.targetContext))
        {
            SC_TRY(resolvePackagedLinuxSysroot(parameters, resolvedProject.targetContext,
                                               resolvedProject.resolvedSysroot));
        }
        SC_TRY(fillVariables(parameters, project, configuration, resolvedProject.targetContext,
                             resolvedProject.adapter.displayName.view(), resolvedProject.variables));

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
        SC_TRY(computeArtifactName(targetPlatform(resolvedProject.targetContext), project, artifactName));
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
        SC_TRY(Path::join(resolvedProject.exportedSymbolsPath,
                          {resolvedProject.intermediateDirectory.view(), "exported_symbols.list"}));
        SC_TRY(Path::join(resolvedProject.exportedSymbolsLinkerPath,
                          {resolvedProject.intermediateDirectory.view(), "exported_symbols.ld"}));

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
            SC_TRY(
                StringBuilder::createForAppendingTo(objectRelative)
                    .append(targetPlatform(resolvedProject.targetContext) == Platform::Windows ? ".obj"_a8 : ".o"_a8));
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
        const bool supportsSanitizers =
            not(isLinuxTarget(resolvedProject.targetContext) and resolvedProject.adapter.family == Toolchain::Clang and
                resolvedProject.targetContext.hostMachine.platform != Platform::Linux);
        if ((resolvedProject.compileFlags.enableASAN or resolvedProject.linkFlags.enableASAN) and supportsSanitizers)
        {
            SC_TRY(commandLine.append("-fsanitize=address,undefined"));
        }
        if (isLinuxTarget(resolvedProject.targetContext) and resolvedProject.adapter.family == Toolchain::Clang and
            resolvedProject.targetContext.hostMachine.platform != Platform::Linux)
        {
            SC_TRY(commandLine.append("-fuse-ld=lld"));
        }
        if (not resolvedProject.compileFlags.enableStdCpp and resolvedProject.adapter.isClangLike())
        {
            SC_TRY(commandLine.append("-nostdlib++"));
        }
        if (resolvedProject.project->targetType == TargetType::SharedLibrary)
        {
            if (targetPlatform(resolvedProject.targetContext) == Platform::Apple)
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

        const bool supportsSanitizers =
            not(isLinuxTarget(resolvedProject.targetContext) and resolvedProject.adapter.family == Toolchain::Clang and
                resolvedProject.targetContext.hostMachine.platform != Platform::Linux);
        if (flags.enableASAN and supportsSanitizers)
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

        if (targetPlatform(resolvedProject.targetContext) == Platform::Linux and
            WriterInternal::isExecutableTarget(*resolvedProject.project) and
            not resolvedProject.project->exportLibraries.isEmpty())
        {
            SC_TRY(commandLine.append("-rdynamic"));
        }

        if (resolvedProject.project->targetType != TargetType::StaticLibrary and
            resolvedProject.linkFlags.enableDeadCodeStripping)
        {
            if (targetPlatform(resolvedProject.targetContext) == Platform::Apple)
            {
                SC_TRY(commandLine.append("-dead_strip"));
            }
            else if (targetPlatform(resolvedProject.targetContext) == Platform::Linux)
            {
                SC_TRY(commandLine.append("-Wl,--gc-sections"));
            }
        }

        if (WriterInternal::shouldPreserveExportedSymbols(*resolvedProject.project, resolvedProject.linkFlags))
        {
            switch (targetPlatform(resolvedProject.targetContext))
            {
            case Platform::Apple:
                SC_TRY(commandLine.append("-Xlinker"));
                SC_TRY(commandLine.append("-exported_symbols_list"));
                SC_TRY(commandLine.append("-Xlinker"));
                SC_TRY(commandLine.append(resolvedProject.exportedSymbolsPath.view()));
                break;
            case Platform::Linux: {
                SC_TRY(commandLine.append("-Wl,--gc-keep-exported"));
                String option = StringEncoding::Utf8;
                SC_TRY(StringBuilder::format(option, "-Wl,--dynamic-list={}",
                                             resolvedProject.exportedSymbolsLinkerPath.view()));
                SC_TRY(commandLine.append(option.view()));
                break;
            }
            case Platform::Unknown:
            case Platform::Windows:
            case Platform::Wasm: break;
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

        if (targetPlatform(resolvedProject.targetContext) == Platform::Apple)
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
        ResolvedTargetContext actionTargetContext;
        SC_TRY(resolveTargetContext(action.parameters, actionTargetContext));

        Vector<const Project*> orderedProjects;
        if (action.allTargets)
        {
            for (const Project& project : workspace.projects)
            {
                SC_TRY(appendProjectBuildOrder(workspace, targetPlatform(actionTargetContext), project,
                                               action.configurationName, orderedProjects));
            }
        }
        else
        {
            const Project*       project       = nullptr;
            const Configuration* configuration = nullptr;
            SC_TRY(findProjectConfiguration(workspace, action.projectName, action.configurationName, project,
                                            configuration));
            SC_COMPILER_UNUSED(configuration);
            SC_TRY(appendProjectBuildOrder(workspace, targetPlatform(actionTargetContext), *project,
                                           action.configurationName, orderedProjects));
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
        SC_TRY(findWorkspaceDependencies(workspace, targetPlatform(resolvedProject.targetContext), project,
                                         configuration.name.view(), resolvedProject.workspaceDependencies));

        Vector<String> externalLibraries;
        for (const String& library : resolvedProject.linkFlags.libraries)
        {
            const Project* dependency = nullptr;
            if (findWorkspaceStaticLibraryDependency(workspace, targetPlatform(resolvedProject.targetContext),
                                                     library.view(), dependency))
            {
                String    dependencyOutputDirectory = StringEncoding::Utf8;
                String    dependencyArtifactName    = StringEncoding::Utf8;
                String    dependencyArtifactPath    = StringEncoding::Utf8;
                Variables dependencyVariables;

                const Configuration* dependencyConfiguration = dependency->getConfiguration(configuration.name.view());
                SC_TRY_MSG(dependencyConfiguration != nullptr, "Dependency configuration not found");
                SC_TRY(fillVariables(parameters, *dependency, *dependencyConfiguration, resolvedProject.targetContext,
                                     resolvedProject.adapter.displayName.view(), dependencyVariables));
                SC_TRY(expandConfiguredPath(parameters.directories.outputsDirectory.view(),
                                            dependencyConfiguration->outputPath.view(), dependencyVariables,
                                            dependencyOutputDirectory));
                SC_TRY(computeArtifactName(targetPlatform(resolvedProject.targetContext), *dependency,
                                           dependencyArtifactName));
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

    static Machine resolveBuildMachine()
    {
        Machine buildMachine;
        switch (HostPlatform)
        {
        case SC::Platform::Apple: buildMachine.platform = Build::Platform::Apple; break;
        case SC::Platform::Linux: buildMachine.platform = Build::Platform::Linux; break;
        case SC::Platform::Windows: buildMachine.platform = Build::Platform::Windows; break;
        case SC::Platform::Emscripten: buildMachine.platform = Build::Platform::Wasm; break;
        }
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: buildMachine.architecture = Build::Architecture::Arm64; break;
        case InstructionSet::Intel64: buildMachine.architecture = Build::Architecture::Intel64; break;
        case InstructionSet::Intel32: buildMachine.architecture = Build::Architecture::Intel32; break;
        }
        buildMachine.environment = TargetEnvironment::Native;
        return buildMachine;
    }

    static Result resolveTargetContext(const Parameters& parameters, ResolvedTargetContext& context)
    {
        context.buildMachine = resolveBuildMachine();

        context.hostMachine = parameters.hostMachine;
        if (context.hostMachine.platform == Platform::Unknown)
        {
            context.hostMachine.platform = context.buildMachine.platform;
        }
        if (context.hostMachine.architecture == Architecture::Any)
        {
            context.hostMachine.architecture = context.buildMachine.architecture;
        }
        if (context.hostMachine.environment == TargetEnvironment::Native)
        {
            context.hostMachine.environment = context.buildMachine.environment;
        }

        context.targetMachine = parameters.targetMachine;
        if (context.targetMachine.platform == Platform::Unknown)
        {
            context.targetMachine.platform = parameters.platform;
        }
        if (context.targetMachine.architecture == Architecture::Any)
        {
            context.targetMachine.architecture = parameters.architecture;
        }
        if (context.targetMachine.platform == Platform::Unknown)
        {
            context.targetMachine.platform = context.hostMachine.platform;
        }
        if (context.targetMachine.architecture == Architecture::Any)
        {
            context.targetMachine.architecture = context.hostMachine.architecture;
        }
        if (context.targetMachine.environment == TargetEnvironment::Native and
            parameters.toolchain.family == Toolchain::LLVMMingw)
        {
            context.targetMachine.environment = TargetEnvironment::WindowsGNU;
        }
        return Result(true);
    }

    static constexpr bool isWindowsGNUTarget(const ResolvedTargetContext& context)
    {
        return context.targetMachine.platform == Platform::Windows and
               context.targetMachine.environment == TargetEnvironment::WindowsGNU;
    }

    static constexpr bool isWindowsMSVCTarget(const ResolvedTargetContext& context)
    {
        return context.targetMachine.platform == Platform::Windows and
               context.targetMachine.environment == TargetEnvironment::WindowsMSVC;
    }

    static constexpr bool isLinuxTarget(const ResolvedTargetContext& context)
    {
        return context.targetMachine.platform == Platform::Linux and
               (context.targetMachine.environment == TargetEnvironment::LinuxGlibc or
                context.targetMachine.environment == TargetEnvironment::LinuxMusl);
    }

    static constexpr bool shouldUsePackagedLLVMToolchain(const ResolvedTargetContext& context)
    {
        return isLinuxTarget(context) and context.hostMachine.platform != Platform::Linux;
    }

    static constexpr bool shouldUsePackagedLinuxSysroot(const ResolvedTargetContext& context)
    {
        return isLinuxTarget(context) and context.hostMachine.platform == Platform::Apple;
    }

    static constexpr StringView hostLLVMExecutableName(StringView baseName)
    {
#if SC_PLATFORM_WINDOWS
        if (baseName == "clang"_a8)
        {
            return "clang.exe"_a8;
        }
        if (baseName == "clang++"_a8)
        {
            return "clang++.exe"_a8;
        }
        if (baseName == "llvm-ar"_a8)
        {
            return "llvm-ar.exe"_a8;
        }
#endif
        return baseName;
    }

    static Result resolvePackagedLLVMToolchain(const Parameters& parameters, CompilerAdapter& adapter)
    {
        Tools::Package llvmPackage;
        SC_TRY(Tools::installLLVMToolchain(parameters.directories.packagesCacheDirectory.view(),
                                           parameters.directories.packagesInstallDirectory.view(), llvmPackage));

        String defaultCompilerC   = StringEncoding::Utf8;
        String defaultCompilerCpp = StringEncoding::Utf8;
        String defaultArchiver    = StringEncoding::Utf8;
        SC_TRY(Path::join(defaultCompilerC,
                          {llvmPackage.installDirectoryLink.view(), "bin", hostLLVMExecutableName("clang"_a8)}));
        SC_TRY(Path::join(defaultCompilerCpp,
                          {llvmPackage.installDirectoryLink.view(), "bin", hostLLVMExecutableName("clang++"_a8)}));
        SC_TRY(Path::join(defaultArchiver,
                          {llvmPackage.installDirectoryLink.view(), "bin", hostLLVMExecutableName("llvm-ar"_a8)}));

        SC_TRY(resolveExecutable(parameters.toolchain.compilerC.view(), defaultCompilerC.view(), adapter.executableC));
        SC_TRY(resolveExecutable(parameters.toolchain.compilerCpp.view(), defaultCompilerCpp.view(),
                                 adapter.executableCpp));
        SC_TRY(resolveExecutable(parameters.toolchain.linker.view(), adapter.executableCpp.view(),
                                 adapter.executableLink));
        SC_TRY(
            resolveExecutable(parameters.toolchain.archiver.view(), defaultArchiver.view(), adapter.executableArchive));
        return Result(true);
    }

    static Result resolvePackagedLinuxSysroot(const Parameters& parameters, const ResolvedTargetContext& context,
                                              String& sysroot)
    {
        Tools::LinuxSysrootSpec spec;
        switch (context.targetMachine.environment)
        {
        case TargetEnvironment::LinuxGlibc: spec.environment = Tools::LinuxSysrootSpec::Glibc; break;
        case TargetEnvironment::LinuxMusl: spec.environment = Tools::LinuxSysrootSpec::Musl; break;
        case TargetEnvironment::Native:
        case TargetEnvironment::WindowsGNU:
        case TargetEnvironment::WindowsMSVC:
            return Result::Error("Packaged Linux sysroots require a Linux glibc or musl target");
        }

        switch (targetArchitecture(context))
        {
        case Architecture::Intel64: spec.architecture = InstructionSet::Intel64; break;
        case Architecture::Arm64: spec.architecture = InstructionSet::ARM64; break;
        case Architecture::Intel32:
        case Architecture::Any:
        case Architecture::Wasm: return Result::Error("Packaged Linux sysroots only support x86_64 and arm64");
        }

        Tools::Package sysrootPackage;
        SC_TRY(Tools::installLinuxSysroot(parameters.directories.packagesCacheDirectory.view(),
                                          parameters.directories.packagesInstallDirectory.view(), spec,
                                          sysrootPackage));
        SC_TRY(sysroot.assign(sysrootPackage.installDirectoryLink.view()));
        return Result(true);
    }

    static Result windowsMSVCTargetArchitectureDirectory(Architecture::Type architecture, StringView& directoryName)
    {
        switch (architecture)
        {
        case Architecture::Intel64: directoryName = "x64"; return Result(true);
        case Architecture::Arm64: directoryName = "arm64"; return Result(true);
        case Architecture::Intel32:
        case Architecture::Any:
        case Architecture::Wasm: return Result::Error("Portable MSVC only supports x86_64 and arm64 targets");
        }
        Assert::unreachable();
    }

    static constexpr bool isWineRunnableWindowsTarget(const ResolvedTargetContext& context)
    {
        return context.targetMachine.platform == Platform::Windows and
               (context.targetMachine.environment == TargetEnvironment::WindowsGNU or
                context.targetMachine.environment == TargetEnvironment::WindowsMSVC);
    }

    static constexpr Platform::Type targetPlatform(const ResolvedTargetContext& context)
    {
        return context.targetMachine.platform;
    }

    static constexpr Architecture::Type targetArchitecture(const ResolvedTargetContext& context)
    {
        return context.targetMachine.architecture;
    }

    static constexpr StringView defaultTargetTriple(const ResolvedTargetContext& context)
    {
        if (isWindowsGNUTarget(context))
        {
            switch (targetArchitecture(context))
            {
            case Architecture::Intel64: return "x86_64-w64-windows-gnu";
            case Architecture::Arm64: return "aarch64-w64-windows-gnu";
            case Architecture::Intel32:
            case Architecture::Any:
            case Architecture::Wasm: break;
            }
        }
        if (isWindowsMSVCTarget(context))
        {
            switch (targetArchitecture(context))
            {
            case Architecture::Intel64: return "x86_64-pc-windows-msvc";
            case Architecture::Arm64: return "aarch64-pc-windows-msvc";
            case Architecture::Intel32: return "i686-pc-windows-msvc";
            case Architecture::Any:
            case Architecture::Wasm: break;
            }
        }
        if (targetPlatform(context) == Platform::Linux)
        {
            switch (context.targetMachine.environment)
            {
            case TargetEnvironment::LinuxGlibc:
                switch (targetArchitecture(context))
                {
                case Architecture::Intel64: return "x86_64-unknown-linux-gnu";
                case Architecture::Arm64: return "aarch64-unknown-linux-gnu";
                case Architecture::Intel32:
                case Architecture::Any:
                case Architecture::Wasm: break;
                }
                break;
            case TargetEnvironment::LinuxMusl:
                switch (targetArchitecture(context))
                {
                case Architecture::Intel64: return "x86_64-unknown-linux-musl";
                case Architecture::Arm64: return "aarch64-unknown-linux-musl";
                case Architecture::Intel32:
                case Architecture::Any:
                case Architecture::Wasm: break;
                }
                break;
            case TargetEnvironment::Native:
            case TargetEnvironment::WindowsGNU:
            case TargetEnvironment::WindowsMSVC: break;
            }
        }
        return {};
    }

    static constexpr bool canRunDirectly(const ResolvedTargetContext& context)
    {
        return targetPlatform(context) == context.hostMachine.platform and
               targetArchitecture(context) == context.hostMachine.architecture and
               context.targetMachine.environment == TargetEnvironment::Native;
    }

    static constexpr bool shouldPreferWineConsole(const ResolvedTargetContext& context)
    {
        return context.hostMachine.platform == Platform::Linux and
               context.hostMachine.architecture != Architecture::Arm64;
    }

    static Result appendRunnerArgument(Vector<String>& arguments, StringView value)
    {
        String item = StringEncoding::Utf8;
        SC_TRY(item.assign(value));
        SC_TRY(arguments.push_back(move(item)));
        return Result(true);
    }

    static Result appendRunnerArguments(Span<const String> source, Vector<String>& destination)
    {
        for (const String& argument : source)
        {
            SC_TRY(appendRunnerArgument(destination, argument.view()));
        }
        return Result(true);
    }

    static bool pathExists(StringView path)
    {
        FileSystem fs;
        if (not fs.init("."))
        {
            return false;
        }
        return fs.exists(path);
    }

    static Result resolveHostCommandPath(StringView executable, String& output)
    {
        if (StringView(executable).containsString("/") or StringView(executable).containsString("\\"))
        {
            SC_TRY(output.assign(executable));
            return Result(true);
        }

        Process process;
        String  commandPath = StringEncoding::Utf8;
        SC_TRY(process.exec({"which", executable}, commandPath));
        SC_TRY_MSG(process.getExitStatus() == 0, "Cannot resolve host command path");
        SC_TRY(output.assign(StringView(commandPath.view()).trimWhiteSpaces()));
        return Result(true);
    }

    static bool resolveRunnableHostCommand(StringView executable, String& output)
    {
        if (not resolveHostCommandPath(executable, output))
        {
            return false;
        }
        return probeExecutable(output.view());
    }

    static Result writeLinuxWrappedRunnerExecutable(StringView packagesCacheDirectory, StringView wrapperDirectoryName,
                                                    StringView executableName, StringView firstStage,
                                                    StringView secondStage, String& output)
    {
        FileSystem fs;
        SC_TRY(fs.init("."));

        String wrapperDirectory = StringEncoding::Utf8;
        SC_TRY(Path::join(wrapperDirectory, {packagesCacheDirectory, "runners", wrapperDirectoryName}));
        SC_TRY(fs.makeDirectoryRecursive(wrapperDirectory.view()));

        SC_TRY(Path::join(output, {wrapperDirectory.view(), executableName}));

        String scriptContents = StringEncoding::Utf8;
        auto   builder        = StringBuilder::create(scriptContents);
        SC_TRY(builder.append("#!/bin/sh\n"));
        SC_TRY(builder.append("exec \"{}\" \"{}\" \"$@\"\n", firstStage, secondStage));
        builder.finalize();

        SC_TRY(fs.writeString(output.view(), scriptContents.view()));
        SC_TRY(fs.chmod(output.view(), 0755u));
        return Result(true);
    }

    static Result resolveLinuxWineExecutable(const Parameters& parameters, const ResolvedTargetContext& targetContext,
                                             StringView configured, String& output)
    {
        if (not configured.isEmpty())
        {
            SC_TRY(resolveHostCommandPath(configured, output));
            return Result(true);
        }

        String wine64Path      = StringEncoding::Utf8;
        String winePath        = StringEncoding::Utf8;
        String wineConsolePath = StringEncoding::Utf8;
        String box64Path       = StringEncoding::Utf8;

        const bool hasWine64      = resolveRunnableHostCommand("wine64", wine64Path);
        const bool hasWine        = resolveRunnableHostCommand("wine", winePath);
        const bool hasWineConsole = resolveRunnableHostCommand("wineconsole", wineConsolePath);
        const bool hasBox64       = resolveRunnableHostCommand("box64", box64Path);

        if (parameters.hostMachine.architecture == Architecture::Arm64 and
            targetArchitecture(targetContext) == Architecture::Arm64)
        {
            if (hasWine64)
            {
                SC_TRY(output.assign(wine64Path.view()));
                return Result(true);
            }
            if (hasWine)
            {
                SC_TRY(output.assign(winePath.view()));
                return Result(true);
            }

            Tools::Package winePackage;
            if (Tools::installLinuxNativeArm64WineRunner(parameters.directories.packagesCacheDirectory.view(),
                                                         parameters.directories.packagesInstallDirectory.view(),
                                                         winePackage))
            {
                SC_TRY(Path::join(output, {winePackage.installDirectoryLink.view(), "bin", "wine"}));
                return Result(true);
            }
            return Result::Error("Cannot find a usable Windows ARM64 Wine runner. Install wine64/wine for Linux arm64, "
                                 "or pass --runner-path with an ARM64-capable Wine wrapper.");
        }

        if (parameters.hostMachine.architecture == Architecture::Arm64 and hasBox64)
        {
            StringView wineStage       = hasWine64 ? wine64Path.view() : winePath.view();
            StringView wrapperRootName = hasWine64 ? "linux-box64-wine64"_a8 : "linux-box64-wine"_a8;
            if (not wineStage.isEmpty())
            {
                SC_TRY(writeLinuxWrappedRunnerExecutable(parameters.directories.packagesCacheDirectory.view(),
                                                         wrapperRootName, "wine", box64Path.view(), wineStage, output));
                if (hasWineConsole)
                {
                    String unused = StringEncoding::Utf8;
                    SC_TRY(writeLinuxWrappedRunnerExecutable(parameters.directories.packagesCacheDirectory.view(),
                                                             wrapperRootName, "wineconsole", box64Path.view(),
                                                             wineConsolePath.view(), unused));
                }
                return Result(true);
            }
        }

        if (hasWine64)
        {
            SC_TRY(output.assign(wine64Path.view()));
            return Result(true);
        }
        if (hasWine)
        {
            SC_TRY(output.assign(winePath.view()));
            return Result(true);
        }

        if (parameters.hostMachine.architecture == Architecture::Arm64)
        {
            Tools::Package winePackage;
            if (Tools::installWineStableRunner(parameters.directories.packagesCacheDirectory.view(),
                                               parameters.directories.packagesInstallDirectory.view(), winePackage))
            {
                SC_TRY(Path::join(output, {winePackage.installDirectoryLink.view(), "bin", "wine"}));
                return Result(true);
            }
            return Result::Error("Cannot find a usable Wine runner. Install wine64/wine, or install box64 plus "
                                 "wine64/wine, or pass --runner-path with a wrapper path. Linux arm64 hosts need "
                                 "a runner that can launch the Windows x64 tools and binaries.");
        }
        return Result::Error("Cannot find runner executable");
    }

    static Result resolveWrappedRunnerExecutable(StringView configured, StringView primaryFallback,
                                                 StringView secondaryFallback, String& output)
    {
        if (not configured.isEmpty())
        {
            SC_TRY(output.assign(configured));
            return Result(true);
        }
        if (not primaryFallback.isEmpty() and probeExecutable(primaryFallback))
        {
            SC_TRY(output.assign(primaryFallback));
            return Result(true);
        }
        if (not secondaryFallback.isEmpty() and probeExecutable(secondaryFallback))
        {
            SC_TRY(output.assign(secondaryFallback));
            return Result(true);
        }
        return Result::Error("Cannot find runner executable");
    }

    static Result resolveAppleWineExecutable(const Parameters& parameters, StringView configured, String& output)
    {
        if (not configured.isEmpty())
        {
            SC_TRY(output.assign(configured));
            return Result(true);
        }

        static constexpr StringView commonWinePaths[] = {
            "/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine",
            "/Applications/Wine Devel.app/Contents/Resources/wine/bin/wine",
            "/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine",
        };
        for (const StringView candidate : commonWinePaths)
        {
            if (probeExecutable(candidate))
            {
                SC_TRY(output.assign(candidate));
                return Result(true);
            }
        }

        Tools::Package winePackage;
        SC_TRY(Tools::installWineStableRunner(parameters.directories.packagesCacheDirectory.view(),
                                              parameters.directories.packagesInstallDirectory.view(), winePackage));
        SC_TRY(StringBuilder::format(output, "{}/Wine Stable.app/Contents/Resources/wine/bin/wine",
                                     winePackage.installDirectoryLink));
        return Result(true);
    }

    static Result resolveRunner(const Parameters& parameters, const ResolvedTargetContext& targetContext,
                                ResolvedRunner& runner)
    {
        const RunnerSpec& runnerSpec = parameters.runner;
        switch (runnerSpec.type)
        {
        case RunnerSpec::None:
            SC_TRY_MSG(canRunDirectly(targetContext), "Runner is disabled for foreign targets");
            runner.mode = ResolvedRunner::Direct;
            return Result(true);
        case RunnerSpec::Auto:
            if (canRunDirectly(targetContext))
            {
                runner.mode = ResolvedRunner::Direct;
                return Result(true);
            }
            if (isWineRunnableWindowsTarget(targetContext))
            {
                SC_TRY_MSG(targetContext.hostMachine.platform == Platform::Apple or
                               targetContext.hostMachine.platform == Platform::Linux,
                           "Wine auto-run is only supported on macOS and Linux hosts");
                runner.mode = ResolvedRunner::Wrapped;
                if (targetContext.hostMachine.platform == Platform::Apple)
                {
                    SC_TRY(resolveAppleWineExecutable(parameters, runnerSpec.executable.view(), runner.executable));
                }
                else
                {
                    SC_TRY(resolveLinuxWineExecutable(parameters, targetContext, runnerSpec.executable.view(),
                                                      runner.executable));
                }
                SC_TRY(appendRunnerArguments(runnerSpec.arguments.toSpanConst(), runner.arguments));
                return Result(true);
            }
            return Result::Error("No auto runner is available for this host/target pair");
        case RunnerSpec::Wine:
            SC_TRY_MSG(targetContext.targetMachine.platform == Platform::Windows,
                       "Wine runner requires a Windows target");
            SC_TRY_MSG(targetContext.hostMachine.platform == Platform::Apple or
                           targetContext.hostMachine.platform == Platform::Linux,
                       "Wine runner is only supported on macOS and Linux hosts");
            SC_TRY_MSG(isWineRunnableWindowsTarget(targetContext), "Wine runner requires a Windows GNU or MSVC target");
            runner.mode = ResolvedRunner::Wrapped;
            if (targetContext.hostMachine.platform == Platform::Apple)
            {
                SC_TRY(resolveAppleWineExecutable(parameters, runnerSpec.executable.view(), runner.executable));
            }
            else
            {
                SC_TRY(resolveLinuxWineExecutable(parameters, targetContext, runnerSpec.executable.view(),
                                                  runner.executable));
            }
            SC_TRY(appendRunnerArguments(runnerSpec.arguments.toSpanConst(), runner.arguments));
            return Result(true);
        case RunnerSpec::QEMU: return Result::Error("QEMU runner is not implemented yet");
        case RunnerSpec::Custom:
            SC_TRY_MSG(not runnerSpec.executable.isEmpty(), "Custom runner requires executable");
            runner.mode = ResolvedRunner::Wrapped;
            SC_TRY(runner.executable.assign(runnerSpec.executable.view()));
            SC_TRY(appendRunnerArguments(runnerSpec.arguments.toSpanConst(), runner.arguments));
            return Result(true);
        }
        Assert::unreachable();
    }

    static Result configureWineProcess(Process& process, StringView prefixDirectory,
                                       const ResolvedTargetContext& targetContext)
    {
        SC_TRY(process.setEnvironment("WINEPREFIX", prefixDirectory));
        SC_TRY(process.setEnvironment("WINEDLLOVERRIDES", "winemenubuilder.exe=d;winedbg.exe=d;vctip.exe=d"));
        SC_TRY(process.setEnvironment("WINEDEBUG", "-all"));
        SC_TRY(process.setEnvironment("MVK_CONFIG_LOG_LEVEL", "0"));
        if (targetArchitecture(targetContext) == Architecture::Intel64 or
            targetArchitecture(targetContext) == Architecture::Arm64)
        {
            SC_TRY(process.setEnvironment("WINEARCH", "win64"));
        }
        return Result(true);
    }

    static Result convertPosixPathToWindowsPath(StringView posixPath, String& windowsPath)
    {
        String normalizedPath = StringEncoding::Utf8;
        SC_TRY(Path::normalize(normalizedPath, posixPath, Path::AsNative));

        auto builder = StringBuilder::create(windowsPath);
        SC_TRY(builder.append("Z:"));
        for (size_t idx = 0; idx < normalizedPath.view().sizeInBytes(); ++idx)
        {
            const char character = normalizedPath.view().bytesWithoutTerminator()[idx];
            if (character == '/')
            {
                SC_TRY(builder.append("\\"));
            }
            else
            {
                const char singleCharacter[] = {character, '\0'};
                SC_TRY(builder.append(StringView::fromNullTerminated(singleCharacter, StringEncoding::Ascii)));
            }
        }
        builder.finalize();
        return Result(true);
    }

    static Result bundledWineLoaderDirectory(StringView runnerExecutable, StringView loaderDirectory, String& output)
    {
        SC_TRY(output.assign(Path::dirname(runnerExecutable, Path::AsNative)));
        SC_TRY(Path::append(output, {"..", "lib", "wine", loaderDirectory}, Path::AsNative));
        String normalized = StringEncoding::Utf8;
        SC_TRY(Path::normalize(normalized, output.view(), Path::AsNative));
        SC_TRY(output.assign(normalized.view()));
        return Result(true);
    }

    static bool bundledWineHasWindowsLoader(StringView runnerExecutable, Architecture::Type architecture)
    {
        String wineLibraryRoot = StringEncoding::Utf8;
        if (not bundledWineLoaderDirectory(runnerExecutable, ".", wineLibraryRoot))
        {
            return true;
        }

        if (not pathExists(wineLibraryRoot.view()))
        {
            return true;
        }

        auto hasLoader = [&](StringView loaderDirectory) -> bool
        {
            String candidate = StringEncoding::Utf8;
            return bundledWineLoaderDirectory(runnerExecutable, loaderDirectory, candidate) and
                   pathExists(candidate.view());
        };

        switch (architecture)
        {
        case Architecture::Intel64: return hasLoader("x86_64-windows");
        case Architecture::Arm64: return hasLoader("arm64-windows") or hasLoader("aarch64-windows");
        case Architecture::Intel32: return hasLoader("i386-windows");
        case Architecture::Any:
        case Architecture::Wasm: return false;
        }
        Assert::unreachable();
    }

    static Result prepareWinePrefixHeadless(StringView runnerExecutable, StringView prefixDirectory,
                                            const ResolvedTargetContext& targetContext)
    {
        Process process;
        SC_TRY(configureWineProcess(process, prefixDirectory, targetContext));

        String           stdOut                     = StringEncoding::Utf8;
        String           stdErr                     = StringEncoding::Utf8;
        const StringSpan showCrashDialogArguments[] = {
            runnerExecutable,
            "reg",
            "add",
            "HKEY_CURRENT_USER\\Software\\Wine\\WineDbg",
            "/v",
            "ShowCrashDialog",
            "/t",
            "REG_DWORD",
            "/d",
            "0",
            "/f",
        };
        SC_TRY(process.exec(showCrashDialogArguments, stdOut, {}, stdErr));
        SC_TRY_MSG(process.getExitStatus() == 0, "Failed configuring WineDbg crash dialog");

        Process process2;
        SC_TRY(configureWineProcess(process2, prefixDirectory, targetContext));
        const StringSpan breakOnFirstChanceArguments[] = {
            runnerExecutable,
            "reg",
            "add",
            "HKEY_CURRENT_USER\\Software\\Wine\\WineDbg",
            "/v",
            "BreakOnFirstChance",
            "/t",
            "REG_DWORD",
            "/d",
            "0",
            "/f",
        };
        String stdOut2 = StringEncoding::Utf8;
        String stdErr2 = StringEncoding::Utf8;
        SC_TRY(process2.exec(breakOnFirstChanceArguments, stdOut2, {}, stdErr2));
        SC_TRY_MSG(process2.getExitStatus() == 0, "Failed configuring WineDbg first-chance exceptions");

        Process process3;
        SC_TRY(configureWineProcess(process3, prefixDirectory, targetContext));
        const StringSpan removeWinemenubuilderArguments[] = {
            runnerExecutable,
            "reg",
            "delete",
            "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
            "/v",
            "winemenubuilder",
            "/f",
        };
        String stdOut3 = StringEncoding::Utf8;
        String stdErr3 = StringEncoding::Utf8;
        SC_TRY(process3.exec(removeWinemenubuilderArguments, stdOut3, {}, stdErr3));
        SC_TRY_MSG(process3.getExitStatus() == 0 or process3.getExitStatus() == 1,
                   "Failed disabling Wine menu builder startup hook");

        Process process4;
        SC_TRY(configureWineProcess(process4, prefixDirectory, targetContext));
        const StringSpan removeWow64WinemenubuilderArguments[] = {
            runnerExecutable,
            "reg",
            "delete",
            "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\RunServices",
            "/v",
            "winemenubuilder",
            "/f",
        };
        String stdOut4 = StringEncoding::Utf8;
        String stdErr4 = StringEncoding::Utf8;
        SC_TRY(process4.exec(removeWow64WinemenubuilderArguments, stdOut4, {}, stdErr4));
        SC_TRY_MSG(process4.getExitStatus() == 0 or process4.getExitStatus() == 1,
                   "Failed disabling Wine menu builder startup hook");
        return Result(true);
    }

    static Result appendTargeting(CommandLine& commandLine, const ResolvedProject& resolvedProject, bool forCompiler)
    {
        if (resolvedProject.adapter.isMSVCStyle())
        {
            StringView targetTriple = resolvedProject.parameters->toolchain.targetTriple.view();
            if (targetTriple.isEmpty() and resolvedProject.adapter.isClangCL())
            {
                switch (targetArchitecture(resolvedProject.targetContext))
                {
                case Architecture::Intel64: targetTriple = "x86_64-pc-windows-msvc"; break;
                case Architecture::Intel32: targetTriple = "i686-pc-windows-msvc"; break;
                case Architecture::Arm64: targetTriple = "aarch64-pc-windows-msvc"; break;
                case Architecture::Any:
                case Architecture::Wasm: break;
                }
            }
            if (not resolvedProject.resolvedSysroot.isEmpty())
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
        if (targetTriple.isEmpty())
        {
            targetTriple = defaultTargetTriple(resolvedProject.targetContext);
        }
        if (targetTriple.isEmpty() and targetPlatform(resolvedProject.targetContext) == Platform::Apple and
            targetArchitecture(resolvedProject.targetContext) != Architecture::Any and
            resolvedProject.adapter.isClangLike())
        {
            switch (targetArchitecture(resolvedProject.targetContext))
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

        if (not resolvedProject.resolvedSysroot.isEmpty())
        {
            if (targetPlatform(resolvedProject.targetContext) == Platform::Apple)
            {
                SC_TRY(commandLine.append("-isysroot"));
                SC_TRY(commandLine.append(resolvedProject.resolvedSysroot.view()));
            }
            else
            {
                SC_TRY(commandLine.append("--sysroot"));
                SC_TRY(commandLine.append(resolvedProject.resolvedSysroot.view()));
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
                                const Configuration& configuration, const ResolvedTargetContext& targetContext,
                                StringView compilerName, Variables& variables)
    {
        SC_TRY(variables.projectName.assign(project.name.view()));
        SC_TRY(variables.projectRoot.assign(project.rootDirectory.view()));
        if (isLinuxTarget(targetContext))
        {
            SC_TRY(variables.targetOS.assign(TargetEnvironment::toString(targetContext.targetMachine.environment)));
        }
        else
        {
            SC_TRY(variables.targetOS.assign(platformName(targetPlatform(targetContext))));
        }
        SC_TRY(variables.targetArchitectures.assign(architectureName(targetArchitecture(targetContext))));
        SC_TRY(variables.buildSystem.assign(Generator::toString(parameters.generator)));
        SC_TRY(variables.compiler.assign(compilerName));
        SC_TRY(variables.configuration.assign(configuration.name.view()));
        return Result(true);
    }

    static Result resolveCompilerAdapter(const Parameters& parameters, const ResolvedTargetContext& targetContext,
                                         CompilerAdapter& adapter)
    {
        const Toolchain& toolchain = parameters.toolchain;

        adapter.family = toolchain.family;
        if (adapter.family == Toolchain::HostDefault)
        {
            if (isWindowsGNUTarget(targetContext))
            {
                adapter.family = Toolchain::LLVMMingw;
            }
            else if (targetPlatform(targetContext) == Platform::Windows)
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
            if (shouldUsePackagedLLVMToolchain(targetContext))
            {
                SC_TRY(resolvePackagedLLVMToolchain(parameters, adapter));
            }
            else
            {
                SC_TRY(resolveExecutable(toolchain.compilerC.view(), "clang", adapter.executableC));
                SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), "clang++", adapter.executableCpp));
                SC_TRY(
                    resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
                SC_TRY(resolveExecutable(toolchain.archiver.view(), "ar", adapter.executableArchive));
            }
            SC_TRY(adapter.displayName.assign("clang"));
            break;
        case Toolchain::GCC:
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), "gcc", adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), "g++", adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), "ar", adapter.executableArchive));
            SC_TRY(adapter.displayName.assign("gcc"));
            break;
        case Toolchain::LLVMMingw: {
            SC_TRY_MSG(targetContext.hostMachine.platform == Platform::Apple or
                           targetContext.hostMachine.platform == Platform::Linux,
                       "llvm-mingw cross compilation is only supported on macOS and Linux hosts");
            SC_TRY_MSG(isWindowsGNUTarget(targetContext), "llvm-mingw requires a Windows GNU target");

            Tools::Package llvmMingwPackage;
            SC_TRY(Tools::installLLVMMingwToolchain(parameters.directories.packagesCacheDirectory.view(),
                                                    parameters.directories.packagesInstallDirectory.view(),
                                                    llvmMingwPackage));

            StringView compilerPrefix;
            switch (targetArchitecture(targetContext))
            {
            case Architecture::Intel64: compilerPrefix = "x86_64-w64-mingw32"; break;
            case Architecture::Arm64: compilerPrefix = "aarch64-w64-mingw32"; break;
            case Architecture::Intel32:
            case Architecture::Any:
            case Architecture::Wasm:
                return Result::Error("llvm-mingw only supports x86_64 and arm64 Windows GNU targets");
            }

            String defaultCompilerC   = StringEncoding::Utf8;
            String defaultCompilerCpp = StringEncoding::Utf8;
            String defaultArchiver    = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(defaultCompilerC, "{}/bin/{}-clang", llvmMingwPackage.installDirectoryLink,
                                         compilerPrefix));
            SC_TRY(StringBuilder::format(defaultCompilerCpp, "{}/bin/{}-clang++", llvmMingwPackage.installDirectoryLink,
                                         compilerPrefix));
            SC_TRY(StringBuilder::format(defaultArchiver, "{}/bin/llvm-ar", llvmMingwPackage.installDirectoryLink));
            SC_TRY(resolveExecutable(toolchain.compilerC.view(), defaultCompilerC.view(), adapter.executableC));
            SC_TRY(resolveExecutable(toolchain.compilerCpp.view(), defaultCompilerCpp.view(), adapter.executableCpp));
            SC_TRY(resolveExecutable(toolchain.linker.view(), adapter.executableCpp.view(), adapter.executableLink));
            SC_TRY(resolveExecutable(toolchain.archiver.view(), defaultArchiver.view(), adapter.executableArchive));
            SC_TRY(adapter.displayName.assign("llvm-mingw"));
            break;
        }
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
            if (targetContext.hostMachine.platform != Platform::Windows and isWindowsMSVCTarget(targetContext))
            {
                SC_TRY_MSG(targetContext.hostMachine.platform == Platform::Apple or
                               targetContext.hostMachine.platform == Platform::Linux,
                           "Portable MSVC cross compilation is only supported on macOS and Linux hosts");

                Tools::Package msvcPackage;
                SC_TRY(Tools::installMSVCToolchain(parameters.directories.packagesCacheDirectory.view(),
                                                   parameters.directories.packagesInstallDirectory.view(),
                                                   msvcPackage));

                String     defaultCompilerC   = StringEncoding::Utf8;
                String     defaultCompilerCpp = StringEncoding::Utf8;
                String     defaultLinker      = StringEncoding::Utf8;
                String     defaultArchiver    = StringEncoding::Utf8;
                StringView targetDirectory;
                SC_TRY(windowsMSVCTargetArchitectureDirectory(targetArchitecture(targetContext), targetDirectory));
                SC_TRY(Path::join(defaultCompilerC,
                                  {msvcPackage.installDirectoryLink.view(), "bin", targetDirectory, "cl"}));
                SC_TRY(Path::join(defaultCompilerCpp,
                                  {msvcPackage.installDirectoryLink.view(), "bin", targetDirectory, "cl"}));
                SC_TRY(Path::join(defaultLinker,
                                  {msvcPackage.installDirectoryLink.view(), "bin", targetDirectory, "link"}));
                SC_TRY(Path::join(defaultArchiver,
                                  {msvcPackage.installDirectoryLink.view(), "bin", targetDirectory, "lib"}));
                SC_TRY(resolveExecutable(toolchain.compilerC.view(), defaultCompilerC.view(), adapter.executableC));
                SC_TRY(
                    resolveExecutable(toolchain.compilerCpp.view(), defaultCompilerCpp.view(), adapter.executableCpp));
                SC_TRY(resolveExecutable(toolchain.linker.view(), defaultLinker.view(), adapter.executableLink));
                SC_TRY(resolveExecutable(toolchain.archiver.view(), defaultArchiver.view(), adapter.executableArchive));
            }
            else
            {
                SC_TRY(resolveExecutable(toolchain.compilerC.view(), "cl", adapter.executableC));
                SC_TRY(
                    resolveExecutable(toolchain.compilerCpp.view(), adapter.executableC.view(), adapter.executableCpp));
                SC_TRY(resolveExecutable(toolchain.linker.view(), "link", adapter.executableLink));
                SC_TRY(resolveExecutable(toolchain.archiver.view(), "lib", adapter.executableArchive));
            }
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
        globalConsole->print("[trace] build={} host={} target={} env={}\n",
                             platformName(resolvedProject.targetContext.buildMachine.platform),
                             platformName(resolvedProject.targetContext.hostMachine.platform),
                             platformName(targetPlatform(resolvedProject.targetContext)),
                             TargetEnvironment::toString(resolvedProject.targetContext.targetMachine.environment));
        if (not resolvedProject.parameters->toolchain.targetTriple.isEmpty() or
            not resolvedProject.resolvedSysroot.isEmpty())
        {
            globalConsole->print("[trace] target triple={} sysroot={}\n",
                                 resolvedProject.parameters->toolchain.targetTriple.view(),
                                 resolvedProject.resolvedSysroot.view());
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
