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

    /// @brief Write the project file at given directories
    [[nodiscard]] bool write(StringView filename);

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

SC::Build::Configuration::Configuration()
{
    (void)outputPath.assign(getStandardBuildDirectory());
    (void)StringBuilder(intermediatesPath).format("$(PROJECT_NAME)/{}", getStandardBuildDirectory());
}

bool SC::Build::Configuration::applyPreset(Preset newPreset, const Parameters& parameters)
{
    preset = newPreset;
    switch (preset)
    {
    case Configuration::Preset::DebugCoverage:
        SC_TRY(compile.set<Compile::enableCoverage>(true));
        SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Debug));
        SC_TRY(compile.addDefines({"DEBUG=1"}));
        if (parameters.generator == Build::Generator::VisualStudio2022)
        {
            visualStudio.platformToolset = "ClangCL";
        }
        break;
    case Configuration::Preset::Debug:
        SC_TRY(compile.set<Compile::enableASAN>(true));
        SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Debug));
        SC_TRY(compile.addDefines({"DEBUG=1"}));
        break;
    case Configuration::Preset::Release:
        SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Release));
        SC_TRY(compile.addDefines({"NDEBUG=1"}));
        break;
    case Configuration::Preset::None: break;
    }
    return true;
}

bool SC::Build::Project::setRootDirectory(StringView file)
{
    SmallVector<StringView, 256> components;
    return Path::normalize(file, components, &rootDirectory, Path::AsPosix);
}

bool SC::Build::Project::addPresetConfiguration(Configuration::Preset preset, const Parameters& parameters,
                                                StringView configurationName)
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
    SC_TRY(configuration.applyPreset(preset, parameters));
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

