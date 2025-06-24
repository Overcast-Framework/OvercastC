#include "ocpch.h"
#include "project_system.h"

using namespace std::literals;

std::string Overcast::ProjectSystem::Project::SerializeTOML()
{
    toml::table tbl;

    toml::table projData;
    projData.insert("ProjectName", ProjectName);
    projData.insert("ProjectVersion", ProjectVersion.to_string());
    projData.insert("LangVersion", ProjectVersion.to_string());

    toml::array depDirs;
    
    for (const auto& depDir : DependencyDirectories)
        depDirs.push_back(depDir);

    projData.insert("DependencyDirectories", depDirs);

    toml::table deps;
    for (const auto& dep : Dependencies)
    {
        deps.insert(dep.Name, dep.Version.to_string());
    }

    toml::table buildFlags;
    buildFlags.insert("no_std", no_std);
    buildFlags.insert("emit_llvm", emit_llvm);
    buildFlags.insert("no_autolink", skip_autolink);
    buildFlags.insert("OutputDirectory", outputFolder);

    tbl.insert("BuildInfo", buildFlags);
    tbl.insert("Dependencies", deps);
    tbl.insert("Project", projData);

    std::ostringstream outBuf;
    outBuf << tbl;

    return outBuf.str();
}

Overcast::ProjectSystem::Project Overcast::ProjectSystem::Project::LoadFromTOML(std::string toml)
{
    auto tbl = toml::parse(toml);
    
    Project p;
    p.ProjectName = tbl["Project"]["ProjectName"].value_or("<unknown>");
    p.ProjectVersion = Version::parse(tbl["Project"]["ProjectVersion"].value_or("1.0.0"));
    p.CompilerVersion = Version::parse(tbl["Project"]["LangVersion"].value_or("1.0.0"));
    
    auto depDirsArray = tbl["Project"]["DependencyDirectories"].as_array();
    for (const auto& dir : *depDirsArray)
    {
        p.DependencyDirectories.push_back(dir.as_string()->get());
    }

    for (const auto& i : *tbl["Dependencies"].as_table())
    {
        if (i.second.is_string())
            p.Dependencies.push_back({ std::string(i.first.str()), Version::parse(i.second.as_string()->get()) });
    }

    p.no_std = tbl["BuildInfo"]["no_std"].as_boolean();
    p.emit_llvm = tbl["BuildInfo"]["emit_llvm"].as_boolean();
    p.skip_autolink = tbl["BuildInfo"]["no_autolink"].as_boolean();
    p.outputFolder = tbl["BuildInfo"]["OutputDirectory"].as_string()->get();

    return p;
}
