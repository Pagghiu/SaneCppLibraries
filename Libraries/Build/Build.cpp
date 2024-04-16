// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Build.h"

#include "../Process/Process.h" // for Actions::compile

namespace SC
{
namespace Build
{
/// @brief Writes all project files for a given Definition with some Parameters using the provided DefinitionCompiler
struct ProjectWriter
{
    const Definition&         definition;
    const DefinitionCompiler& definitionCompiler;
    const Parameters&         parameters;

    ProjectWriter(const Definition& definition, const DefinitionCompiler& definitionCompiler,
                  const Parameters& parameters)
        : definition(definition), definitionCompiler(definitionCompiler), parameters(parameters)
    {}

    /// @brief Write the Definition outputs at the destinationDirectory, with filename
    [[nodiscard]] bool write(StringView destinationDirectory, StringView filename, StringView projectSubdirectory);

  private:
    struct WriterXCode;
    struct WriterVisualStudio;
    struct WriterMakefile;
};
} // namespace Build
} // namespace SC
#include "Internal/BuildWriterMakefile.inl"
#include "Internal/BuildWriterVisualStudio.inl"
#include "Internal/BuildWriterXCode.inl"

#include "../Containers/SmallVector.h"

bool SC::Build::Project::setRootDirectory(StringView file)
{
    SmallVector<StringView, 256> components;
    return Path::normalize(file, components, &rootDirectory, Path::AsPosix);
}

bool SC::Build::Project::addPresetConfiguration(Configuration::Preset preset, StringView configurationName)
{
    Configuration configuration;
    if (configurationName.isEmpty())
    {
        SC_TRY(configuration.name.assign(Configuration::PresetToString(preset)));
    }
    else
    {
        SC_TRY(configuration.name.assign(configurationName));
    }
    SC_TRY(configuration.applyPreset(preset));
    return configurations.push_back(configuration);
}

SC::Build::Configuration* SC::Build::Project::getConfiguration(StringView configurationName)
{
    size_t index;
    if (configurations.find([=](auto it) { return it.name == configurationName; }, &index))
    {
        return &configurations[index];
    }
    return nullptr;
}
const SC::Build::Configuration* SC::Build::Project::getConfiguration(StringView configurationName) const
{
    size_t index;
    if (configurations.find([=](auto it) { return it.name == configurationName; }, &index))
    {
        return &configurations[index];
    }
    return nullptr;
}

bool SC::Build::Project::addDirectory(StringView subdirectory, StringView filter)
{
    if (subdirectory.containsCodePoint('*') or subdirectory.containsCodePoint('?'))
        return false;
    return files.push_back({Project::File::Add, subdirectory, filter});
}

bool SC::Build::Project::addFile(StringView singleFile)
{
    if (singleFile.containsCodePoint('*') or singleFile.containsCodePoint('?'))
        return false;
    return files.push_back({Project::File::Add, {}, singleFile});
}

bool SC::Build::Project::removeFiles(StringView subdirectory, StringView filter)
{
    if (subdirectory.containsCodePoint('*') or subdirectory.containsCodePoint('?'))
        return false;
    return files.push_back({Project::File::Remove, subdirectory, filter});
}

SC::Result SC::Build::Project::validate() const
{
    SC_TRY_MSG(not name.isEmpty(), "Project needs name");
    return Result(true);
}

SC::Result SC::Build::Workspace::validate() const
{
    for (const auto& project : projects)
    {
        SC_TRY(project.validate());
    }
    return Result(true);
}

SC::Result SC::Build::Definition::configure(StringView projectFileName, const Build::Parameters& parameters,
                                            StringView rootPath) const
{
    Build::DefinitionCompiler definitionCompiler(*this);
    SC_TRY(definitionCompiler.validate());
    SC_TRY(definitionCompiler.build());
    Build::ProjectWriter writer(*this, definitionCompiler, parameters);
    return Result(writer.write(rootPath, projectFileName, Generator::toString(parameters.generator)));
}

