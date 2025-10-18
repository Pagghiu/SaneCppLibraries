// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../File/File.h"
#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Internal/IGrowableBuffer.h"
#include "../Foundation/StringPath.h"

namespace SC
{
struct SC_COMPILER_EXPORT ProcessChain;

struct SC_COMPILER_EXPORT ProcessDescriptor
{
    using Handle                  = detail::FileDescriptorDefinition::Handle;
    static constexpr auto Invalid = detail::FileDescriptorDefinition::Invalid;
};

/// @brief Wraps the code returned by a process that has exited
struct ProcessExitStatus
{
    int32_t status = -1;
};

//! @defgroup group_process Process
//! @copybrief library_process (see @ref library_process for more details)

//! @addtogroup group_process
//! @{

/// @brief Native os handle to a process identifier
struct ProcessID
{
    int32_t pid = 0;
};

/// @brief Execute a child process with standard file descriptors redirection.
/// @n
/// Features:
///
/// - Redirect standard in/out/err of a child process to a Pipe
/// - Inherit child process file descriptors from parent process
/// - Ignore (silence) child process standard file descriptor
/// - Wait for the child process exit code
///
/// Example: execute child process (launch and wait for it to fully execute)
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessSnippet1
///
/// Example: execute child process, redirecting stdout to a string
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessSnippet2
///
/// Example: launch a child process and wait for it to finish execution
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessSnippet3
///
/// Example: execute child process, filling its stdin with a StringSpan
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessSnippet4
///
/// Example: read process output using a pipe, using launch + waitForExitSync
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessSnippet5
///
/// Example: Add an environment variable
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessEnvironmentNewVar
///
/// Example: Redefine an environment variable
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessEnvironmentRedefine
///
/// Example: Disable environment variable inheritance
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessEnvironmentDisableInheritance

struct SC_COMPILER_EXPORT Process
{
    struct SC_COMPILER_EXPORT Options
    {
        bool windowsHide; ///< [Windows] Hides child process window (default == Process::isWindowsConsoleSubsystem)
        Options();
    };

    struct StdStream
    {
        /// @brief Read the process standard output/error into the given String / Buffer
        template <typename T>
        StdStream(T& destination)
        {
            GrowableBuffer<T>& buf = growableBufferStorage.reinterpret_as<GrowableBuffer<T>>();
            placementNew(buf, destination);
            growableBuffer = &buf;
            operation      = Operation::GrowableBuffer;
        }

        ~StdStream()
        {
            if (growableBuffer)
            {
                growableBuffer->~IGrowableBuffer();
            }
        }

        /// @brief Redirects child process standard output/error to a given file descriptor
        StdStream(FileDescriptor&& file)
        {
            operation = Operation::FileDescriptor;
            (void)file.get(fileDescriptor, Result::Error("Invalid redirection file descriptor"));
            file.detach();
        }

        StdStream(PipeDescriptor& pipe)
        {
            operation      = Operation::ExternalPipe;
            pipeDescriptor = &pipe;
        }

        StdStream(const StdStream&)            = delete;
        StdStream(StdStream&&)                 = delete;
        StdStream& operator=(const StdStream&) = delete;
        StdStream& operator=(StdStream&&)      = delete;

      protected:
        struct AlreadySetup
        {
        };
        StdStream() = default;
        StdStream(AlreadySetup) { operation = Operation::AlreadySetup; }
        friend struct Process;
        friend struct ProcessChain;

        enum class Operation
        {
            AlreadySetup,
            Inherit,
            Ignore,
            ExternalPipe,
            FileDescriptor,
            GrowableBuffer,
            WritableSpan,
            ReadableSpan
        };
        Operation operation = Operation::Inherit;

        Span<const char> readableSpan;
        Span<char>*      writableSpan = nullptr;

        IGrowableBuffer*                  growableBuffer = nullptr;
        AlignedStorage<6 * sizeof(void*)> growableBufferStorage;

