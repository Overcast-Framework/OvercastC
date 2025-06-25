#include "ocpch.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <sstream>
#include "lexer.h"
#include "Overcast/SyntaxAnalysis/parser.h"
#include "Overcast/SyntaxAnalysis/types.h"
#include "Overcast/SemanticAnalysis/binder.h"
#include "Overcast/CodeGen/CGEngine.h"
#include "Overcast/ProjectSystem/project_system.h"
#include "oc_examplefiles.h"

void create_project(std::string name, bool nostd, bool noal, bool emit_llvm)
{
	std::filesystem::path cwd = std::filesystem::current_path();
	std::filesystem::create_directory(cwd / name);
	std::filesystem::create_directory(cwd / name / "bin");
	std::filesystem::create_directory(cwd / name / "obj");

	Overcast::ProjectSystem::Project proj;
	proj.ProjectName = name;
	proj.ProjectVersion = Overcast::ProjectSystem::Version::parse("1.0.0");
	proj.CompilerVersion = Overcast::ProjectSystem::Version::parse(OVERCAST_C_VER);
	if(!nostd)
		proj.Dependencies.push_back({ "stdlib", Overcast::ProjectSystem::Version::parse(OVERCAST_C_VER) });
	proj.no_std = nostd;
	proj.emit_llvm = emit_llvm;
	proj.skip_autolink = noal;
	proj.outputFolder = "bin";

	auto tomlConfig = proj.SerializeTOML();
	std::ofstream projectFile(cwd / name / (name + ".ocproj"));
	projectFile << tomlConfig;
	projectFile.close();

	std::ofstream ocFile(cwd / name / "main.oc");
	ocFile << "package " << name << "\n";
	ocFile << OC_EXAMPLE_HELLOWORLD;
	ocFile.close();

	std::cout << "Created project " << name << std::endl;
}

void build_project(std::string projectName, int threadCount)
{
	auto startTime = std::chrono::high_resolution_clock::now();
	std::filesystem::path cwd = std::filesystem::current_path();
	Overcast::ProjectSystem::BuildSystem buildSystem;

	std::filesystem::path projectFilePath;
	// run discovery for the project file
	if (projectName.empty())
	{
		for (const auto& files : std::filesystem::directory_iterator(cwd, std::filesystem::directory_options::skip_permission_denied))
		{
			auto ext = files.path().extension();
			if (ext == ".ocproj")
			{
				projectFilePath = files;
				break;
			}
		}
	}
	else
	{
		projectFilePath = cwd / (projectName + ".ocproj");
		if (!std::filesystem::exists(projectFilePath))
		{
			std::cerr << "Could not find a project at the path " << projectFilePath << std::endl;
			return;
		}
	}

	std::ifstream inFile(projectFilePath);
	std::string projectTOML(std::istreambuf_iterator<char>(inFile), {});
	auto project = Overcast::ProjectSystem::Project::LoadFromTOML(projectTOML);

	std::cout << "Building project " << project.ProjectName << "..." << std::endl;
	std::cout << "Discovering source files..." << std::endl;

	for (std::filesystem::recursive_directory_iterator i(cwd, std::filesystem::directory_options::skip_permission_denied), end; i != end; ++i)
	{
		if (!std::filesystem::is_directory(i->path()))
		{
			if(i->path().extension() == ".oc")
				buildSystem.AddBuildFile(i->path().string(), {}); // feed them into the build system, dependency discovery happens later.
		}
	}

	std::cout << "Building..." << std::endl;
	auto buildResult = buildSystem.RunBuild(project.ProjectName);
	std::cout << buildResult.BuildMessage << std::endl;
	if (!buildResult.IsSuccess())
	{
		std::cerr << "An error occured during the build process: " + buildResult.GetErrors() << std::endl;
		return;
	}

	auto endTime = std::chrono::high_resolution_clock::now();

	std::cout << "Build completed in: "
		<< (float)((endTime - startTime).count()) / 1000000.0f
		<< " ms\n";
}

int main(int argc, char* argv[])
{
	try
	{
		cxxopts::Options opts("overcast", "[build/create/clean] (project name)? (-emit-llvm/-no_std/-no_autolink)? (-c [Debug/Release]) (-t <thread count>)?");

		opts.add_options()
			("emit-llvm", "Emit LLVM IR")          
			("no_std", "Disable standard library")   
			("no_autolink", "Disable autolink")  
			("c", "Set configuration for build", cxxopts::value<std::string>()->default_value("Debug"))
			("t", "Thread count", cxxopts::value<int>()->default_value(std::to_string(std::thread::hardware_concurrency())))
			("help", "Print help");

		opts.parse_positional({ "command", "project" });

		opts.add_options()
			("command", "Command to execute (build/create/clean)", cxxopts::value<std::string>())
			("project", "Project name (optional)", cxxopts::value<std::string>()->default_value(""));

		auto result = opts.parse(argc, argv);
		if (result.count("help"))
		{
			std::cout << opts.help();
			return 0;
		}

		if (result.count("command"))
		{
			std::string command = result["command"].as<std::string>();
			if (command == "create")
			{
				std::string projectName = result["project"].as<std::string>();
				if (projectName.empty())
				{
					std::cerr << "A project name must be supplied to 'create' when creating a new project." << std::endl;
					return -1;
				}

				create_project(projectName, result.count("no_std") > 0, result.count("no_autolink") > 0, result.count("emit-llvm") > 0);
			}
			else if (command == "build")
			{
				std::string projectName = result["project"].as<std::string>();
				int threadCount = std::thread::hardware_concurrency();
				if(result.count("t,threads"))
					threadCount = result["t,threads"].as<int>();
				build_project(projectName, threadCount);
			}
			else if (command == "clean")
			{

			}
		}
	}
	catch (const cxxopts::exceptions::exception & e) {
		std::cerr << "Error parsing options: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}