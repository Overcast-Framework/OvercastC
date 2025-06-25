#pragma once
#include <iostream>
#include <sstream>
#include <future>
#include <unordered_map>
#include <filesystem>
#include "Overcast/lexer.h"
#include "Overcast/SyntaxAnalysis/parser.h"
#include "Overcast/SemanticAnalysis/binder.h"
#include "Overcast/CodeGen/CGEngine.h"

namespace Overcast::ProjectSystem
{
	struct Version
	{
		int Major = 1, Minor = 0, Patch = 0;
		// Tag -> rc/dev/alpha/beta/build/etc.
		std::string PreRelease;
		std::string BuildMeta;

		static inline Version parse(const std::string& str)
		{
			Version ver;
			size_t pos = 0;

			size_t dot1 = str.find('.', pos);
			if (dot1 == std::string::npos) throw std::runtime_error("Invalid version string");
			ver.Major = std::stoi(str.substr(pos, dot1 - pos));
			pos = dot1 + 1;

			size_t dot2 = str.find('.', pos);
			if (dot2 == std::string::npos) throw std::runtime_error("Invalid version string");
			ver.Minor = std::stoi(str.substr(pos, dot2 - pos));
			pos = dot2 + 1;

			size_t dash = str.find('-', pos);
			size_t plus = str.find('+', pos);
			size_t endPatch = std::min(dash == std::string::npos ? str.size() : dash,
				plus == std::string::npos ? str.size() : plus);

			ver.Patch = std::stoi(str.substr(pos, endPatch - pos));
			pos = endPatch;

			if (pos < str.size() && str[pos] == '-')
			{
				++pos;
				size_t nextPlus = str.find('+', pos);
				ver.PreRelease = str.substr(pos, nextPlus == std::string::npos ? std::string::npos : nextPlus - pos);
				pos += ver.PreRelease.size();
			}

			if (pos < str.size() && str[pos] == '+')
			{
				++pos;
				ver.BuildMeta = str.substr(pos);
			}

			return ver;
		}

		std::string to_string() const
		{
			auto str = std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch);
			if (PreRelease != "")
			{
				str += "-" + PreRelease;
			}
			if (BuildMeta != "")
			{
				str += "+" + BuildMeta;
			}
			return str;
		}
	};

	class Project
	{
	public:
		struct Dependency
		{
			std::string Name;
			Version Version;
		};

		std::string ProjectName;
		Version ProjectVersion;
		Version CompilerVersion;

		std::vector<std::string> DependencyDirectories;
		std::vector<Dependency> Dependencies;

		bool no_std = false;
		bool emit_llvm = false;
		bool skip_autolink = false;

		std::string outputFolder;

		std::string SerializeTOML();
		static Project LoadFromTOML(std::string toml);
	};

	struct BuildResult
	{
		enum class BuildState { SUCCESS, FAILURE } State;
		std::string BuildMessage;
		std::string ObjectFilePath;
		std::vector<std::unique_ptr<Statement>> ASTresult;
		std::unordered_map<std::string, Overcast::Semantic::Binder::Symbol> GlobalSymbols;

		bool IsSuccess() const
		{
			return State == BuildState::SUCCESS;
		}

		std::string GetErrors() const
		{
			return BuildMessage;
		}

		// Constructor(s)
		BuildResult(BuildState state, std::string buildMessage, std::string objectFilePath, std::vector<std::unique_ptr<Statement>>&& astRes)
			: State(state), BuildMessage(std::move(buildMessage)), ObjectFilePath(std::move(objectFilePath)), ASTresult(std::move(astRes)) {
		}

		BuildResult(BuildState state, std::string buildMessage)
			: State(state), BuildMessage(std::move(buildMessage)), ObjectFilePath(""), ASTresult() {
		}

		BuildResult(BuildState state)
			: State(state), BuildMessage(""), ObjectFilePath(""), ASTresult() {
		}

		// Explicitly delete copy operations
		BuildResult(const BuildResult&) = delete;
		BuildResult& operator=(const BuildResult&) = delete;

		// Explicitly allow move operations
		BuildResult(BuildResult&&) noexcept = default;
		BuildResult& operator=(BuildResult&&) noexcept = default;
	};

	class BuildProcess
	{
	public:
		std::string buildFilePath;
		Overcast::Parser::Parser parser;

		std::mutex coutMutex;

		bool EmitLLVM = false;

		std::shared_ptr<BuildResult> Build();

		bool IsComplete()
		{
			return _isBuildComplete;
		}

		BuildProcess(const std::string& filePath) : 
			buildFilePath(filePath)
		{}
	private:
		bool _isBuildComplete = false;
	};

	class ThreadPool
	{
	private:
		std::vector<std::thread> workers;
		std::queue<std::function<void()>> tasks;

		std::mutex queueMutex;
		std::condition_variable condition;
		bool stop = false;

		std::atomic<int> tasksInProgress{ 0 };
	public:
		ThreadPool(uint32_t numThreads);
		~ThreadPool();
		std::shared_future<std::shared_ptr<BuildResult>> Submit(std::function<std::shared_ptr<BuildResult>()> task);
		void WaitAll();
	};

	class BuildSystem
	{
	private:
		std::unordered_map<std::string, std::shared_ptr<BuildProcess>> processes;
		std::unordered_map<std::string, std::vector<std::string>> dependencies;
	public:
		void AddBuildFile(const std::string& file, const std::vector<std::string>& deps);
		BuildResult RunBuild(std::string projectName, uint32_t numThreads = std::thread::hardware_concurrency());
	};
};