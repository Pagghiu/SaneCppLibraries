// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Build.h"

#include "BuildWriterMakefile.inl"
#include "BuildWriterVisualStudio.inl"
#include "BuildWriterXCode.inl"

#include "../../Libraries/Containers/Vector.h"
#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/FileSystemIterator/FileSystemIterator.h"
#include "../../Libraries/Process/Process.h" // for Actions::compile

struct SC::Build::CompileFlags::Internal
{
    template <typename FieldType, typename FlagsClass>
    static void writeStrongest(FieldType FlagsClass::* ptr, const SC::Span<const FlagsClass*> opinions,
                               FlagsClass& flags)
    {
        for (const FlagsClass* opinion : opinions)
        {
            if (((*opinion).*ptr).hasBeenSet())
            {
                flags.*ptr = ((*opinion).*ptr);
                break;
            }
        }
    }
};

SC::Result SC::Build::CompileFlags::merge(Span<const CompileFlags*> opinions, CompileFlags& flags)
{
    Internal::writeStrongest(&CompileFlags::optimizationLevel, opinions, flags);
    Internal::writeStrongest(&CompileFlags::enableASAN, opinions, flags);
    Internal::writeStrongest(&CompileFlags::enableExceptions, opinions, flags);
    Internal::writeStrongest(&CompileFlags::enableStdCpp, opinions, flags);
    Internal::writeStrongest(&CompileFlags::enableCoverage, opinions, flags);
    Internal::writeStrongest(&CompileFlags::cppStandard, opinions, flags);

    // TODO: Implement ability to "remove" paths from stronger opinions
    for (const CompileFlags* opinion : opinions)
    {
        SC_TRY(flags.defines.insert(0, opinion->defines.toSpanConst()));
        SC_TRY(flags.includePaths.insert(0, opinion->includePaths.toSpanConst()));
        SC_TRY(flags.warnings.insert(0, opinion->warnings.toSpanConst()));
    }

    return Result(true);
}

bool SC::Build::CompileFlags::disableWarnings(Span<const uint32_t> number)
{
    for (auto& it : number)
        SC_TRY(warnings.push_back({Warning::Disabled, it}));
    return true;
}

bool SC::Build::CompileFlags::disableWarnings(Span<const StringView> name)
{

    for (auto& it : name)
        SC_TRY(warnings.push_back({Warning::Disabled, it}));
    return true;
}

bool SC::Build::CompileFlags::disableClangWarnings(Span<const StringView> name)
{
    for (auto& it : name)
        SC_TRY(warnings.push_back({Warning::Disabled, it, Warning::ClangWarning}));
    return true;
}

bool SC::Build::CompileFlags::disableGCCWarnings(Span<const StringView> name)
{
    for (auto& it : name)
        SC_TRY(warnings.push_back({Warning::Disabled, it, Warning::GCCWarning}));
    return true;
}

bool SC::Build::CompileFlags::addIncludePaths(Span<const StringView> paths) { return includePaths.append(paths); }

bool SC::Build::CompileFlags::addDefines(Span<const StringView> preprocessorDefines)
{
    return defines.append(preprocessorDefines);
}

SC::Result SC::Build::LinkFlags::merge(Span<const LinkFlags*> opinions, LinkFlags& flags)
{
    CompileFlags::Internal::writeStrongest(&LinkFlags::enableASAN, opinions, flags);

    // TODO: Implement ability to "remove" paths from stronger opinions
    for (const LinkFlags* opinion : opinions)
    {
        SC_TRY(flags.libraryPaths.append(opinion->libraryPaths.toSpanConst()));
        SC_TRY(flags.libraries.append(opinion->libraries.toSpanConst()));
        SC_TRY(flags.frameworks.append(opinion->libraries.toSpanConst()));
        SC_TRY(flags.frameworksIOS.append(opinion->libraries.toSpanConst()));
        SC_TRY(flags.frameworksMacOS.append(opinion->libraries.toSpanConst()));
    }
    return Result(true);
}

SC::Build::Configuration::Configuration()
{
    (void)outputPath.assign(getStandardBuildDirectory());
    (void)StringBuilder::format(intermediatesPath, "$(PROJECT_NAME)/{}", getStandardBuildDirectory());
}