SC::Result SC::Build::Definition::configure(StringView projectFileName, const Build::Parameters& parameters) const
{
    Build::DefinitionCompiler definitionCompiler(*this);
    SC_TRY(definitionCompiler.validate());
    SC_TRY(definitionCompiler.build());
    String projectGeneratorSubFolder = StringEncoding::Utf8;
    {
        Vector<StringView> components;
        SC_TRY(Path::normalize(parameters.directories.projectsDirectory.view(), components, &projectGeneratorSubFolder,
                               Path::Type::AsPosix));
        SC_TRY(
            Path::append(projectGeneratorSubFolder, {Generator::toString(parameters.generator)}, Path::Type::AsPosix));
    }
    Build::Parameters newParameters             = parameters;
    newParameters.directories.projectsDirectory = move(projectGeneratorSubFolder);
    Build::ProjectWriter writer(*this, definitionCompiler, newParameters);
    return Result(writer.write(projectFileName));
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
    String                      buffer;
    SmallVector<StringView, 16> components;
    for (const Workspace& workspace : definition.workspaces)
    {
        for (const Project& project : workspace.projects)
        {
            for (const Project::File& file : project.files)
            {
                SC_TRY(buffer.assign(project.rootDirectory.view()));
                if (Path::isAbsolute(file.base.view(), Path::Type::AsNative))
                {
                    Project::File absFile;
                    absFile.operation = file.operation;
                    SC_TRY(Path::normalize(file.base.view(), components, &absFile.base, Path::Type::AsPosix));
                    SC_TRY(absFile.mask.assign(file.mask.view()));
                    SC_TRY(paths.getOrCreate(absFile.base.view())->insert(absFile));
                    continue;
                }
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

bool SC::Build::ProjectWriter::write(StringView defaultProjectName)
{
    const Directories& directories = parameters.directories;
    (void)defaultProjectName;
    FileSystem fs;
    SC_TRY(Path::isAbsolute(directories.projectsDirectory.view(), Path::AsNative));
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(directories.projectsDirectory.view()));
    SC_TRY(fs.init(directories.projectsDirectory.view()));

    // TODO: Generate all projects for all workspaces
    const Workspace& workspace = definition.workspaces[0];

    String buffer;

    switch (parameters.generator)
    {
    case Generator::XCode: {
        String prjName;
        // Write all projects
        for (const Project& project : workspace.projects)
        {
            RelativeDirectories relativeDirectories;
            SC_TRY(relativeDirectories.computeRelativeDirectories(directories, Path::AsPosix, project,
                                                                  "$(PROJECT_DIR)/{}"));
            WriterXCode           writer(definition, definitionCompiler, directories, relativeDirectories);
            WriterXCode::Renderer renderer;
            StringView            projectName = project.name.view();
            SC_TRY(writer.prepare(project, renderer));
            {
                StringBuilder builder(buffer, StringBuilder::Clear);
                SC_TRY(writer.writeProject(builder, project, renderer));
                SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj", projectName));
                SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
                SC_TRY(
                    StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj/project.pbxproj", projectName));
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), buffer.view()));
            }
            {
                StringBuilder builder(buffer, StringBuilder::Clear);
                SC_TRY(writer.writeScheme(builder, project, renderer, projectName));
                SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj/xcshareddata", projectName));
                SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
                SC_TRY(StringBuilder(prjName, StringBuilder::Clear)
                           .format("{}.xcodeproj/xcshareddata/xcschemes", projectName));
                SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
                SC_TRY(StringBuilder(prjName, StringBuilder::Clear)
                           .format("{}.xcodeproj/xcshareddata/xcschemes/{}.xcscheme", projectName, projectName));
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), buffer.view()));
            }
        }
        break;
    }
    case Generator::VisualStudio2019:
    case Generator::VisualStudio2022: {

        Vector<String> projectsGuids;
        // Write all projects
        for (const Project& project : workspace.projects)
        {
            RelativeDirectories relativeDirectories;
            SC_TRY(relativeDirectories.computeRelativeDirectories(directories, Path::AsWindows, project,
                                                                  "$(ProjectDir){}"));
            WriterVisualStudio writer(definition, definitionCompiler, directories, relativeDirectories,
                                      parameters.generator);

            WriterVisualStudio::Renderer renderer;
            SC_TRY(writer.prepare(project, renderer));
            SC_TRY(writer.generateGuidFor(project.name.view(), writer.hashing, writer.projectGuid));
            {
                StringBuilder builder(buffer, StringBuilder::Clear);
                SC_TRY(writer.writeProject(builder, project, renderer));
                String prjName;
                SC_TRY(StringBuilder(prjName, StringBuilder::Clear).format("{}.vcxproj", project.name));
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), buffer.view()));
            }
            {
                StringBuilder builder(buffer, StringBuilder::Clear);
                SC_TRY(writer.writeFilters(builder, renderer));
                String prjFilterName;
                SC_TRY(StringBuilder(prjFilterName, StringBuilder::Clear).format("{}.vcxproj.filters", project.name));
                SC_TRY(fs.removeFileIfExists(prjFilterName.view()));
                SC_TRY(fs.writeString(prjFilterName.view(), buffer.view()));
            }
            SC_TRY(projectsGuids.push_back(writer.projectGuid));
            // Write solution
            {
                StringBuilder builder(buffer, StringBuilder::Clear);
                SC_TRY(WriterVisualStudio::writeSolution(builder, {project}, projectsGuids.toSpanConst()));
                String slnName;
                SC_TRY(StringBuilder(slnName, StringBuilder::Clear).format("{}.sln", project.name));
                SC_TRY(fs.removeFileIfExists(slnName.view()));
                SC_TRY(fs.writeString(slnName.view(), buffer.view()));
            }
        }
        break;
    }
    case Generator::Make: {
        String prjName;
        SC_TRY(StringBuilder(prjName).format("Makefile.{}", Platform::toString(parameters.platform)));
        WriterMakefile writer(definition, definitionCompiler, directories);

        WriterMakefile::Renderer renderer;
        {
            StringBuilder builder(buffer, StringBuilder::Clear);
            SC_TRY(writer.writeMakefile(builder, workspace, renderer));
            SC_TRY(fs.removeFileIfExists(prjName.view()));
            SC_TRY(fs.writeString(prjName.view(), buffer.view()));
        }
        break;
    }
    }
    return Result(true);
}