        FileDescriptor::Handle fileDescriptor;

        PipeDescriptor* pipeDescriptor;
    };

    struct StdOut : public StdStream
    {
        // clang-format off
        struct Ignore{};
        struct Inherit{};
        
        /// @brief Ignores child process standard output/error (child process output will be silenced)
        StdOut(Ignore) { operation = Operation::Ignore; }
        
        /// @brief Inherits child process standard output/error (child process will print into parent process console)
        StdOut(Inherit) { operation = Operation::Inherit; }
        
        /// @brief Read the process standard output/error into the given Span
        StdOut(Span<char>& span) { operation = Operation::WritableSpan; writableSpan = &span; }
        
        using StdStream::StdStream;
        friend struct ProcessChain;
        // clang-format on
    };

    using StdErr = StdOut;

    struct StdIn : public StdStream
    {
        // clang-format off
        struct Inherit{};
        
        /// @brief Inherits child process Input from parent process
        StdIn(Inherit) { operation = Operation::Inherit; }
        
        /// @brief Fills standard input with content of a C-String
        template <int N> StdIn(const char (&item)[N]) : StdIn(StringSpan({item, N - 1}, true, StringEncoding::Ascii)) {}
        
        /// @brief Fills standard input with content of a StringSpan
        StdIn(StringSpan string) : StdIn(string.toCharSpan()) {}
        
        /// @brief Fills standard input with content of a Span
        StdIn(Span<const char> span) { operation = Operation::ReadableSpan; readableSpan = span;}
        
        using StdStream::StdStream;
        friend struct ProcessChain;
        // clang-format on
    };

    ProcessID processID; ///< ID of the process (can be the same as handle on Posix)
    Options   options;   ///< Options for the child process (hide console window etc.)

    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;

    /// @brief Waits (blocking) for process to exit after launch. It can only be called if Process::launch succeeded.
    Result waitForExitSync();

    /// @brief Launch child process with the given arguments
    /// @param cmd Process executable path and its arguments (if any)
    /// @param stdOut Process::StdOut::Ignore{}, Process::StdOut::Inherit{} or redirect stdout to String/Vector/Span
    /// @param stdIn Process::StdIn::Ignore{}, Process::StdIn::Inherit{} or feed stdin from
    /// StringSpan/String/Vector/Span
    /// @param stdErr Process::StdErr::Ignore{}, Process::StdErr::Inherit{} or redirect stderr to String/Vector/Span
    /// @returns Error if the requested executable doesn't exist / is not accessible / it cannot be executed
    Result launch(Span<const StringSpan> cmd,                        //
                  const StdOut&          stdOut = StdOut::Inherit{}, //
                  const StdIn&           stdIn  = StdIn::Inherit{},  //
                  const StdErr&          stdErr = StdErr::Inherit{})
    {
        SC_TRY(formatArguments(cmd));
        return launch(stdOut, stdIn, stdErr);
    }

    /// @brief Executes a  child process with the given arguments, waiting (blocking) until it's fully finished
    /// @param cmd Process executable path and its arguments (if any)
    /// @param stdOut Process::StdOut::Ignore{}, Process::StdOut::Inherit{} or redirect stdout to String/Vector/Span
    /// @param stdIn Process::StdIn::Ignore{}, Process::StdIn::Inherit{} or feed stdin from
    /// StringSpan/String/Vector/Span
    /// @param stdErr Process::StdErr::Ignore{}, Process::StdErr::Inherit{} or redirect stderr to String/Vector/Span
    /// @returns Error if the requested executable doesn't exist / is not accessible / it cannot be executed
    Result exec(Span<const StringSpan> cmd,                        //
                const StdOut&          stdOut = StdOut::Inherit{}, //
                const StdIn&           stdIn  = StdIn::Inherit{},  //
                const StdErr&          stdErr = StdErr::Inherit{})
    {
        SC_TRY(launch(cmd, stdOut, stdIn, stdErr));
        return waitForExitSync();
    }