SC::Result SC::Build::DefinitionCompiler::validate()
{
    for (const auto& workspace : definition.workspaces)
    {
        SC_TRY(workspace.validate());
    }
    return Result(true);
}

SC::Result SC::Build::DefinitionCompiler::build()
{
    VectorMap<String, VectorSet<Project::File>> uniquePaths;
    SC_TRY(collectUniqueRootPaths(uniquePaths));
    for (auto& it : uniquePaths)
    {
        SC_TRY(fillPathsList(it.key.view(), it.value, resolvedPaths));
    }
    return Result(true);
}

SC::Result SC::Build::DefinitionCompiler::fillPathsList(StringView path, const VectorSet<Project::File>& filters,
                                                        VectorMap<String, Vector<String>>& filtersToFiles)
{
    bool doRecurse = false;
    for (const auto& it : filters)
    {
        if (it.mask.view().containsCodePoint('/'))
        {
            doRecurse = true;
            break;
        }
        if (it.mask.view().containsString("**"))
        {
            doRecurse = true;
            break;
        }
    }

    if (filters.size() == 1 and FileSystem().existsAndIsFile(path))
    {
        SC_TRY(filtersToFiles.getOrCreate(path)->push_back(path));
        return Result(true);
    }

    Vector<Project::File> renderedFilters;
    for (const auto& filter : filters)
    {
        Project::File file;
        file.operation = filter.operation;
        SC_TRY(file.mask.assign(path));
        SC_TRY(Path::append(file.mask, {filter.mask.view()}, Path::AsPosix));
        SC_TRY(renderedFilters.push_back(move(file)));
    }

    FileSystemIterator fsIterator;
    fsIterator.options.forwardSlashes = true;
    SC_TRY(fsIterator.init(path));

    while (fsIterator.enumerateNext())
    {
        auto& item = fsIterator.get();
        if (doRecurse and item.isDirectory())
        {
            // TODO: Check if it's possible to optimize entire subdirectory out in some cases
            SC_TRY(fsIterator.recurseSubdirectory());
        }
        else
        {
            for (const auto& filter : renderedFilters)
            {
                if (StringAlgorithms::matchWildcard(filter.mask.view(), item.path))
                {
                    SC_TRY(filtersToFiles.getOrCreate(filter.mask)->push_back(item.path));
                }
            }
        }
    }
    return fsIterator.checkErrors();
}

// Collects root paths to build a stat map
SC::Result SC::Build::DefinitionCompiler::collectUniqueRootPaths(VectorMap<String, VectorSet<Project::File>>& paths)
{
    String buffer;
    for (const Workspace& workspace : definition.workspaces)
    {
        for (const Project& project : workspace.projects)
        {
            for (const Project::File& file : project.files)
            {
                SC_TRY(buffer.assign(project.rootDirectory.view()));
                if (file.base.view().isEmpty())
                {
                    if (not file.mask.isEmpty())
                    {
                        SC_TRY(Path::append(buffer, file.mask.view(), Path::AsPosix));
                        auto* value = paths.getOrCreate(buffer);
                        SC_TRY(value != nullptr and value->insert(file));
                    }
                    continue;
                }
                SC_TRY(Path::append(buffer, file.base.view(), Path::AsPosix));
                // Some example cases:
                // 1. /SC/Tests/SCTest
                // 2. /SC/Libraries
                // 3. /SC/Libraries/UserInterface
                // 4. /SC/Libraries
                // 5. /SC/LibrariesASD

                bool shouldInsert = true;
                for (auto& it : paths)
                {
                    size_t commonOverlap = 0;
                    if (it.key.view().fullyOverlaps(buffer.view(), commonOverlap))
                    {
                        // they are the same (Case 4. after 2. has been inserted)
                        SC_TRY(it.value.insert(file));
                        shouldInsert = false;
                        break;
                    }
                    else
                    {
                        const auto overlapNew      = buffer.view().sliceStart(commonOverlap);
                        const auto overlapExisting = it.key.view().sliceStart(commonOverlap);
                        if (overlapExisting.isEmpty())
                        {
                            // Case .5 and .3 after .2
                            if (overlapNew.startsWithAnyOf({'/'}))
                            {
                                // Case .3 after 2 (can be merged)
                                Project::File mergedFile;
                                mergedFile.operation = file.operation;
                                SC_TRY(mergedFile.base.assign(it.value.begin()->base.view()));
                                SC_TRY(mergedFile.mask.assign(Path::removeStartingSeparator(overlapNew)));
                                SC_TRY(Path::append(mergedFile.mask, {file.mask.view()}, Path::AsPosix));
                                SC_TRY(it.value.insert(mergedFile));
                                shouldInsert = false;
                                break;
                            }
                        }
                    }
                }
                if (shouldInsert)
                {
                    auto* value = paths.getOrCreate(buffer);
                    SC_TRY(value != nullptr and value->insert(file));
                }
            }
        }
    }
    return Result(true);
}

