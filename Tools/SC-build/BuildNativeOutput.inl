// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

namespace SC
{
namespace Build
{
struct NativeJobKind
{
    enum Type
    {
        Compile,
        Link,
        Archive,
        Strip,
    };
};

struct NativeJobStatus
{
    enum Type
    {
        Skipped,
        Succeeded,
        Failed,
        CancelledBeforeStart,
    };
};

struct NativeJobRecord
{
    size_t step       = 0;
    size_t totalSteps = 0;

    NativeJobKind::Type   kind   = NativeJobKind::Compile;
    NativeJobStatus::Type status = NativeJobStatus::Succeeded;

    String stepName      = StringEncoding::Utf8;
    String label         = StringEncoding::Utf8;
    String command       = StringEncoding::Utf8;
    String rebuildReason = StringEncoding::Utf8;
    String stdOut        = StringEncoding::Utf8;
    String stdErr        = StringEncoding::Utf8;

    int  exitStatus = -1;
    bool started    = false;
    bool finished   = false;

    Result clear()
    {
        step       = 0;
        totalSteps = 0;
        kind       = NativeJobKind::Compile;
        status     = NativeJobStatus::Succeeded;
        exitStatus = -1;
        started    = false;
        finished   = false;
        SC_TRY(stepName.assign({}));
        SC_TRY(label.assign({}));
        SC_TRY(command.assign({}));
        SC_TRY(rebuildReason.assign({}));
        SC_TRY(stdOut.assign({}));
        SC_TRY(stdErr.assign({}));
        return Result(true);
    }
};

struct NativeBuildSummary
{
    size_t plannedSteps         = 0;
    size_t started              = 0;
    size_t skipped              = 0;
    size_t succeeded            = 0;
    size_t failed               = 0;
    size_t drainedAfterFailure  = 0;
    size_t cancelledBeforeStart = 0;

    Time::Monotonic    startedAt;
    Time::Milliseconds elapsed;
};

struct NativeBuildReporter
{
    OutputMode::Type mode = OutputMode::Normal;

    bool failureDetected         = false;
    bool deferredFailuresFlushed = false;

    NativeBuildSummary      summary;
    Vector<NativeJobRecord> failedJobs;

    static OutputMode::Type resolveMode(const ExecutionOptions& execution)
    {
        if (execution.outputMode.hasBeenSet())
        {
            return execution.outputMode;
        }
        return execution.verbose ? OutputMode::Verbose : OutputMode::Normal;
    }

    Result begin(const ExecutionOptions& execution, size_t totalSteps)
    {
        mode                         = resolveMode(execution);
        failureDetected              = false;
        deferredFailuresFlushed      = false;
        summary.plannedSteps         = totalSteps;
        summary.started              = 0;
        summary.skipped              = 0;
        summary.succeeded            = 0;
        summary.failed               = 0;
        summary.drainedAfterFailure  = 0;
        summary.cancelledBeforeStart = 0;
        summary.startedAt            = Time::Monotonic::now();
        summary.elapsed              = Time::Milliseconds();
        failedJobs.clear();
        return Result(true);
    }

    [[nodiscard]] bool isQuiet() const { return mode == OutputMode::Quiet; }
    [[nodiscard]] bool isNormal() const { return mode == OutputMode::Normal; }
    [[nodiscard]] bool isVerbose() const { return mode == OutputMode::Verbose; }
    [[nodiscard]] bool shouldStopScheduling() const { return failureDetected; }

    Result printStepStarted(NativeJobRecord& job)
    {
        job.started = true;
        summary.started++;
        if (not isQuiet())
        {
            globalConsole->print("[{}/{}] {} {}\n", job.step, job.totalSteps, job.stepName.view(), job.label.view());
            globalConsole->flush();
        }
        return Result(true);
    }

    Result printRebuildTrace(StringView stepName, StringView label, StringView reason)
    {
        if (isVerbose())
        {
            globalConsole->print("[trace] rebuild {} {} because {}\n", stepName, label, reason);
        }
        return Result(true);
    }

    Result recordSkipped(size_t step, size_t totalSteps, StringView stepName, StringView label, StringView reason)
    {
        summary.skipped++;
        if (isVerbose())
        {
            globalConsole->print("[{}/{}] SKIP {} {} ({})\n", step, totalSteps, stepName, label, reason);
        }
        return Result(true);
    }

    void recordCancelled(size_t count) { summary.cancelledBeforeStart += count; }

