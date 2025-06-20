#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Overcast/SyntaxAnalysis/statements.h"
#include "Overcast/SyntaxAnalysis/expressions.h"

namespace llvm {
	class LLVMContext;
	class Module;
	class Function;
	class Value;
	class Type;
}

namespace Overcast::CodeGen {
	struct StructDef
	{
		struct StructMember
		{
			llvm::Type* Type;
			std::string Name;
			int Index;
		};
		llvm::Type* StructType;
		std::map<std::string, StructMember> StructMembers;
	};

	class CGEngine {
	private:
		llvm::LLVMContext context;
		llvm::IRBuilder<> builder;
		std::unique_ptr<llvm::Module> module;
		llvm::Function* currentFunction = nullptr;

		std::unordered_map<std::string, llvm::Value*> symbolTable;
		std::unordered_map<std::string, StructDef> structDefTable;

		llvm::Value* GenerateStatement(Statement& statement);
		llvm::Value* GenerateFunction(const FunctionDeclStatement& funcDecl);
		llvm::Value* GenerateReturn(const ReturnStatement& retDecl);
		llvm::Value* GenerateStructDecl(const StructDeclStatement& strDecl);
		llvm::Value* GenerateVarDecl(const VariableDeclStatement& varDecl);
		llvm::Value* GenerateVarSet(const VariableSetStatement& varSet);
		llvm::Value* GenerateIfStatement(const IfStatement& ifStmt);
		llvm::Value* GenerateExpression(Expression& expression);
		llvm::Value* GenerateFunctionCall(const InvokeFunctionExpr& funcCall);
		llvm::Type* GetLLVMType(OCType& ocType);
	public:
		~CGEngine();

		llvm::Module* Generate(const std::vector<std::unique_ptr<Statement>>& statements);
		void EmitToObjectFile(const std::string& outputFile, llvm::Module* module);

		CGEngine(const std::string& moduleName)
			: builder(context), module(std::make_unique<llvm::Module>(moduleName, context)) {
		}
	};
};