bool SC::Build::Configuration::applyPreset(const Project& project, Preset newPreset, const Parameters& parameters)
{
    switch (newPreset)
    {
    case Configuration::Preset::DebugCoverage:
        if (not project.files.compile.enableASAN.hasBeenSet())
        {
            compile.enableCoverage = true;
        }
        compile.optimizationLevel = Optimization::Debug;
        SC_TRY(compile.defines.append({"DEBUG=1"}));
        if (parameters.generator == Build::Generator::VisualStudio2022)
        {
            visualStudio.platformToolset = "ClangCL";
        }
        break;
    case Configuration::Preset::Debug:
        if (not project.files.compile.enableASAN.hasBeenSet())
        {
            // VS ASAN is unsupported on ARM64 and needs manual flags / libs with ClangCL toolset
            // It also needs paths where clang_rt.asan_*.dll exist to be manually set before debugging
            if (parameters.generator != Generator::VisualStudio2022 and
                parameters.generator != Generator::VisualStudio2019)
            {
                compile.enableASAN = true;
            }
        }
        compile.optimizationLevel = Optimization::Debug;
        SC_TRY(compile.defines.append({"DEBUG=1"}));
        break;
    case Configuration::Preset::Release:
        compile.optimizationLevel = Optimization::Release;
        SC_TRY(compile.defines.append({"NDEBUG=1"}));
        break;
    default: return false;
    }
    return true;
}

bool SC::Build::Project::setRootDirectory(StringView file)
{
    return Path::normalize(rootDirectory, file, Path::AsPosix);
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
    SC_TRY(configuration.applyPreset(*this, preset, parameters));
    return configurations.push_back(configuration);
}

SC::Build::Configuration* SC::Build::Project::getConfiguration(StringView configurationName)
{
    size_t index = 0;
    if (configurations.find([=](auto it) { return it.name == configurationName; }, &index))
    {
        return &configurations[index];
    }
    return nullptr;
}

const SC::Build::Configuration* SC::Build::Project::getConfiguration(StringView configurationName) const
{
    size_t index = 0;
    if (configurations.find([=](auto it) { return it.name == configurationName; }, &index))
    {
        return &configurations[index];
    }
    return nullptr;
}

bool SC::Build::SourceFiles::addSelection(StringView directory, StringView filter)
{
    return selection.push_back({FilesSelection::Add, directory, filter});
}

bool SC::Build::SourceFiles::removeSelection(StringView directory, StringView filter)
{
    return selection.push_back({FilesSelection::Remove, directory, filter});
}

bool SC::Build::Project::addFiles(StringView subdirectory, StringView filter)
{
    if (subdirectory.containsCodePoint('*') or subdirectory.containsCodePoint('?'))
        return false;
    if (subdirectory.isEmpty() and filter.isEmpty())
        return false;
    FilesSelection selection;
    selection.action = FilesSelection::Add;
    SC_TRY(selection.base.assign(subdirectory));
    SC_TRY(selection.mask.assign(filter));

    // Relativize path if subdirectory or filter is absolute
    StringView source = subdirectory.isEmpty() ? filter : subdirectory;
    StringView other  = subdirectory.isEmpty() ? subdirectory : filter;
    String&    dest   = subdirectory.isEmpty() ? selection.mask : selection.base;
    if (Path::isAbsolute(source, Path::AsNative))
    {
        if (Path::isAbsolute(other, Path::AsNative))
            return false; // cannot be both absolute
        String relativePath;
        SC_TRY(Path::relativeFromTo(relativePath, rootDirectory.view(), source, Path::AsNative));
        SC_TRY(StringBuilder::create(dest).appendReplaceAll(relativePath.view(), "\\", "/"));
    }
    else
    {
        SC_TRY(StringBuilder::create(dest).appendReplaceAll(source, "\\", "/"));
    }

    return files.selection.push_back(move(selection));
}

bool SC::Build::Project::addIncludePaths(Span<const StringView> includePaths)
{
    return files.compile.includePaths.append(includePaths);
}

bool SC::Build::Project::addLinkLibraryPaths(Span<const StringView> libraryPaths)
{
    return link.libraryPaths.append(libraryPaths);
}

bool SC::Build::Project::addLinkLibraries(Span<const StringView> linkLibraries)
{
    return link.libraries.append(linkLibraries);
}

bool SC::Build::Project::addLinkFrameworks(Span<const StringView> frameworks)
{
    return link.frameworks.append(frameworks);
}

bool SC::Build::Project::addLinkFrameworksMacOS(Span<const StringView> frameworks)
{
    return link.frameworksMacOS.append(frameworks);
}

bool SC::Build::Project::addLinkFrameworksIOS(Span<const StringView> frameworks)
{
    return link.frameworksIOS.append(frameworks);
}

bool SC::Build::Project::addDefines(Span<const StringView> defines) { return files.compile.defines.append(defines); }

bool SC::Build::Project::addFile(StringView singleFile) { return addFiles({}, singleFile); }

bool SC::Build::Project::addSpecificFileFlags(SourceFiles selection)
{
    return filesWithSpecificFlags.push_back(selection);
}

bool SC::Build::Project::removeFiles(StringView subdirectory, StringView filter)
{
    if (subdirectory.containsCodePoint('*') or subdirectory.containsCodePoint('?'))
        return false;
    return files.selection.push_back({FilesSelection::Remove, subdirectory, filter});
}