    /// @brief gets the return code from the exited child process (valid only after exec or waitForExitSync)
    int32_t getExitStatus() const { return exitStatus.status; }

    /// @brief Sets the starting working directory of the process that will be launched / executed
    Result setWorkingDirectory(StringSpan processWorkingDirectory);

    /// @brief Controls if the newly spawned child process will inherit parent process environment variables
    void inheritParentEnvironmentVariables(bool inherit) { inheritEnv = inherit; }

    /// @brief Sets the environment variable for the newly spawned child process
    Result setEnvironment(StringSpan environmentVariable, StringSpan value);

    /// @brief Returns number of (virtual) processors available
    [[nodiscard]] static size_t getNumberOfProcessors();

    /// @brief Returns true only under Windows if executable is compiled with `/SUBSYSTEM:Console`
    [[nodiscard]] static bool isWindowsConsoleSubsystem();

    /// @brief Returns true if we're emulating x64 on ARM64 or the inverse on Windows
    [[nodiscard]] static bool isWindowsEmulatedProcess();

    /// @brief Constructs a Process object passing (optional) memory storage for command and environment variables.
    /// @param commandMemory Memory storage for command and arguments. If empty, a default storage will be used.
    /// @param environmentMemory Memory storage for environment variables. If empty, a default storage will be used.
    Process(Span<native_char_t> commandMemory = {}, Span<native_char_t> environmentMemory = {})
        : command({commandMemory}), environment({environmentMemory})
    {
        if (commandMemory.empty())
            command = {commandStorage};
        if (environmentMemory.empty())
            environment = {environmentStorage};
    }

  private:
    ProcessExitStatus exitStatus; ///< Exit status code returned after process is finished

    FileDescriptor stdInFd;  ///< Descriptor of process stdin
    FileDescriptor stdOutFd; ///< Descriptor of process stdout
    FileDescriptor stdErrFd; ///< Descriptor of process stderr

    Result launch(const StdOut& stdOutput, const StdIn& stdInput, const StdErr& stdError);

    Result formatArguments(Span<const StringSpan> cmd);

    StringPath currentDirectory;

    // On Windows command holds the concatenation of executable and arguments.
    // On Posix command holds the concatenation of executable and arguments SEPARATED BY null-terminators (\0).
    // This is done so that in this single buffer with no allocation (under 255) or a single allocation (above 255)
    // we can track all arguments to be passed to execve.
    native_char_t              commandStorage[StringPath::MaxPath + 1024];
    StringSpan::NativeWritable command;
#if !SC_PLATFORM_WINDOWS // On Posix we need to track the "sub-strings" hidden in command
    static constexpr size_t MAX_NUM_ARGUMENTS = 64;
    size_t commandArgumentsByteOffset[MAX_NUM_ARGUMENTS]; // Tracking length of each argument in the command string
    size_t commandArgumentsNumber = 0;                    // Counts number of arguments (including executable name)
#endif

    native_char_t              environmentStorage[4096 * 4]; // 16K (w)chars of storage for environment variables
    StringSpan::NativeWritable environment;

    static constexpr size_t MAX_NUM_ENVIRONMENT = 256;

    size_t environmentByteOffset[MAX_NUM_ENVIRONMENT]; // Tracking length of each environment variable
    size_t environmentNumber = 0;                      // Counts number of environment variable

    bool inheritEnv = true;

    friend struct ProcessChain;
    ProcessChain* parent = nullptr;

