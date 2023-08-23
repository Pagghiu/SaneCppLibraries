// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Build.h"
#include "WriterVisualStudio.inl"
#include "WriterXCode.inl"

#include "../Foundation/SmallVector.h"
#include "../Foundation/StringViewAlgorithms.h"

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
        SC_TRY_IF(configuration.name.assign(Configuration::PresetToString(preset)));
    }
    else
    {
        SC_TRY_IF(configuration.name.assign(configurationName));
    }
    SC_TRY_IF(configuration.applyPreset(preset));
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

bool SC::Build::Project::addFiles(StringView subdirectory, StringView filter)
{
    if (subdirectory.containsChar('*') or subdirectory.containsChar('?'))
        return false;
    return files.push_back({Project::File::Add, subdirectory, filter});
}

bool SC::Build::Project::removeFiles(StringView subdirectory, StringView filter)
{
    if (subdirectory.containsChar('*') or subdirectory.containsChar('?'))
        return false;
    return files.push_back({Project::File::Remove, subdirectory, filter});
}

SC::ReturnCode SC::Build::Project::validate() const
{
    SC_TRY_MSG(not name.isEmpty(), "Project needs name"_a8);
    return true;
}

SC::ReturnCode SC::Build::Workspace::validate() const
{
    for (const auto& project : projects)
    {
        SC_TRY_IF(project.validate());
    }
    return true;
}

SC::ReturnCode SC::Build::DefinitionCompiler::validate()
{
    for (const auto& workspace : definition.workspaces)
    {
        SC_TRY_IF(workspace.validate());
    }
    return true;
}

SC::ReturnCode SC::Build::DefinitionCompiler::build()
{
    Map<String, Set<Project::File>> uniquePaths;
    SC_TRY_IF(collectUniqueRootPaths(uniquePaths));
    for (auto& it : uniquePaths)
    {
        SC_TRY_IF(fillPathsList(it.key.view(), it.value, resolvedPaths));
    }
    return true;
}

SC::ReturnCode SC::Build::DefinitionCompiler::fillPathsList(StringView path, const Set<Project::File>& filters,
                                                            Map<String, Vector<String>>& filtersToFiles)
{
    bool doRecurse = false;
    for (const auto& it : filters)
    {
        if (it.mask.view().containsString("**"))
        {
            doRecurse = true;
            break;
        }
    }

    Vector<Project::File> renderedFilters;
    for (const auto& filter : filters)
    {
        Project::File file;
        file.operation = filter.operation;
        SC_TRY_IF(file.mask.assign(path));
        SC_TRY_IF(Path::append(file.mask, {filter.mask.view()}, Path::AsPosix));
        SC_TRY_IF(renderedFilters.push_back(move(file)));
    }

    FileSystemWalker walker;
    walker.options.forwardSlashes = true;
    SC_TRY_IF(walker.init(path));

    while (walker.enumerateNext())
    {
        auto& item = walker.get();
        if (doRecurse and item.isDirectory())
        {
            // TODO: Check if it's possible to optimize entire subdirectory out in some cases
            SC_TRY_IF(walker.recurseSubdirectory());
        }
        else
        {
            for (const auto& filter : renderedFilters)
            {
                if (StringAlgorithms::matchWildcard(filter.mask.view(), item.path))
                {
                    SC_TRY_IF(filtersToFiles.getOrCreate(filter.mask)->push_back(item.path));
                }
            }
        }
    }
    return walker.checkErrors();
}