SC::Result SC::Build::Project::validate() const
{
    SC_TRY_MSG(not name.isEmpty(), "Project needs name");
    SC_TRY_MSG(not targetName.isEmpty(), "Project needs targetName");
    SC_TRY_MSG(not rootDirectory.isEmpty(), "Project needs targetName");
    SC_TRY_MSG(configurations.size() > 0, "Project needs at least one configuration");
    for (const Configuration& config : configurations)
    {
        SC_TRY_MSG(not config.name.isEmpty(), "Configuration needs a name");
        SC_TRY_MSG(not config.outputPath.isEmpty(), "Configuration needs an output path");
        SC_TRY_MSG(not config.intermediatesPath.isEmpty(), "Configuration needs an intermediates path");
    }
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

SC::Result SC::Build::Definition::configure(StringView workspaceName, const Build::Parameters& parameters) const
{
    for (const auto& workspace : workspaces)
    {
        SC_TRY(workspace.validate());
    }
    FilePathsResolver filePathsResolver;
    SC_TRY(filePathsResolver.resolve(*this));
    String projectGeneratorSubFolder = StringEncoding::Utf8;
    {
        Vector<StringView> components;
        SC_TRY(Path::normalize(projectGeneratorSubFolder, parameters.directories.projectsDirectory.view(),
                               Path::Type::AsPosix));
        SC_TRY(Path::append(projectGeneratorSubFolder, {Generator::toString(parameters.generator), workspaceName},
                            Path::Type::AsPosix));
        if (parameters.generator == Generator::Make)
        {
            if (parameters.platform == Platform::Linux)
            {
                SC_TRY(Path::append(projectGeneratorSubFolder, {"linux"}, Path::Type::AsPosix));
            }
            else
            {
                SC_TRY(Path::append(projectGeneratorSubFolder, {"apple"}, Path::Type::AsPosix));
            }
        }
    }
    Build::Parameters newParameters             = parameters;
    newParameters.directories.projectsDirectory = move(projectGeneratorSubFolder);
    Build::ProjectWriter writer(*this, filePathsResolver, newParameters);
    return Result(writer.write(workspaceName));
}

bool SC::Build::Definition::findConfiguration(StringView workspaceName, StringView projectName,
                                              StringView configurationName, Workspace*& workspace, Project*& project,
                                              Configuration*& configuration)
{
    size_t workspaceIdx = 0;
    SC_TRY(workspaces.find([&](auto& it) { return it.name == workspaceName; }, &workspaceIdx));
    workspace         = &workspaces[workspaceIdx];
    size_t projectIdx = 0;
    SC_TRY(workspace->projects.find([&](auto& it) { return it.name == projectName; }, &projectIdx));
    project                 = &workspace->projects[projectIdx];
    size_t configurationIdx = 0;
    SC_TRY(project->configurations.find([&](auto& it) { return it.name == configurationName; }, &configurationIdx));
    configuration = &project->configurations[configurationIdx];
    return true;
}

SC::Result SC::Build::FilePathsResolver::enumerateFileSystemFor(StringView                         path,
                                                                const VectorSet<FilesSelection>&   filters,
                                                                VectorMap<String, Vector<String>>& filtersToFiles)
{
    bool doRecurse = false;
    for (const FilesSelection& it : filters)
    {
        if (StringView(it.mask.view()).containsCodePoint('/'))
        {
            doRecurse = true;
            break;
        }
        if (StringView(it.mask.view()).containsString("**"))
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

    Vector<FilesSelection> renderedFilters;
    for (const FilesSelection& filter : filters)
    {
        FilesSelection file;
        file.action = filter.action;
        SC_TRY(file.mask.assign(path));
        SC_TRY(Path::append(file.mask, {filter.mask.view()}, Path::AsPosix));
        SC_TRY(renderedFilters.push_back(move(file)));
    }

    FileSystemIterator::FolderState entries[16];

    FileSystemIterator fsIterator;
    fsIterator.options.forwardSlashes = true;
    SC_TRY(fsIterator.init(path, entries));

    while (fsIterator.enumerateNext())
    {
        const FileSystemIterator::Entry& item = fsIterator.get();
        if (doRecurse and item.isDirectory())
        {
            // TODO: Check if it's possible to optimize entire subdirectory out in some cases
            SC_TRY(fsIterator.recurseSubdirectory());
        }
        else
        {
            for (const FilesSelection& filter : renderedFilters)
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

SC::Result SC::Build::FilePathsResolver::resolve(const Build::Definition& definition)
{
    VectorMap<String, VectorSet<FilesSelection>> uniquePaths;

    String buffer;

    SmallVector<StringView, 16> components;

    for (const Workspace& workspace : definition.workspaces)
    {
        for (const Project& project : workspace.projects)
        {
            for (const FilesSelection& file : project.files.selection)
            {
                SC_TRY(mergePathsFor(file, project.rootDirectory.view(), buffer, uniquePaths));
            }
            for (const SourceFiles& sourceFiles : project.filesWithSpecificFlags)
            {
                for (const FilesSelection& file : sourceFiles.selection)
                {
                    SC_TRY(mergePathsFor(file, project.rootDirectory.view(), buffer, uniquePaths));
                }
            }
        }
    }

    for (auto& it : uniquePaths)
    {
        SC_TRY(enumerateFileSystemFor(it.key.view(), it.value, resolvedPaths));
    }
    return Result(true);
}

SC::Result SC::Build::FilePathsResolver::mergePathsFor(const FilesSelection& file, const StringView rootDirectory,
                                                       String&                                       buffer,
                                                       VectorMap<String, VectorSet<FilesSelection>>& paths)
{
    SC_TRY(buffer.assign(rootDirectory));
    if (Path::isAbsolute(file.base.view(), Path::Type::AsNative))
    {
        FilesSelection absFile;
        absFile.action = file.action;
        SC_TRY(Path::normalize(absFile.base, file.base.view(), Path::Type::AsPosix));
        SC_TRY(absFile.mask.assign(file.mask.view()));
        SC_TRY(paths.getOrCreate(absFile.base.view())->insert(absFile));
        return Result(true);
    }
    if (file.base.view().isEmpty())
    {
        if (not file.mask.isEmpty())
        {
            if (Path::isAbsolute(file.mask.view(), Path::AsNative))
            {
                return Result::Error("Absolute path detected");
            }
            SC_TRY(Path::append(buffer, {file.mask.view()}, Path::AsPosix));
            auto* value = paths.getOrCreate(buffer);
            SC_TRY(value != nullptr and value->insert(file));
        }
        return Result(true);
    }
    SC_TRY(Path::append(buffer, {file.base.view()}, Path::AsPosix));
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

        StringView key = it.key.view();
        StringView buf = buffer.view();
        if (key.fullyOverlaps(buffer.view(), commonOverlap))
        {
            // they are the same (Case 4. after 2. has been inserted)
            SC_TRY(it.value.insert(file));
            shouldInsert = false;
            break;
        }
        else
        {
            const StringView overlapNew      = buf.sliceStart(commonOverlap);
            const StringView overlapExisting = key.sliceStart(commonOverlap);
            if (overlapExisting.isEmpty())
            {
                // Case .5 and .3 after .2
                if (overlapNew.startsWithAnyOf({'/'}))
                {
                    // Case .3 after 2 (can be merged)
                    FilesSelection mergedFile;
                    mergedFile.action = file.action;
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
    return Result(true);
}

SC::Result SC::Build::ProjectWriter::write(StringView workspaceName)
{
    const Directories& directories = parameters.directories;
    SC_TRY(Path::isAbsolute(directories.projectsDirectory.view(), Path::AsNative));

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(directories.projectsDirectory.view()));
    SC_TRY(fs.init(directories.projectsDirectory.view()));

    size_t idx = 0;
    SC_TRY_MSG(definition.workspaces.find([&](const Workspace& it) { return it.name == workspaceName; }, &idx),
               "Workspace not found in definition");

    const Workspace& workspace = definition.workspaces[idx];

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
            WriterXCode           writer(definition, filePathsResolver, directories, relativeDirectories);
            WriterXCode::Renderer renderer;
            StringView            projectName = project.name.view();
            SC_TRY(writer.prepare(project, renderer));
            {
                auto builder = StringBuilder::create(buffer);
                SC_TRY(writer.writeProject(builder, project, renderer));
                builder.finalize();
                SC_TRY(StringBuilder::format(prjName, "{}.xcodeproj", projectName));
                SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));

                SC_TRY(StringBuilder::format(prjName, "{}.xcodeproj/project.pbxproj", projectName));
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), buffer.view()));
            }
            {
                auto builder = StringBuilder::create(buffer);
                SC_TRY(writer.writeScheme(builder, project, renderer, projectName));
                SC_TRY(StringBuilder::format(prjName, "{}.xcodeproj/xcshareddata", projectName));
                SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
                SC_TRY(StringBuilder::format(prjName, "{}.xcodeproj/xcshareddata/xcschemes", projectName));
                SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
                SC_TRY(StringBuilder::format(prjName, "{}.xcodeproj/xcshareddata/xcschemes/{}.xcscheme", projectName,
                                             projectName));
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), builder.finalize()));
            }
            switch (project.targetType)
            {
            case TargetType::ConsoleExecutable: break;
            case TargetType::GUIApplication: {
                auto builderEntitlements = StringBuilder::create(buffer);
                SC_TRY(StringBuilder::format(prjName, "{0}.entitlements", projectName));
                SC_TRY(writer.writeEntitlements(builderEntitlements, project));
                builderEntitlements.finalize();
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), buffer.view()));

                auto builderStoryboard = StringBuilder::create(buffer);
                SC_TRY(StringBuilder::format(prjName, "{0}.storyboard", projectName));
                SC_TRY(writer.writeStoryboard(builderStoryboard, project));
                builderStoryboard.finalize();
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), buffer.view()));

                SC_TRY(writer.writeAssets(fs, project));
            }
            break;
            }
        }
        // Write workspace
        {
            auto builder = StringBuilder::create(buffer);
            SC_TRY(WriterXCode::writeWorkspace(builder, workspace.projects.toSpanConst()));
            builder.finalize();
            SC_TRY(StringBuilder::format(prjName, "{}.xcworkspace", workspace.name));
            SC_TRY(fs.makeDirectoryIfNotExists({prjName.view()}));
            SC_TRY(StringBuilder::format(prjName, "{}.xcworkspace/contents.xcworkspacedata", workspace.name));
            SC_TRY(fs.removeFileIfExists(prjName.view()));
            SC_TRY(fs.writeString(prjName.view(), buffer.view()));
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
            WriterVisualStudio writer(definition, filePathsResolver, directories, relativeDirectories,
                                      parameters.generator);

            WriterVisualStudio::Renderer renderer;
            SC_TRY(writer.prepare(project, renderer));
            SC_TRY(writer.generateGuidFor(project.name.view(), writer.hashing, writer.projectGuid));
            {
                auto builder = StringBuilder::create(buffer);
                SC_TRY(writer.writeProject(builder, project, renderer));
                String prjName;
                SC_TRY(StringBuilder::format(prjName, "{}.vcxproj", project.name));
                SC_TRY(fs.removeFileIfExists(prjName.view()));
                SC_TRY(fs.writeString(prjName.view(), builder.finalize()));
            }
            {
                auto builder = StringBuilder::create(buffer);
                SC_TRY(writer.writeFilters(builder, renderer));
                String prjFilterName;
                SC_TRY(StringBuilder::format(prjFilterName, "{}.vcxproj.filters", project.name));
                SC_TRY(fs.removeFileIfExists(prjFilterName.view()));
                SC_TRY(fs.writeString(prjFilterName.view(), builder.finalize()));
            }
            SC_TRY(projectsGuids.push_back(writer.projectGuid));
        }
        // Write solution for all projects
        {
            auto builder = StringBuilder::create(buffer);
            SC_TRY(WriterVisualStudio::writeSolution(builder, workspace.projects.toSpanConst(),
                                                     projectsGuids.toSpanConst()));
            String slnName;
            SC_TRY(StringBuilder::format(slnName, "{}.sln", workspace.name));
            SC_TRY(fs.removeFileIfExists(slnName.view()));
            SC_TRY(fs.writeString(slnName.view(), builder.finalize()));
        }
        break;
    }
    case Generator::Make: {
        WriterMakefile           writer(definition, filePathsResolver, directories);
        WriterMakefile::Renderer renderer;
        {
            auto builder = StringBuilder::create(buffer);
            SC_TRY(writer.writeMakefile(builder, workspace, renderer));
            builder.finalize();
            SC_TRY(fs.removeFileIfExists("Makefile"));
            SC_TRY(fs.writeString("Makefile", buffer.view()));
        }
        break;
    }
    }
    return Result(true);
}

