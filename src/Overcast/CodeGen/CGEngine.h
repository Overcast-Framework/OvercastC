#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/TargetParser/Triple.h"                   
#include "llvm/IR/LLVMContext.h"               
#include "llvm/IR/Module.h"                   
#include "llvm/Support/TargetSelect.h"                      
#include "llvm/MC/TargetRegistry.h"        
#include "llvm/Target/TargetMachine.h"           
#include "llvm/Target/TargetOptions.h"            
#include "llvm/Support/raw_ostream.h"            
#include "llvm/Passes/PassBuilder.h"            
#include "llvm/IR/LegacyPassManager.h"          
#include "llvm/Support/FileSystem.h"  
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/Error.h"                  
#include "Overcast/SyntaxAnalysis/statements.h"
#include "Overcast/SyntaxAnalysis/expressions.h"
#include <iostream>
#include <optional>

namespace Overcast::CodeGen {
	class CGEngine {
	private:
		llvm::LLVMContext context;
		llvm::IRBuilder<> builder;
		std::unique_ptr<llvm::Module> module;
		llvm::Function* currentFunction = nullptr;

		std::unordered_map<std::string, llvm::Value*> symbolTable;

		llvm::Value* GenerateStatement(Statement& statement);
		llvm::Value* GenerateFunction(const FunctionDeclStatement& funcDecl);
		llvm::Value* GenerateReturn(const ReturnStatement& retDecl);
		llvm::Value* GenerateVarDecl(const VariableDeclStatement& varDecl);
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