struct SC::Build::Action::Internal
{
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

SC::Result SC::Build::Action::Internal::configure(ConfigureFunction configure, StringView projectFileName,
                                                  const Action& action)
{
    Build::Definition definition;
    SC_TRY(configure(definition, action.parameters));
    SC_TRY(definition.configure(projectFileName, action.parameters));
    return Result(true);
}

SC::Result SC::Build::Action::Internal::coverage(StringView projectFileName, const Action& action)
{
    Action newAction = action;
    String executablePath;

    // Build the configuration with coverage information
    newAction.action = Action::Compile;
    SC_TRY(executeInternal(projectFileName, newAction));

    // Get coverage configuration executable path
    newAction.action = Action::Print;
    SC_TRY(executeInternal(projectFileName, newAction, &executablePath));
    String coverageDirectory;
    SC_TRY(Path::join(coverageDirectory, {action.parameters.directories.projectsDirectory.view(), "..", "_Coverage"}));

    {
        FileSystem fs;
        SC_TRY(fs.init(action.parameters.directories.projectsDirectory.view()));

        // Recreate Coverage Dir
        if (fs.existsAndIsDirectory(coverageDirectory.view()))
        {
            SC_TRY(fs.removeDirectoryRecursive(coverageDirectory.view()));
        }
        SC_TRY(fs.makeDirectory(coverageDirectory.view()));
    }
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
        arguments[numArguments++] = "llvm-cov"; // 2
        arguments[numArguments++] = "show";     // 3
        arguments[numArguments++] = "-format";  // 4
        arguments[numArguments++] = "html";     // 5
        // TODO: De-hardcode this filter and pass it as a parameter
        arguments[numArguments++] = "-ignore-filename-regex=" // 6
                                    "^(.*\\/SC-.*\\.*|.*\\/Tools.*|.*\\Test.(cpp|h|c)|.*\\test.(c|h)|"
                                    ".*\\/Tests/.*\\.*|.*\\/LibrariesExtra/.*\\.*)$";
        arguments[numArguments++] = "--output-dir";                    // 7
        arguments[numArguments++] = "coverage";                        // 8
        arguments[numArguments++] = "-instr-profile=profile.profdata"; // 9
        arguments[numArguments++] = executablePath.view();             // 10
        SC_TRY(process.exec({arguments, numArguments}));
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing llvm-cov show");
    }
    // Extract report data to generate badge
    {
        numArguments = baseArguments;

        // Generate coverage report
        Process process;
        SC_TRY(process.setWorkingDirectory(coverageDirectory.view()));
        arguments[numArguments++] = "llvm-cov"; // 2
        arguments[numArguments++] = "report";   // 3
        // TODO: De-hardcode this filter and pass it as a parameter
        arguments[numArguments++] = "-ignore-filename-regex=" // 6
                                    "^(.*\\/SC-.*\\.*|.*\\/Tools.*|.*\\Test.(cpp|h|c)|.*\\test.(c|h)|"
                                    ".*\\/Tests/.*\\.*|.*\\/LibrariesExtra/.*\\.*)$";
        arguments[numArguments++] = "-instr-profile=profile.profdata"; // 9
        arguments[numArguments++] = executablePath.view();             // 10

        String output;
        SC_TRY(process.exec({arguments, numArguments}, output));
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing llvm-cov report");

        // Parse coverage report
        StringView totals;
        SC_TRY(output.view().splitAfter("\nTOTAL ", totals));
        StringViewTokenizer tokenizer(totals);
        for (int i = 0; i < 9; ++i)
            SC_TRY(tokenizer.tokenizeNext({' '}, StringViewTokenizer::SkipEmpty));

        // Generate coverage badge if not existing
        {
            StringView coverageString = tokenizer.component.trimEndAnyOf({'%'});
            String     localFile;
            SC_TRY(Path::join(localFile, {coverageDirectory.view(), "coverage", "coverage.svg"}));

            // Define coverage badge color
            StringView coverageColor;
            float      coverageFloat;
            SC_TRY(coverageString.parseFloat(coverageFloat));
            if (coverageFloat < 80)
                coverageColor = "e05d44"; // red
            else if (coverageFloat < 90)
                coverageColor = "dfb317"; // yellow
            else
                coverageColor = "97ca00"; // green

            // Coverage badge SVG template
            StringView coverageBadge =
                R"svg(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="114" height="20" role="img" aria-label="coverage: {0}%"><title>coverage: {0}%</title><linearGradient id="s" x2="0" y2="100%"><stop offset="0" stop-color="#bbb" stop-opacity=".1"/><stop offset="1" stop-opacity=".1"/></linearGradient><clipPath id="r"><rect width="114" height="20" rx="3" fill="#fff"/></clipPath><g clip-path="url(#r)"><rect width="61" height="20" fill="#555"/><rect x="61" width="53" height="20" fill="#{1}"/><rect width="114" height="20" fill="url(#s)"/></g><g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" text-rendering="geometricPrecision" font-size="110"><text aria-hidden="true" x="315" y="150" fill="#010101" fill-opacity=".3" transform="scale(.1)" textLength="510">coverage</text><text x="315" y="140" transform="scale(.1)" fill="#fff" textLength="510">coverage</text><text aria-hidden="true" x="865" y="150" fill="#010101" fill-opacity=".3" transform="scale(.1)" textLength="430">{0}%</text><text x="865" y="140" transform="scale(.1)" fill="#fff" textLength="430">{0}%</text></g></svg>)svg";

            // Compile coverage badge SVG template with proper color and percentage
            String compiledCoverageBadge;
            SC_TRY(StringBuilder(compiledCoverageBadge).format(coverageBadge, coverageString, coverageColor));

            // Write badge svg to disk
            FileSystem fs;
            SC_TRY(fs.init(coverageDirectory.view()));
            SC_TRY(fs.writeString("coverage/coverage.svg", compiledCoverageBadge.view()));
        }
    }

