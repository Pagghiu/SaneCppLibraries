// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Build.h"
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
};
} // namespace Build
} // namespace SC
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

SC::Result SC::Build::Definition::generate(StringView projectFileName, const Build::Parameters& parameters,
                                           StringView rootPath) const
{
    Build::DefinitionCompiler definitionCompiler(*this);
    SC_TRY(definitionCompiler.validate());
    SC_TRY(definitionCompiler.build());
    Build::ProjectWriter writer(*this, definitionCompiler, parameters);
    StringView           directory;
    switch (parameters.generator)
    {
    case Build::Generator::XCode14: directory = "MacOS"; break;
    case Build::Generator::VisualStudio2022: directory = "Windows"; break;
    }
    return Result(writer.write(rootPath, projectFileName, directory));
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
                            if (overlapNew.startsWithCodePoint('/'))
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
    case Generator::XCode14: {
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
            SC_TRY(fs.write(prjName.view(), buffer.view()));
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
            SC_TRY(fs.write(prjName.view(), buffer.view()));
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
            SC_TRY(fs.write(prjName.view(), buffer1.view()));
        }
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY(writer.writeFilters(sb1, renderer));
            String prjFilterName;
            SC_TRY(StringBuilder(prjFilterName, StringBuilder::Clear).format("{}.vcxproj.filters", filename));
            SC_TRY(fs.removeFileIfExists(prjFilterName.view()));
            SC_TRY(fs.write(prjFilterName.view(), buffer1.view()));
        }
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY(writer.writeSolution(sb1, prjName.view(), project));
            String slnName;
            SC_TRY(StringBuilder(slnName, StringBuilder::Clear).format("{}.sln", filename));
            SC_TRY(fs.removeFileIfExists(slnName.view()));
            SC_TRY(fs.write(slnName.view(), buffer1.view()));
        }
        break;
    }
    }

    return Result(true);
}