bool SC::Build::ProjectWriter::write(StringView destinationDirectory, StringView filename,
                                     StringView projectSubdirectory)
{
    String normalizedDirectory = StringEncoding::Utf8;
    {
        Vector<StringView> components;
        SC_TRY(Path::normalize(destinationDirectory, components, &normalizedDirectory, Path::Type::AsPosix));
        SC_TRY(Path::append(normalizedDirectory, {projectSubdirectory}, Path::Type::AsPosix));
    }
    FileSystem fs;
    SC_TRY(Path::isAbsolute(destinationDirectory, Path::AsNative));
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(normalizedDirectory.view()));
    SC_TRY(fs.init(normalizedDirectory.view()));

    String prjName;
    switch (parameters.generator)
    {
    case Generator::XCode: {
        String                buffer;
        WriterXCode           writer(definition, definitionCompiler);
        const Project&        project = definition.workspaces[0].projects[0];
        WriterXCode::Renderer renderer;
        SC_TRY(writer.prepare(normalizedDirectory.view(), project, renderer));
        {
            StringBuilder sb(buffer, StringBuilder::Clear);
            SC_TRY(writer.writeProject(sb, project, renderer));
            SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj", filename));
            SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
            SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj/project.pbxproj", filename));
            SC_TRY(fs.removeFileIfExists(prjName.view()));
            SC_TRY(fs.writeString(prjName.view(), buffer.view()));
        }
        {
            StringBuilder sb(buffer, StringBuilder::Clear);
            SC_TRY(writer.writeScheme(sb, project, renderer, normalizedDirectory.view(), filename));
            SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj/xcshareddata", filename));
            SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
            SC_TRY(
                StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj/xcshareddata/xcschemes", filename));
            SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
            SC_TRY(StringBuilder(prjName, StringBuilder::Clear)
                       .format("{}.xcodeproj/xcshareddata/xcschemes/{}.xcscheme", filename, filename));
            SC_TRY(fs.removeFileIfExists(prjName.view()));
            SC_TRY(fs.writeString(prjName.view(), buffer.view()));
        }
        break;
    }
    case Generator::VisualStudio2022: {
        String buffer1;
        SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.vcxproj", filename));
        WriterVisualStudio           writer(definition, definitionCompiler);
        WriterVisualStudio::Renderer renderer;
        const Project&               project = definition.workspaces[0].projects[0];
        SC_TRY(writer.prepare(normalizedDirectory.view(), project, renderer));
        SC_TRY(writer.generateGuidFor(project.name.view(), writer.hashing, writer.projectGuid));
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY(writer.writeProject(sb1, project, renderer));
            SC_TRY(fs.removeFileIfExists(prjName.view()));
            SC_TRY(fs.writeString(prjName.view(), buffer1.view()));
        }
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY(writer.writeFilters(sb1, renderer));
            String prjFilterName;
            SC_TRY(StringBuilder(prjFilterName, StringBuilder::Clear).format("{}.vcxproj.filters", filename));
            SC_TRY(fs.removeFileIfExists(prjFilterName.view()));
            SC_TRY(fs.writeString(prjFilterName.view(), buffer1.view()));
        }
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY(writer.writeSolution(sb1, prjName.view(), project));
            String slnName;
            SC_TRY(StringBuilder(slnName, StringBuilder::Clear).format("{}.sln", filename));
            SC_TRY(fs.removeFileIfExists(slnName.view()));
            SC_TRY(fs.writeString(slnName.view(), buffer1.view()));
        }
        break;
    }
    case Generator::Make: {
        String buffer1;
        SC_TRY(prjName.assign("Makefile"));
        WriterMakefile           writer(definition, definitionCompiler);
        WriterMakefile::Renderer renderer;
        StringBuilder            sb1(buffer1, StringBuilder::Clear);
        SC_TRY(writer.writeMakefile(sb1, normalizedDirectory.view(), definition.workspaces[0], renderer));
        SC_TRY(fs.removeFileIfExists(prjName.view()));
        SC_TRY(fs.writeString(prjName.view(), buffer1.view()));
        break;
    }
    }
    return Result(true);
}

