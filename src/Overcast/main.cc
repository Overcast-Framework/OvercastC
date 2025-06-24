#include "ocpch.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <sstream>
#include "lexer.h"
#include "Overcast/SyntaxAnalysis/parser.h"
#include "Overcast/SyntaxAnalysis/types.h"
#include "Overcast/SemanticAnalysis/binder.h"
#include "Overcast/CodeGen/CGEngine.h"
#include "Overcast/ProjectSystem/project_system.h"

std::string agetTokenName(TokenType tok)
{
	auto it = tokenNames.find(tok);
	if (it != tokenNames.end()) {
		std::string tokenStr = it->second;
		return tokenStr;
	}
	else {
		return "<unknown/eof>";
	}
}

void printFunc(const std::unique_ptr<Statement>& stmt) {
	FunctionDeclStatement* func = dynamic_cast<FunctionDeclStatement*>(stmt.get());

	if (func) {
		std::cout << "Parsed function: " << func->FuncName << " with " << func->Parameters.size() << " parameters." << std::endl;

		for (const auto& param : func->Parameters) {
			std::cout << "  Param: " << param.ParameterName << " of type " << param.ParameterType->to_string() << std::endl;
		}
	}
}

int main()
{
	Overcast::ProjectSystem::Project proj;
	proj.ProjectName = "HelloWorld";
	proj.ProjectVersion = Overcast::ProjectSystem::Version::parse("1.0.0-dev+20250624");
	proj.CompilerVersion = Overcast::ProjectSystem::Version::parse("1.0.0");
	proj.DependencyDirectories.push_back("C:\\path\\to\\stdlib");
	proj.Dependencies.push_back({ "stdlib", Overcast::ProjectSystem::Version::parse("1.0.0") });
	proj.Dependencies.push_back({ "test_dep", Overcast::ProjectSystem::Version::parse("1.39.40-rc1+31000825") });
	proj.no_std = false;
	proj.emit_llvm = true;
	proj.skip_autolink = false;
	proj.outputFolder = "bin/Release";

	auto project = Overcast::ProjectSystem::Project::LoadFromTOML(proj.SerializeTOML());
	std::cout << project.ProjectName << std::endl;

	unsigned long allTimes = 0;
	for (int i = 0; i < 1; i++)
	{
		auto bstart = std::chrono::high_resolution_clock::now();
		std::ifstream inFile("examples/hello_world.oc", std::ios::in | std::ios::binary);
		if (!inFile) {
			std::cerr << "Failed to open file\n";
			return 1;
		}

		inFile.seekg(0, std::ios::end);
		size_t size = inFile.tellg();
		inFile.seekg(0, std::ios::beg);

		std::string code(size, '\0');
		inFile.read(&code[0], size);
		auto start = std::chrono::high_resolution_clock::now();
		auto tokens = LexAll(code);
		auto end = std::chrono::high_resolution_clock::now();
		/*std::cout << "Lexer time: "
			<< std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
			<< " ns\n";*/
		try
		{
			start = std::chrono::high_resolution_clock::now();
			Overcast::Parser::Parser parser(tokens);
			auto AST = parser.Parse();
			end = std::chrono::high_resolution_clock::now();
			/*std::cout << "Parse time: "
				<< std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
				<< " ns\n";*/
			start = std::chrono::high_resolution_clock::now();
			Overcast::Semantic::Binder::Binder binder;
			binder.Run(AST);
			end = std::chrono::high_resolution_clock::now();
			/*std::cout << "Bind time: "
				<< std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
				<< " ns\n";*/

			start = std::chrono::high_resolution_clock::now();
			Overcast::CodeGen::CGEngine codeGen("helloworld");
			auto module = codeGen.Generate(AST);
			end = std::chrono::high_resolution_clock::now();
			codeGen.EmitToObjectFile("helloworld.obj", module);
			module->print(llvm::errs(), nullptr);
			end = std::chrono::high_resolution_clock::now();

			/*std::cout << "CodeGen time: "
				<< std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
				<< " ns\n";*/

			auto bend = std::chrono::high_resolution_clock::now();
			allTimes += std::chrono::duration_cast<std::chrono::nanoseconds>(bend - bstart).count();
		}
		catch (Overcast::Parser::SyntaxError syntaxError)
		{
			std::cerr << syntaxError.what() << std::endl;
			return 1;
		}
		catch (std::runtime_error error)
		{
			std::cerr << error.what() << std::endl;
			return 1;
		}
	}

	std::cout << "Average time: "
		<< (float)(allTimes)/1000000.0f
		<< " ms\n";

	std::cout << "Total time: "
		<< allTimes
		<< " ns\n";

	return 0;
}