    Process* next = nullptr;
    Process* prev = nullptr;
    struct Internal;
    struct InternalFork;
    friend struct ProcessFork;
    Result launchImplementation();
    Result launchForkChild(PipeDescriptor& pipe);
    Result launchForkParent(PipeDescriptor& pipe, const void* previousSignals);
};

/// @brief Execute multiple child processes chaining input / output between them.
/// @n
/// Chains multiple child processes together, so that the output of a process becomes input of another (similar to what
/// happens wit the pipe (`|`) operator on Posix shells).
///
/// SC::PipeDescriptor from [File](@ref library_file) library is used to chain read / write endpoints of different
/// processes together.
///
/// Example: Inherit stdout file descriptor
/// \snippet Tests/Libraries/Process/ProcessTest.cpp processChainInheritDualSnippet
///
/// Example: Read stderr and stdout into a string
/// \snippet Tests/Libraries/Process/ProcessTest.cpp processChainPipeSingleSnippet
///
/// Example: Read standard output into a string using a Pipe
/// \snippet Tests/Libraries/Process/ProcessTest.cpp processChainPipeDualSnippet
struct SC_COMPILER_EXPORT ProcessChain
{
    Process::Options options;
    /// @brief Add a process to the chain, with given arguments
    /// @param process A non-launched Process object (allocated by caller, must be alive until waitForExitSync)
    /// @param cmd Path to executable and eventual args for this process
    /// @return Invalid result if given process failed to create pipes for I/O redirection
    Result pipe(Process& process, const Span<const StringSpan> cmd);

    /// @brief Launch the entire chain of processes. Reading from pipes can be done after launching.
    /// You can then call ProcessChain::waitForExitSync to block until the child process is fully finished.
    /// @return Valid result if given process chain has been launched successfully
    Result launch(const Process::StdOut& stdOut = Process::StdOut::Inherit{}, //
                  const Process::StdIn&  stdIn  = Process::StdIn::Inherit{},  //
                  const Process::StdErr& stdErr = Process::StdErr::Inherit{});

    /// @brief Waits (blocking) for entire process chain to exit. Can be called only after ProcessChain::launch.
    /// @return Valid result if the given process chain exited normally without aborting
    Result waitForExitSync();

    /// @brief Launch the entire chain of processes and waits for the results (calling ProcessChain::waitForExitSync)
    /// @return Valid result if given process chain has been launched and waited for exit successfully
    Result exec(const Process::StdOut& stdOut = Process::StdOut::Inherit{}, //
                const Process::StdIn&  stdIn  = Process::StdIn::Inherit{},  //
                const Process::StdErr& stdErr = Process::StdErr::Inherit{})
    {
        SC_TRY(launch(stdOut, stdIn, stdErr));
        return waitForExitSync();
    }

  private:
    // Trimmed duplicate of IntrusiveDoubleLinkedList<T>
    struct ProcessLinkedList
    {
        Process* back  = nullptr; // has no next
        Process* front = nullptr; // has no prev

        [[nodiscard]] bool isEmpty() const { return front == nullptr; }

        void clear();
        void queueBack(Process& process);
    };
    ProcessLinkedList processes;
};

/// @brief Reads current process environment variables
///
/// Example: Print all environment variables to stdout
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessEnvironmentPrint
struct ProcessEnvironment
{
    ProcessEnvironment();
    ~ProcessEnvironment();

    ProcessEnvironment(const ProcessEnvironment&)            = delete;
    ProcessEnvironment(ProcessEnvironment&&)                 = delete;
    ProcessEnvironment& operator=(const ProcessEnvironment&) = delete;
    ProcessEnvironment& operator=(ProcessEnvironment&&)      = delete;

    /// @brief Returns the total number of environment variables for current process
    [[nodiscard]] size_t size() const { return numberOfEnvironment; }

    /// @brief Get the environment variable at given index, returning its name and value
    /// @param index The index of the variable to retrieve (must be less than ProcessEnvironment::size())
    /// @param name The parsed name of the environment variable at requested index
    /// @param value The parsed value of the environment variable at requested index
    [[nodiscard]] bool get(size_t index, StringSpan& name, StringSpan& value) const;