struct SC::Build::Action::Internal
{
    static Build::Parameters fillDefaultParameters(Generator::Type generator);

    static Result configure(ConfigureFunction configure, StringView projectFileName, const Action& action);
    static Result coverage(StringView projectFileName, const Action& action);
    static Result executeInternal(StringView projectFileName, const Action& action, String* outputExecutable = nullptr);

    [[nodiscard]] static Result toVisualStudioArchitecture(Architecture::Type architectureType,
                                                           StringView&        architecture)
    {
        switch (architectureType)
        {
        case Architecture::Intel32: architecture = "x86"; break;
        case Architecture::Intel64: architecture = "x64"; break;
        case Architecture::Arm64: architecture = "ARM64"; break;
        case Architecture::Any: break;

        case Architecture::Wasm: // Unsupported
            return Result::Error("Unsupported architecture for Visual Studio");
        }
        return Result(true);
    }

    [[nodiscard]] static Result toXCodeArchitecture(Architecture::Type architectureType, StringView& architecture)
    {
        switch (architectureType)
        {
        case Architecture::Intel64: architecture = "x86_64"; break;
        case Architecture::Arm64: architecture = "arm64"; break;
        case Architecture::Any: architecture = "arm64 x86_64"; break;
        case Architecture::Intel32: // Unsupported
        case Architecture::Wasm:    // Unsupported
            return Result::Error("Unsupported architecture for XCode");
        }
        return Result(true);
    }

    [[nodiscard]] static Result toMakefileArchitecture(Architecture::Type architectureType, StringView& architecture)
    {
        switch (architectureType)
        {
        case Architecture::Intel64: architecture = "TARGET_ARCHITECTURE=x86_64"; break;
        case Architecture::Arm64: architecture = "TARGET_ARCHITECTURE=arm64"; break;
        case Architecture::Any: break;
        case Architecture::Intel32: // Unsupported
        case Architecture::Wasm:    // Unsupported
            return Result::Error("Unsupported architecture for make");
        }
        return Result(true);
    }
};

SC::Result SC::Build::Action::execute(const Action& action, ConfigureFunction configure, StringView projectName)
{
    switch (action.action)
    {
    case Print:
    case Run:
    case Compile: return Internal::executeInternal(projectName, action);
    case Coverage: return Internal::coverage(projectName, action);
    case Configure: return Internal::configure(configure, projectName, action);
    }
    return Result::Error("Action::execute - unsupported action");
}