struct SC::Build::Action::Internal
{
    static Result configure(ConfigureFunction configure, const Action& action);
    static Result coverage(ConfigureFunction configure, const Action& action);
    static Result compileRunPrint(const Action& action, Span<StringView> environment = {},
                                  String* outputExecutable = nullptr);
    static Result runExecutable(StringView executablePath, Span<StringView> arguments, const Action& action);

    static Result toVisualStudioArchitecture(Architecture::Type architectureType, StringView& architecture)
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

    static Result toXCodeArchitecture(Architecture::Type architectureType, StringView& architecture)
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

    static Result toMakefileArchitecture(Architecture::Type architectureType, StringView& architecture)
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

SC::Result SC::Build::Action::execute(const Action& action, ConfigureFunction configure,
                                      StringView defaultWorkspaceName)
{
    Action newAction = action;

    if (newAction.workspaceName.isEmpty())
    {
        newAction.workspaceName = defaultWorkspaceName;
    }
    if (newAction.projectName.isEmpty())
    {
        newAction.allTargets  = true;
        newAction.projectName = newAction.workspaceName;
    }
    else
    {
        newAction.allTargets = false;
    }
    if (newAction.configurationName.isEmpty())
    {
        newAction.configurationName = "Debug";
    }
    switch (action.action)
    {
    case Print:
    case Run:
    case Compile: return Internal::compileRunPrint(newAction);
    case Coverage: return Internal::coverage(configure, newAction);
    case Configure: return Internal::configure(configure, newAction);
    }
    return Result::Error("Action::execute - unsupported action");
}

SC::Result SC::Build::Action::Internal::configure(ConfigureFunction configure, const Action& action)
{
    Build::Definition definition;
    SC_TRY(configure(definition, action.parameters));
    SC_TRY(definition.configure(action.workspaceName, action.parameters));
    return Result(true);
}
SC::Result SC::Build::Action::Internal::coverage(ConfigureFunction configure, const Action& action)
{
    Action newAction = action;
    String executablePath;

    // Build the configuration with coverage information
    newAction.action         = Action::Compile;
    StringView environment[] = {"CC", "clang", "CXX", "clang++"};
    SC_TRY(compileRunPrint(newAction, environment));

    // Get coverage configuration executable path
    newAction.action = Action::Print;
    SC_TRY(compileRunPrint(newAction, environment, &executablePath));

    Build::Definition definition;
    SC_TRY(configure(definition, action.parameters));
    Workspace*     workspace     = nullptr;
    Project*       project       = nullptr;
    Configuration* configuration = nullptr;
    SC_TRY(definition.findConfiguration(action.workspaceName, action.projectName, action.configurationName, workspace,
                                        project, configuration));
    String coverageExcludeRegex;
    if (not configuration->coverage.excludeRegex.isEmpty())
    {
        SC_TRY(StringBuilder::format(coverageExcludeRegex, "-ignore-filename-regex=^({})$",
                                     configuration->coverage.excludeRegex.view()));
    }
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
        SC_TRY_MSG(process.exec({executablePath.view()}), "Cannot find instrumented executable");
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing instrumented executable");
    }

