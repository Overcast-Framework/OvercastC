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
	unsigned long allTimes = 0;
	for (int i = 0; i < 100; i++)
	{
		auto bstart = std::chrono::high_resolution_clock::now();
		std::ifstream inFile("examples/hello_world.oc");
		std::string code((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
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
			//codeGen.EmitToObjectFile("helloworld.obj", module);
			//module->print(llvm::errs(), nullptr);
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
		<< (allTimes / 100)/1000000
		<< " ms\n";

	std::cout << "Total time: "
		<< allTimes
		<< " ns\n";

	return 0;
}