SC::Build::Parameters SC::Build::Action::Internal::fillDefaultParameters(Generator::Type generator)
{
    switch (generator)
    {
    case Generator::VisualStudio2022: {
        Build::Parameters parameters;
        parameters.generator     = Generator::VisualStudio2022;
        parameters.platforms     = {Platform::Windows};
        parameters.architectures = {Architecture::Arm64, Architecture::Intel64};
        return parameters;
    }
    break;
    case Generator::XCode: {
        Build::Parameters parameters;
        parameters.generator     = Generator::XCode;
        parameters.platforms     = {Platform::MacOS};
        parameters.architectures = {Architecture::Arm64, Architecture::Intel64};
        return parameters;
    }
    break;

    case Generator::Make: {
        Build::Parameters parameters;
        parameters.generator     = Generator::Make;
        parameters.platforms     = {Platform::MacOS, Build::Platform::Linux};
        parameters.architectures = {Architecture::Arm64, Architecture::Intel64};
        return parameters;
    }
    break;
    }
    SC_ASSERT_RELEASE(false);
}

SC::Result SC::Build::Action::Internal::configure(ConfigureFunction configure, StringView projectFileName,
                                                  const Action& action)
{
    Build::Parameters parameters = fillDefaultParameters(action.generator);
    Build::Definition definition;
    SC_TRY(configure(definition, parameters, action.libraryDirectory));
    SC_TRY(definition.configure(projectFileName, parameters, action.targetDirectory));
    return Result(true);
}

SC::Result SC::Build::Action::Internal::coverage(StringView projectFileName, const Action& action)
{
    Action newAction = action;
    String executablePath;
    newAction.action = Action::Compile;

    // Build the configuration with coverage information
    SC_TRY(executeInternal(projectFileName, newAction));
    newAction.action = Action::Print;

    // Get coverage configuration executable path
    SC_TRY(executeInternal(projectFileName, newAction, &executablePath));
    String coverageDirectory;
    SC_TRY(Path::join(coverageDirectory, {action.targetDirectory, "..", "_Coverage"}));

    FileSystem fs;
    SC_TRY(fs.init(coverageDirectory.view()));

    // Recreate Coverage Dir
    if (fs.existsAndIsDirectory(coverageDirectory.view()))
    {
        SC_TRY(fs.removeDirectoryRecursive(coverageDirectory.view()));
    }
    SC_TRY(fs.makeDirectory(coverageDirectory.view()));

    // Execute process instrumented for coverage
    {
        Process process;
        SC_TRY(process.setEnvironment("LLVM_PROFILE_FILE", "profile.profraw"));
        SC_TRY(process.setWorkingDirectory(coverageDirectory.view()));
        SC_TRY(process.exec({executablePath.view()}));
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing instrumented executable");
    }

    // Merge coverage files
    StringView arguments[16];

    size_t numArguments  = 0;
    size_t baseArguments = 0;
    switch (HostPlatform)
    {
    case SC::Platform::Apple:
        arguments[numArguments++] = "xcrun"; // 1
        baseArguments             = 1;
        break;
    default: break;
    }
    {
        numArguments = baseArguments;
        Process process;
        SC_TRY(process.setWorkingDirectory(coverageDirectory.view()));
        arguments[numArguments++] = "llvm-profdata";    // 2
        arguments[numArguments++] = "merge";            // 3
        arguments[numArguments++] = "-sparse";          // 4
        arguments[numArguments++] = "profile.profraw";  // 5
        arguments[numArguments++] = "-o";               // 6
        arguments[numArguments++] = "profile.profdata"; // 7
        SC_TRY(process.exec({arguments, numArguments}));
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing llvm-profdata");
    }
    // Generate HTML excluding all tests and SC::Tools
    {
        numArguments = baseArguments;
        Process process;
        SC_TRY(process.setWorkingDirectory(coverageDirectory.view()));
        arguments[numArguments++] = "llvm-cov";                                                               // 2
        arguments[numArguments++] = "show";                                                                   // 3
        arguments[numArguments++] = "-format";                                                                // 4
        arguments[numArguments++] = "html";                                                                   // 5
        arguments[numArguments++] = "-ignore-filename-regex=='^(.*\\/SC-.*\\.*|.*\\/Tools.*|.*\\Test.cpp)$'"; // 6
        arguments[numArguments++] = "--output-dir";                                                           // 7
        arguments[numArguments++] = "coverage";                                                               // 8
        arguments[numArguments++] = "-instr-profile=profile.profdata";                                        // 9
        arguments[numArguments++] = executablePath.view();                                                    // 10
        SC_TRY(process.exec({arguments, numArguments}));
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing llvm-profdata");
    }

    return Result(true);
}