    // Merge coverage files
    StringView arguments[16];

    size_t numArguments  = 0;
    size_t baseArguments = 0;

    String llvmProfData = "llvm-profdata";
    String llvmCov      = "llvm-cov";
    switch (HostPlatform)
    {
    case SC::Platform::Apple:
        arguments[numArguments++] = "xcrun"; // 1
        baseArguments             = 1;
        break;
    default: {
        String version;
        SC_TRY_MSG(Process().exec({"clang", "--version"}, version), "Cannot run clang --version");
        StringViewTokenizer tokenizer(version.view());

        int major = -1;
        while (tokenizer.tokenizeNext({' '}))
        {
            StringViewTokenizer subTokenizer(tokenizer.component);
            if (not subTokenizer.tokenizeNext({'.'}))
            {
                continue;
            }
            if (subTokenizer.component.parseInt32(major))
            {
                break;
            }
        }

        if (major > 0)
        {
            SC_TRY(StringBuilder::createForAppendingTo(llvmProfData).append("-{}", major));
            SC_TRY(StringBuilder::createForAppendingTo(llvmCov).append("-{}", major));
        }

        break;
    }
    }
    {
        numArguments = baseArguments;
        Process process;
        SC_TRY(process.setWorkingDirectory(coverageDirectory.view()));
        arguments[numArguments++] = llvmProfData.view(); // 2
        arguments[numArguments++] = "merge";             // 3
        arguments[numArguments++] = "-sparse";           // 4
        arguments[numArguments++] = "profile.profraw";   // 5
        arguments[numArguments++] = "-o";                // 6
        arguments[numArguments++] = "profile.profdata";  // 7
        SC_TRY_MSG(process.exec({arguments, numArguments}), "llvm-profdata missing");
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing llvm-profdata");
    }
    // Generate HTML excluding all tests and SC::Tools
    {
        numArguments = baseArguments;
        Process process;
        SC_TRY(process.setWorkingDirectory(coverageDirectory.view()));
        arguments[numArguments++] = llvmCov.view(); // 2
        arguments[numArguments++] = "show";         // 3
        arguments[numArguments++] = "-format";      // 4
        arguments[numArguments++] = "html";         // 5

        if (not coverageExcludeRegex.isEmpty())
        {
            arguments[numArguments++] = coverageExcludeRegex.view(); // 6
        }
        arguments[numArguments++] = "--output-dir";                    // 7
        arguments[numArguments++] = "coverage";                        // 8
        arguments[numArguments++] = "-instr-profile=profile.profdata"; // 9
        arguments[numArguments++] = executablePath.view();             // 10
        SC_TRY_MSG(process.exec({arguments, numArguments}), "llvm-cov is missing");
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing llvm-cov show");
    }
    // Extract report data to generate badge
    {
        numArguments = baseArguments;

        // Generate coverage report
        Process process;
        SC_TRY(process.setWorkingDirectory(coverageDirectory.view()));
        arguments[numArguments++] = llvmCov.view(); // 2
        arguments[numArguments++] = "report";       // 3

        if (not coverageExcludeRegex.isEmpty())
        {
            arguments[numArguments++] = coverageExcludeRegex.view(); // 4
        }

        arguments[numArguments++] = "-instr-profile=profile.profdata"; // 9
        arguments[numArguments++] = executablePath.view();             // 10

        String output;
        SC_TRY_MSG(process.exec({arguments, numArguments}, output), "llvm-cov is missing");
        SC_TRY_MSG(process.getExitStatus() == 0, "Error executing llvm-cov report");

        // Parse coverage report
        StringView totals;
        StringView out = output.view();
        SC_TRY(out.splitAfter("\nTOTAL ", totals));
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
            SC_TRY_MSG(coverageString.parseFloat(coverageFloat), "Cannot parse coverage percentage");
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
            SC_TRY(StringBuilder::format(compiledCoverageBadge, coverageBadge, coverageString, coverageColor));

            // Write badge svg to disk
            FileSystem fs;
            SC_TRY(fs.init(coverageDirectory.view()));
            SC_TRY(fs.writeString("coverage/coverage.svg", compiledCoverageBadge.view()));
        }
    }

    return Result(true);
}

