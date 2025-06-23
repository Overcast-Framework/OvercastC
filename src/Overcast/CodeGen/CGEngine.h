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
			OCType* SemanticType;
		};
		llvm::Type* StructType;
		std::map<std::string, StructMember> StructMembers;
		OCType* SemanticType;
	};

	struct SymbolDef
	{
		llvm::Type* type; // may add more
	};

	struct PhiVariable
	{
		llvm::Value* value;
		llvm::Type* type;
		std::string name;
	};

	struct CGResult
	{
		llvm::Value* value;
		llvm::Type* type;
		OCType* semanticType;
		llvm::Value* structObject;
	};

	class CGEngine {
	private:
		llvm::LLVMContext context;
		llvm::IRBuilder<> builder;
		std::unique_ptr<llvm::Module> module;
		llvm::Function* currentFunction = nullptr;

		bool RequestPointerAccess = false; // a lil' flag for struct access
		bool RequestFunctionAccess = false; // same as above
		bool analyzePhiVariables = false; // for looping

		std::unordered_map<std::string, llvm::Value*> symbolTable;
		std::unordered_map<std::string, llvm::PHINode*> phiTable;
		std::unordered_map<std::string, SymbolDef> typedSymbolTable;
		std::unordered_map<std::string, OCType*> semanticTypeTable;
		std::unordered_map<std::string, StructDef> structDefTable;

		std::vector<PhiVariable> PhiVariableList; // this gets cleared for each analysis

		llvm::BasicBlock* currentLoopBlock = nullptr;

		llvm::Value* GenerateStatement(Statement& statement);
		llvm::Value* GenerateFunction(const FunctionDeclStatement& funcDecl);
		llvm::Value* GenerateReturn(const ReturnStatement& retDecl);
		llvm::Value* GenerateStructDecl(const StructDeclStatement& strDecl);
		llvm::Value* GenerateVarDecl(const VariableDeclStatement& varDecl);
		llvm::Value* GenerateVarSet(const AssignmentStatement& varSet);
		llvm::Value* GenerateIfStatement(const IfStatement& ifStmt);
		std::string AnalyzeExpression(Expression& expression);
		std::vector<PhiVariable> AnalyzePHIVariables(const std::vector<std::unique_ptr<Statement>>& statements);
		llvm::Value* GenerateWhileStatement(const WhileStatement& whStmt, llvm::BasicBlock* parentCondition = nullptr);
		CGResult GenerateExpression(Expression& expression);
		CGResult GenerateFunctionCall(const InvokeFunctionExpr& funcCall);
		CGResult GenerateStructCtor(StructCtorExpr* strCtorExpr, llvm::Value* overridePtr = nullptr);
		llvm::Type* GetLLVMType(OCType& ocType);
		llvm::Value* GetStructMemberPointer(const std::string& structName, llvm::Value* structInst, const std::string& memberName);
	public:
		~CGEngine();

		llvm::Module* Generate(const std::vector<std::unique_ptr<Statement>>& statements);
		void EmitToObjectFile(const std::string& outputFile, llvm::Module* module);

		CGEngine(const std::string& moduleName)
			: builder(context), module(std::make_unique<llvm::Module>(moduleName, context)) {
		}
	};
};