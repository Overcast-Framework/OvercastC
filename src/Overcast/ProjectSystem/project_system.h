#pragma once
#include <iostream>
#include <sstream>

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
};