SC::Result SC::Build::Action::Internal::runExecutable(StringView executablePath, Span<StringView> arguments,
                                                      const Build::Action& action)
{
    Process runProcess;
    size_t  numArgs = 1;
    arguments[0]    = StringView(executablePath).trimWhiteSpaces();
    globalConsole->print("COMMAND = {}\n", arguments[0]);
    for (size_t idx = 0; idx < action.additionalArguments.sizeInElements(); ++idx)
    {
        if (numArgs == arguments.sizeInElements())
        {
            globalConsole->printLine("Exceeded max number of arguments that can be passed to the executable");
            break;
        }
        arguments[numArgs] = action.additionalArguments[idx];
        globalConsole->print("ARGS[{}] = {}\n", idx, arguments[numArgs]);
        numArgs++;
    }
    globalConsole->flush();
    SC_TRY(runProcess.exec({arguments.data(), numArgs}));
    SC_TRY_MSG(runProcess.getExitStatus() == 0, "Run exited with non zero status");
    return Result(true);
}

SC::Result SC::Build::Action::Internal::compileRunPrint(const Action& action, Span<StringView> environment,
                                                        String* outputExecutable)
{

    SmallString<256> solutionLocation;

    Process process;
    switch (action.parameters.generator)
    {
    case Generator::XCode: {
        SC_TRY(Path::join(solutionLocation, {action.parameters.directories.projectsDirectory.view(),
                                             Generator::toString(action.parameters.generator), action.workspaceName,
                                             action.projectName}));
        StringView extension = action.allTargets ? ".xcworkspace"_a8 : ".xcodeproj"_a8;
        SC_TRY(StringBuilder::createForAppendingTo(solutionLocation).append(extension));
        StringView architecture;
        SC_TRY(toXCodeArchitecture(action.parameters.architecture, architecture));
        SmallString<32> formattedPlatform;
        SC_TRY(StringBuilder::format(formattedPlatform, "ARCHS={}", architecture));

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
        arguments[numArgs++] = action.configurationName; // 4
        String defaultScheme;
        if (action.allTargets)
        {
            arguments[numArgs++] = "-workspace";            // 5
            arguments[numArgs++] = solutionLocation.view(); // 6
            StringView schemeName;
            {
                // TODO: Match behaviour of other backends where empty target means building all
                // Invoke xcodebuild to list available schemes and pick the first scheme
                arguments[numArgs++] = "-list"; // 7
                Process defaultSchemeProcess;
                SC_TRY(defaultSchemeProcess.exec({arguments, numArgs}, defaultScheme));
                numArgs--;
                SC_TRY(StringView(defaultScheme.view()).splitAfter("Schemes:\n", schemeName));
                SC_TRY(schemeName.splitBefore("\n", schemeName));
            }
            arguments[numArgs++] = "-scheme";                    // 7
            arguments[numArgs++] = schemeName.trimWhiteSpaces(); // 8
        }
        else
        {
            arguments[numArgs++] = "-project";              // 7
            arguments[numArgs++] = solutionLocation.view(); // 8
        }
        arguments[numArgs++] = "ONLY_ACTIVE_ARCH=NO";    // 9
        arguments[numArgs++] = formattedPlatform.view(); // 10
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
                StringView executablePath = userExecutable.view();
                SC_TRY(runExecutable(executablePath, arguments, action));
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
                                             Generator::toString(action.parameters.generator), action.workspaceName,
                                             action.projectName}));
        StringView extension = action.allTargets ? ".sln"_a8 : ".vcxproj"_a8;
        SC_TRY(StringBuilder::createForAppendingTo(solutionLocation).append(extension));
        SmallString<32> platformConfiguration;
        SC_TRY(StringBuilder::format(platformConfiguration, "/p:Configuration={}", action.configurationName));

        StringView architecture;
        SC_TRY(Internal::toVisualStudioArchitecture(action.parameters.architecture, architecture));
        SmallString<32> platform;
        SC_TRY(StringBuilder::format(platform, "/p:Platform={}", architecture));

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
                SC_TRY(runExecutable(executablePath, arguments, action));
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
                                             Generator::toString(action.parameters.generator), action.workspaceName}));
        if (action.parameters.generator == Generator::Make)
        {
            if (action.parameters.platform == Platform::Linux)
            {
                SC_TRY(Path::append(solutionLocation, {"linux"}, Path::Type::AsPosix));
            }
            else
            {
                SC_TRY(Path::append(solutionLocation, {"apple"}, Path::Type::AsPosix));
            }
        }
        SmallString<32> platformConfiguration;
        SC_TRY(StringBuilder::format(platformConfiguration, "CONFIG={}", action.configurationName));

        StringView architecture;
        SC_TRY(toMakefileArchitecture(action.parameters.architecture, architecture));
        StringView arguments[32];
        size_t     numArgs   = 0;
        arguments[numArgs++] = "make"; // 1

        SmallString<32> targetName;
        switch (action.action)
        {
        case Action::Compile: {
            if (action.allTargets)
            {
                targetName = "all";
            }
            else
            {
                SC_TRY(StringBuilder::format(targetName, "{}_COMPILE_COMMANDS", action.projectName));
            }
        }
        break;
        case Action::Run: {
            // Not using the _RUN target to avoid one level of indirection
            SC_TRY(StringBuilder::format(targetName, "{}_PRINT_EXECUTABLE_PATH", action.projectName));
        }
        break;
        case Action::Print: {
            SC_TRY(StringBuilder::format(targetName, "{}_PRINT_EXECUTABLE_PATH", action.projectName));
        }
        break;
        default: return Result::Error("Unexpected Build::Action (supported \"compile\", \"run\")");
        }

        arguments[numArgs++] = targetName.view();            // 2
        arguments[numArgs++] = "-j";                         // 3
        arguments[numArgs++] = "-C";                         // 4
        arguments[numArgs++] = solutionLocation.view();      // 5
        arguments[numArgs++] = platformConfiguration.view(); // 7
        if (not architecture.isEmpty())
        {
            arguments[numArgs++] = architecture; // 7
        }
        SC_TRY(process.setEnvironment("GNUMAKEFLAGS", "--no-print-directory"));
        if (environment.sizeInElements() % 2 == 0)
        {
            for (size_t idx = 0; idx < environment.sizeInElements(); idx += 2)
            {
                SC_TRY(process.setEnvironment(environment[idx], environment[idx + 1]));
            }
        }
        if (action.action == Action::Print)
        {
            SC_TRY(process.exec({arguments, numArgs}, *outputExecutable));
            StringView out    = outputExecutable->view();
            *outputExecutable = out.trimWhiteSpaces();
        }
        else if (action.action == Action::Run)
        {
            // We are actually invoking _PRINT_EXECUTABLE_PATH
            String executableName;
            SC_TRY(process.exec({arguments, numArgs}, executableName));
            SC_TRY_MSG(process.getExitStatus() == 0, "Print returned error");
            StringView executablePath = executableName.view();
            SC_TRY(runExecutable(executablePath, arguments, action));
        }
        else
        {
            String stdError;
            SC_TRY(process.exec({arguments, numArgs}, {}, {}, stdError));
            if (not stdError.isEmpty())
            {
                globalConsole->printError(stdError.view());
                globalConsole->flushStdErr();
            }
            if (process.getExitStatus() == 0)
            {
                return Result(true);
            }
            else if (StringView(stdError.view()).startsWith("make: *** No rule to make target"))
            {
                globalConsole->print("Compile failed. Cleaning the project and trying again...\n");
                globalConsole->flush();
                arguments[1] = "clean";
                Process cleanProcess;
                SC_TRY(cleanProcess.exec({arguments, numArgs}));
                if (cleanProcess.getExitStatus() == 0)
                {
                    arguments[1] = targetName.view();
                    Process retryProcess;
                    SC_TRY(retryProcess.exec({arguments, numArgs}));
                    if (retryProcess.getExitStatus() == 0)
                    {
                        return Result(true);
                    }
                }
                return Result::Error("Compile returned error");
            }
            return Result::Error("Compile returned error");
        }
    }
    break;
    }
    return Result(true);
}