// Collects root paths to build a stat map
SC::ReturnCode SC::Build::DefinitionCompiler::collectUniqueRootPaths(Map<String, Set<Project::File>>& paths)
{
    String buffer;
    for (const Workspace& workspace : definition.workspaces)
    {
        for (const Project& project : workspace.projects)
        {
            for (const Project::File& file : project.files)
            {
                SC_TRY_IF(buffer.assign(project.rootDirectory.view()));
                SC_TRY_IF(Path::append(buffer, file.base.view(), Path::AsPosix));
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
                        SC_TRY_IF(it.value.insert(file));
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
                            if (overlapNew.startsWithChar('/'))
                            {
                                // Case .3 after 2 (can be merged)
                                Project::File mergedFile;
                                mergedFile.operation = file.operation;
                                SC_TRY_IF(mergedFile.base.assign(it.value.begin()->base.view()));
                                SC_TRY_IF(mergedFile.mask.assign(Path::removeStartingSeparator(overlapNew)));
                                SC_TRY_IF(Path::append(mergedFile.mask, {file.mask.view()}, Path::AsPosix));
                                SC_TRY_IF(it.value.insert(mergedFile));
                                shouldInsert = false;
                                break;
                            }
                        }
                    }
                }
                if (shouldInsert)
                {
                    auto* value = paths.getOrCreate(buffer);
                    SC_TRY_IF(value != nullptr and value->insert(file));
                }
            }
        }
    }
    return true;
}

bool SC::Build::ProjectWriter::write(StringView destinationDirectory, StringView filename)
{
    String normalizedDirectory = StringEncoding::Utf8;
    {
        Vector<StringView> components;
        SC_TRY_IF(Path::normalize(destinationDirectory, components, &normalizedDirectory, Path::Type::AsPosix));
    }
    FileSystem fs;
    SC_TRY_IF(fs.init(normalizedDirectory.view()));
    String prjName;
    switch (parameters.generator)
    {
    case Generator::XCode14: {
        String        buffer1;
        StringBuilder sb1(buffer1);
        WriterXCode   writer(definition, definitionCompiler);
        SC_TRY_IF(writer.write(sb1, normalizedDirectory.view()));
        SC_TRY_IF(StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj", filename));
        SC_TRY_IF(fs.makeDirectoryIfNotExists({prjName.view()}));
        SC_TRY_IF(StringBuilder(prjName, StringBuilder::Clear).format("{}.xcodeproj/project.pbxproj", filename));
        SC_TRY_IF(fs.removeFileIfExists(prjName.view()));
        SC_TRY_IF(fs.write(prjName.view(), buffer1.view()));
        break;
    }
    case Generator::VisualStudio2022: {
        String buffer1;
        SC_TRY_IF(StringBuilder(prjName, StringBuilder::Clear).format("{}.vcxproj", filename));
        WriterVisualStudio           writer(definition, definitionCompiler);
        WriterVisualStudio::Renderer renderer;
        const Project&               project = definition.workspaces[0].projects[0];
        SC_TRY_IF(writer.prepare(normalizedDirectory.view(), project, renderer));
        SC_TRY_IF(writer.generateGuidFor(project.name.view(), writer.hashing, writer.projectGuid));
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY_IF(writer.writeProject(sb1, project, renderer));
            SC_TRY_IF(fs.removeFileIfExists(prjName.view()));
            SC_TRY_IF(fs.write(prjName.view(), buffer1.view()));
        }
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY_IF(writer.writeFilters(sb1, renderer));
            String prjFilterName;
            SC_TRY_IF(StringBuilder(prjFilterName, StringBuilder::Clear).format("{}.vcxproj.filters", filename));
            SC_TRY_IF(fs.removeFileIfExists(prjFilterName.view()));
            SC_TRY_IF(fs.write(prjFilterName.view(), buffer1.view()));
        }
        {
            StringBuilder sb1(buffer1, StringBuilder::Clear);
            SC_TRY_IF(writer.writeSolution(sb1, prjName.view(), project));
            String slnName;
            SC_TRY_IF(StringBuilder(slnName, StringBuilder::Clear).format("{}.sln", filename));
            SC_TRY_IF(fs.removeFileIfExists(slnName.view()));
            SC_TRY_IF(fs.write(slnName.view(), buffer1.view()));
        }
        break;
    }
    }

    return true;
}