SC::Result SC::Build::Action::Internal::executeInternal(StringView projectFileName, const Action& action,
                                                        String* outputExecutable)
{
    StringView configuration = action.configuration.isEmpty() ? "Debug" : action.configuration;

    Build::Parameters parameters = fillDefaultParameters(action.generator);
    SmallString<256>  solutionLocation;

    Process process;
    switch (action.generator)
    {
    case Generator::XCode: {
        SC_TRY(Path::join(solutionLocation,
                          {action.targetDirectory, Generator::toString(action.generator), projectFileName}));
        SC_TRY(StringBuilder(solutionLocation, StringBuilder::DoNotClear).append(".xcodeproj"));
        StringView architecture;
        SC_TRY(toXCodeArchitecture(action.architecture, architecture));
        SmallString<32> formattedPlatform;
        SC_TRY(StringBuilder(formattedPlatform).format("ARCHS={}", architecture));

        StringView arguments[16];
        size_t     numArgs   = 0;
        arguments[numArgs++] = "xcodebuild"; // 1
        switch (action.action)
        {
        case Action::Compile:
            arguments[numArgs++] = "build"; // 2
            break;
        case Action::Run:
            arguments[numArgs++] = "-showBuildSettings"; // 2
            break;
        default: return Result::Error("Unexpected Build::Action (supported \"compile\", \"run\")");
        }
        arguments[numArgs++] = "-configuration";         // 3
        arguments[numArgs++] = configuration;            // 4
        arguments[numArgs++] = "-project";               // 5
        arguments[numArgs++] = solutionLocation.view();  // 6
        arguments[numArgs++] = "ONLY_ACTIVE_ARCH=NO";    // 7
        arguments[numArgs++] = formattedPlatform.view(); // 8
        if (action.action == Action::Run or action.action == Action::Print)
        {
            String output = StringEncoding::Utf8;
            SC_TRY(process.exec({arguments, numArgs}, output));
            SC_TRY_MSG(process.getExitStatus() == 0, "Run returned error");
            StringViewTokenizer tokenizer(output.view());
            StringView          path, targetName;
            while (tokenizer.tokenizeNextLine())
            {
                const StringView line = tokenizer.component.trimWhiteSpaces();
                if (line.startsWith("TARGET_BUILD_DIR = "))
                {
                    SC_TRY(line.splitAfter(" = ", path));
                    if (not targetName.isEmpty())
                        break;
                }
                if (line.startsWith("EXECUTABLE_NAME = "))
                {
                    SC_TRY(line.splitAfter(" = ", targetName));
                    if (not path.isEmpty())
                        break;
                }
            }

            if (path.isEmpty() or targetName.isEmpty())
            {
                return Result::Error("Cannot find TARGET_BUILD_DIR and EXECUTABLE_NAME");
            }
            String userExecutable;
            SC_TRY(Path::join(userExecutable, {path, "/", targetName}));
            if (action.action == Action::Run)
            {
                Process testProcess;
                SC_TRY(testProcess.exec({userExecutable.view()}));
                SC_TRY_MSG(testProcess.getExitStatus() == 0, "Run exited with non zero status");
            }
            else if (action.action == Action::Print)
            {
                if (outputExecutable)
                {
                    return Result(outputExecutable->assign(userExecutable.view()));
                }
            }
        }
        else
        {
            SC_TRY(process.exec({arguments, numArgs}));
            SC_TRY_MSG(process.getExitStatus() == 0, "Compile returned error");
        }
    }
    break;
    case Generator::VisualStudio2022: {
        SC_TRY(Path::join(solutionLocation,
                          {action.targetDirectory, Generator::toString(action.generator), projectFileName}));
        SC_TRY(StringBuilder(solutionLocation, StringBuilder::DoNotClear).append(".sln"));
        SmallString<32> platformConfiguration;
        SC_TRY(StringBuilder(platformConfiguration).format("/p:Configuration={}", configuration));

        StringView architecture;
        SC_TRY(Internal::toVisualStudioArchitecture(action.architecture, architecture));
        SmallString<32> platform;
        SC_TRY(StringBuilder(platform).format("/p:Platform={}", architecture));

        StringView arguments[16];
        size_t     numArgs   = 0;
        arguments[numArgs++] = "msbuild";                    // 1
        arguments[numArgs++] = solutionLocation.view();      // 2
        arguments[numArgs++] = platformConfiguration.view(); // 3
        if (not architecture.isEmpty())
        {
            arguments[numArgs++] = platform.view(); // 4
        }
        switch (action.action)
        {
        case Action::Compile:
            SC_TRY(process.exec({arguments, numArgs}));
            SC_TRY_MSG(process.getExitStatus() == 0, "Compile returned error");
            break;
        case Action::Print:
        case Action::Run: {
            String output = StringEncoding::Utf8; // TODO: Check encoding of Visual Studio Output.
            SC_TRY(process.exec({arguments, numArgs}, output));
            SC_TRY_MSG(process.getExitStatus() == 0, "Compile returned error");
            StringViewTokenizer tokenizer(output.view());
            StringView          executablePath;
            while (tokenizer.tokenizeNextLine())
            {
                if (tokenizer.component.splitAfter(".vcxproj -> ", executablePath))
                {
                    executablePath = executablePath.trimWhiteSpaces();
                    break;
                }
            }
            SC_TRY_MSG(not executablePath.isEmpty(), "Cannot find executable path from .vcxproj");
            if (action.action == Action::Run)
            {
                Process testProcess;
                SC_TRY(testProcess.exec({executablePath}));
                SC_TRY_MSG(testProcess.getExitStatus() == 0, "Run exited with non zero status");
            }
            else if (action.action == Action::Print)
            {
                if (outputExecutable)
                {
                    return Result(outputExecutable->assign(executablePath));
                }
            }
        }
        break;
        default: return Result::Error("Unexpected Build::Action (supported \"compile\", \"run\")");
        }
    }
    break;
    case Generator::Make: {
        SC_TRY(Path::join(solutionLocation, {action.targetDirectory, Generator::toString(action.generator)}));
        SmallString<32> platformConfiguration;
        SC_TRY(StringBuilder(platformConfiguration).format("CONFIG={}", configuration));

        StringView architecture;
        SC_TRY(toMakefileArchitecture(action.architecture, architecture));
        StringView arguments[16];
        size_t     numArgs   = 0;
        arguments[numArgs++] = "make"; // 1

        switch (action.action)
        {
        case Action::Compile: arguments[numArgs++] = "compile"; break;              // 2
        case Action::Run: arguments[numArgs++] = "run"; break;                      // 2
        case Action::Print: arguments[numArgs++] = "print-executable-paths"; break; // 2
        default: return Result::Error("Unexpected Build::Action (supported \"compile\", \"run\")");
        }
        arguments[numArgs++] = "-j";                         // 3
        arguments[numArgs++] = "-C";                         // 4
        arguments[numArgs++] = solutionLocation.view();      // 5
        arguments[numArgs++] = platformConfiguration.view(); // 6
        if (not architecture.isEmpty())
        {
            arguments[numArgs++] = architecture; // 7
        }
        SC_TRY(process.setEnvironment("GNUMAKEFLAGS", "--no-print-directory"));
        if (action.action == Action::Print)
        {
            SC_TRY(process.exec({arguments, numArgs}, *outputExecutable));
            *outputExecutable = outputExecutable->view().trimWhiteSpaces();
        }
        else
        {
            SC_TRY(process.exec({arguments, numArgs}));
            SC_TRY_MSG(process.getExitStatus() == 0, "Compile returned error");
        }
    }
    break;
    }
    return Result(true);
}