    /// @brief Checks if an environment variable exists in current process
    /// @param variableName Name of the variable to check
    /// @param index Optional pointer to a variable that will receive the index of the variable (only if found)
    /// @returns true if variableName has been found in list of the variable
    [[nodiscard]] bool contains(StringSpan variableName, size_t* index = nullptr);

  private:
    size_t numberOfEnvironment = 0;
#if SC_PLATFORM_WINDOWS
    static constexpr size_t MAX_ENVIRONMENTS = 256;

    StringSpan envStrings[MAX_ENVIRONMENTS];
    wchar_t*   environment = nullptr;
#else
    char** environment = nullptr;
#endif
};

/// @brief Forks current process exiting child at end of process
/// A _fork_ duplicates a _parent_ process execution state, os handles and private memory.
///
/// Its semantics are quite different from platform to platform but on its most common denominator
/// it can be used to carry on "background" operations on snapshots of current program memory.
/// One relevant use case is serializing to disk or network a live, complex and large data structure.
/// Without the fork the program should either:
///
/// 1. Duplicate all the data, to snapshot it in a given instant, and keep it around for Async IO
/// 2. Block program execution and write the live data-structure until all IO is finished
///
/// Fork avoids memory duplication because it will be shared through Copy On Write (COW) mechanisms.
/// COW ensures that un-modified duplicated memory pages will not occupy additional Physical RAM.
///
/// A pair of pipes makes it easy to do some coordination between parent and forked process.
///
/// @warning There are really MANY caveats when forking that one should be aware of:
/// 1. Many API will just not work as expected on the forked process, especially on Windows
/// 2. Limit API calls in forked process to console IO, network and file I/O (avoid GUI / Graphics)
/// 3. All threads other than the current one will be suspended in child process (beware of deadlocks)
/// 4. Create Sockets and FileDescriptors with Inheritable flags if you need them in fork process
/// 5. Process deadlocks under Windows ARM64 / x86 emulation (use Process::IsWindowsEmulatedProcess)
///
/// Example: Fork current process modifying memory in forked process leaving parent's one unmodified.
/// \snippet Tests/Libraries/Process/ProcessTest.cpp ProcessFork
struct SC_COMPILER_EXPORT ProcessFork
{
    ProcessFork();
    ~ProcessFork();
    ProcessFork(const ProcessFork&)            = delete;
    ProcessFork* operator=(const ProcessFork&) = delete;

    enum Side
    {
        ForkParent, ///< Parent side of the fork
        ForkChild,  ///< Child side of the fork
    };

    /// @brief Obtain process parent / fork side
    [[nodiscard]] Side getSide() const { return side; }

    enum State
    {
        Suspended, ///< Start the forked process suspended (resume it with ProcessFork::resumeChildFork)
        Immediate, ///< Start the forked process immediately
    };

    /// @brief Forks current process (use ForkProcess::getType to know the side)
    Result fork(State state);

    /// @brief Sends 1 byte on parentToFork to resume State::Paused child fork
    Result resumeChildFork();

    /// @brief Waits for child fork to finish execution
    Result waitForChild();

    /// @brief Gets the return code from the exited child fork
    int32_t getExitStatus() const { return exitStatus.status; }

    /// @brief Gets the descriptor to "write" something to the other side
    FileDescriptor& getWritePipe();

    /// @brief Gets the descriptor to "read" something from the other side
    FileDescriptor& getReadPipe();

  private:
    Side side = ForkParent;
#if SC_PLATFORM_WINDOWS
    ProcessDescriptor::Handle processHandle = ProcessDescriptor::Invalid;
    ProcessDescriptor::Handle threadHandle  = ProcessDescriptor::Invalid;
#else
    ProcessID processID;
#endif
    ProcessExitStatus exitStatus; ///< Exit status code returned after child fork exits

    PipeDescriptor parentToFork; ///< Pipe to write from parent and read in fork
    PipeDescriptor forkToParent; ///< Pipe to write from fork and read in parent
};
//! @}

} // namespace SC
