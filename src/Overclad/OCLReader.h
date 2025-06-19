#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <vector>

#define CREATE_INCLUDE(inc) m_GenFeed << "#include " << inc << "\n";
#define H_CREATE_INCLUDE(inc) m_HGenFeed << "#include " << inc << "\n";
#define CREATE_FUNC_PROTO(type, name, params) m_HGenFeed << type << " " << name << "(" << params << ")" << ";\n";
#define CREATE_FUNC_SIG(type, name, params) m_GenFeed << type << " " << name << "(" << params << ")" << "\n{\n"; // params are just p,a,r,a,m,s
#define CLOSE_SCOPE() m_GenFeed << "}\n";
#define H_CLOSE_SCOPE() m_HGenFeed << "};\n";
#define CREATE_ENUM_ENTRY(entry, defaultValue) m_HGenFeed << "\t" << entry << ((defaultValue != -1) ? (" = " + std::to_string(defaultValue)) : "") << ",\n";
#define CREATE_VAR(type, name, value) m_GenFeed << "\t" << type << " " << name << ((value != "null") ? (" = " + std::to_string(value)) : "") << ";\n";

namespace Overclad::OCLAnalysis
{
	struct LexemeEntry
	{
		std::string TokenTypeName;
		std::string RegexPattern;
	};

	class OCLReader
	{
	public:
		std::string GenerateCXXCode(std::string path);
		std::stringstream m_GenFeed;
		std::stringstream m_HGenFeed;
	private:
		LexemeEntry ParseLexemeEntry(std::string line);
		void BeginReadProc();
		void BuildFile();

		std::ifstream m_OCLFeed;
		std::vector<LexemeEntry> m_Lexemes;
	};
}