    Result recordCompleted(NativeJobRecord& job)
    {
        const bool wasFailing = failureDetected;
        job.finished          = true;
        switch (job.status)
        {
        case NativeJobStatus::Skipped: summary.skipped++; break;
        case NativeJobStatus::Succeeded:
            summary.succeeded++;
            if (wasFailing)
            {
                summary.drainedAfterFailure++;
            }
            SC_TRY(printSuccessfulOutput(job));
            break;
        case NativeJobStatus::Failed:
            summary.failed++;
            if (wasFailing)
            {
                summary.drainedAfterFailure++;
            }
            failureDetected = true;
            SC_TRY(failedJobs.push_back(move(job)));
            break;
        case NativeJobStatus::CancelledBeforeStart: summary.cancelledBeforeStart++; break;
        }
        return Result(true);
    }

    Result flushDeferredFailures()
    {
        if (deferredFailuresFlushed or failedJobs.isEmpty())
        {
            deferredFailuresFlushed = true;
            return Result(true);
        }

        Algorithms::bubbleSort(failedJobs.begin(), failedJobs.end(),
                               [](const NativeJobRecord& left, const NativeJobRecord& right)
                               { return left.step < right.step; });
        for (const NativeJobRecord& job : failedJobs)
        {
            SC_TRY(printFailureBlock(job));
        }
        deferredFailuresFlushed = true;
        return Result(true);
    }

    Result printFinalSummary()
    {
        summary.elapsed = Time::Monotonic::now().subtractExact(summary.startedAt);

        String line    = StringEncoding::Utf8;
        auto   builder = StringBuilder::create(line);
        SC_TRY(builder.append("Build Summary: "));
        SC_TRY(builder.append("{} succeeded", summary.succeeded));
        if (summary.skipped > 0)
        {
            SC_TRY(builder.append(", {} skipped", summary.skipped));
        }
        if (summary.failed > 0)
        {
            SC_TRY(builder.append(", {} failed", summary.failed));
        }
        if (summary.cancelledBeforeStart > 0)
        {
            SC_TRY(builder.append(", {} cancelled", summary.cancelledBeforeStart));
        }
        SC_TRY(builder.append(" in {}ms", summary.elapsed.ms));
        builder.finalize();
        globalConsole->print("{}\n", line.view());
        globalConsole->flush();
        return Result(true);
    }

  private:
    static Result printCapturedStream(StringView text, bool useStdErr)
    {
        if (text.isEmpty())
        {
            return Result(true);
        }
        if (useStdErr)
        {
            globalConsole->printError(text);
            const char* bytes = text.bytesWithoutTerminator();
            if (text.sizeInBytes() == 0 or bytes[text.sizeInBytes() - 1] != '\n')
            {
                globalConsole->printError("\n"_a8);
            }
            globalConsole->flushStdErr();
        }
        else
        {
            globalConsole->print(text);
            const char* bytes = text.bytesWithoutTerminator();
            if (text.sizeInBytes() == 0 or bytes[text.sizeInBytes() - 1] != '\n')
            {
                globalConsole->print("\n"_a8);
            }
            globalConsole->flush();
        }
        return Result(true);
    }

    Result printSuccessfulOutput(const NativeJobRecord& job)
    {
        const bool hasOutput = not(job.stdOut.isEmpty() and job.stdErr.isEmpty());
        if (not hasOutput)
        {
            return Result(true);
        }

        const bool printOutput = job.kind == NativeJobKind::Compile ? isVerbose() : (isVerbose() or isNormal());
        if (not printOutput)
        {
            return Result(true);
        }

        globalConsole->print("OUTPUT: [{}/{}] {} {}\n", job.step, job.totalSteps, job.stepName.view(),
                             job.label.view());
        SC_TRY(printCapturedStream(job.stdOut.view(), false));
        SC_TRY(printCapturedStream(job.stdErr.view(), true));
        return Result(true);
    }

    Result printFailureBlock(const NativeJobRecord& job)
    {
        globalConsole->print("FAILED: [{}/{}] {} {}", job.step, job.totalSteps, job.stepName.view(), job.label.view());
        if (job.exitStatus >= 0)
        {
            globalConsole->print(" (exit code {})", job.exitStatus);
        }
        globalConsole->print("\n");
        if (isVerbose())
        {
            if (not job.command.isEmpty())
            {
                globalConsole->print("COMMAND: {}\n", job.command.view());
            }
            if (not job.rebuildReason.isEmpty())
            {
                globalConsole->print("REASON: {}\n", job.rebuildReason.view());
            }
        }
        SC_TRY(printCapturedStream(job.stdOut.view(), false));
        SC_TRY(printCapturedStream(job.stdErr.view(), true));
        return Result(true);
    }
};
} // namespace Build
} // namespace SC