    return Result(true);
}

SC::Result SC::Build::Action::Internal::executeInternal(StringView projectFileName, const Action& action,
                                                        String* outputExecutable)
{
    StringView configuration = action.configuration.isEmpty() ? "Debug" : action.configuration;

    SmallString<256> solutionLocation;

    Process process;
    switch (action.parameters.generator)
    {
    case Generator::XCode: {
        SC_TRY(Path::join(solutionLocation, {action.parameters.directories.projectsDirectory.view(),
                                             Generator::toString(action.parameters.generator), projectFileName}));
        SC_TRY(StringBuilder(solutionLocation, StringBuilder::DoNotClear).append(".xcodeproj"));
        StringView architecture;
        SC_TRY(toXCodeArchitecture(action.parameters.architecture, architecture));
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
    case Generator::VisualStudio2019:
    case Generator::VisualStudio2022: {
        SC_TRY(Path::join(solutionLocation, {action.parameters.directories.projectsDirectory.view(),
                                             Generator::toString(action.parameters.generator), projectFileName}));
        SC_TRY(StringBuilder(solutionLocation, StringBuilder::DoNotClear).append(".sln"));
        SmallString<32> platformConfiguration;
        SC_TRY(StringBuilder(platformConfiguration).format("/p:Configuration={}", configuration));

        StringView architecture;
        SC_TRY(Internal::toVisualStudioArchitecture(action.parameters.architecture, architecture));
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
        SC_TRY(Path::join(solutionLocation, {action.parameters.directories.projectsDirectory.view(),
                                             Generator::toString(action.parameters.generator)}));
        String makeFileLocation;
        SC_TRY(StringBuilder(makeFileLocation).append("Makefile.{}", Platform::toString(action.parameters.platform)));
        SmallString<32> platformConfiguration;
        SC_TRY(StringBuilder(platformConfiguration).format("CONFIG={}", configuration));

        StringView architecture;
        SC_TRY(toMakefileArchitecture(action.parameters.architecture, architecture));
        StringView arguments[16];
        size_t     numArgs   = 0;
        arguments[numArgs++] = "make"; // 1

        SmallString<32> targetName;
        switch (action.action)
        {
        case Action::Compile: {
            SC_TRY(StringBuilder(targetName).format("{}_COMPILE", projectFileName));
        }
        break;
        case Action::Run: {
            SC_TRY(StringBuilder(targetName).format("{}_RUN", projectFileName));
        }
        break;
        case Action::Print: {
            SC_TRY(StringBuilder(targetName).format("{}_PRINT_EXECUTABLE_PATH", projectFileName));
        }
        break;
        default: return Result::Error("Unexpected Build::Action (supported \"compile\", \"run\")");
        }

        arguments[numArgs++] = targetName.view();            // 2
        arguments[numArgs++] = "-j";                         // 3
        arguments[numArgs++] = "-C";                         // 4
        arguments[numArgs++] = solutionLocation.view();      // 5
        arguments[numArgs++] = "-f";                         // 6
        arguments[numArgs++] = makeFileLocation.view();      // 7
        arguments[numArgs++] = platformConfiguration.view(); // 8
        if (not architecture.isEmpty())
        {
            arguments[numArgs++] = architecture; // 9
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
