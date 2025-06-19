#include "CGEngine.h"

llvm::Module* Overcast::CodeGen::CGEngine::Generate(const std::vector<std::unique_ptr<Statement>>& statements)
{
	// import printf from C
	llvm::FunctionType* printType = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(context),
		{ llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
		true
	);

	llvm::Function::Create(printType, llvm::Function::ExternalLinkage, "printf", this->module.get());

	for (auto& statement : statements)
	{
		GenerateStatement(*statement);
	}

	return this->module.get();
}

void Overcast::CodeGen::CGEngine::EmitToObjectFile(const std::string& outputFile, llvm::Module* module)
{
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	auto TargetTriple = llvm::sys::getDefaultTargetTriple();
	module->setTargetTriple(TargetTriple);

	std::string Error;
	auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

	auto CPU = "generic";
	auto Features = "";

	llvm::TargetOptions opt;
	auto RM = std::optional<llvm::Reloc::Model>();
	auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

	module->setDataLayout(TargetMachine->createDataLayout());

	llvm::legacy::PassManager pass;

	llvm::PassBuilder passBuilder;
	llvm::LoopAnalysisManager loopAM;
	llvm::FunctionAnalysisManager functionAM;
	llvm::CGSCCAnalysisManager cgsccAM;
	llvm::ModuleAnalysisManager moduleAM;

	passBuilder.registerModuleAnalyses(moduleAM);
	passBuilder.registerCGSCCAnalyses(cgsccAM);
	passBuilder.registerFunctionAnalyses(functionAM);
	passBuilder.registerLoopAnalyses(loopAM);
	passBuilder.crossRegisterProxies(loopAM, functionAM, cgsccAM, moduleAM);

	llvm::ModulePassManager modulePM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);

	modulePM.run(*module, moduleAM);

	std::error_code EC;
	llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);

	if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
		llvm::errs() << "TargetMachine can't emit a file of this type\n";
		return;
	}

	pass.run(*module);
	dest.flush();
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateStatement(Statement& statement)
{
	if (auto func = dynamic_cast<FunctionDeclStatement*>(&statement))
	{
		return GenerateFunction(*func);
	}
	else if (auto expr = dynamic_cast<ExpressionStatement*>(&statement))
	{
		return GenerateExpression(*expr->EncapsulatedExpr.get());
	}
	else if (auto ret = dynamic_cast<ReturnStatement*>(&statement))
	{
		return GenerateReturn(*ret);
	}
	else if (auto varDecl = dynamic_cast<VariableDeclStatement*>(&statement))
	{
		return GenerateVarDecl(*varDecl);
	}
	else if (auto constDecl = dynamic_cast<ConstDeclStatement*>(&statement))
	{
		throw std::runtime_error("Const declarations are not supported yet.");
	}
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateFunction(const FunctionDeclStatement& funcDecl)
{
	std::vector<llvm::Type*> paramTypes;
	for (auto& param : funcDecl.Parameters)
	{
		paramTypes.push_back(GetLLVMType(*param.ParameterType));
	}

	llvm::Type* returnType = GetLLVMType(*funcDecl.ReturnType);
	llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
	llvm::Function* function = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, funcDecl.FuncName, module.get());

	if (funcDecl.IsExtern)
	{
		function->setLinkage(llvm::Function::ExternalLinkage); // just to make sure
		return function;
	}

	for (auto& arg : function->args()) {
		arg.setName(funcDecl.Parameters[arg.getArgNo()].ParameterName);
		symbolTable[arg.getName().str()] = &arg;
	}

	llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(context, "entry", function);
	builder.SetInsertPoint(entryBlock);

	currentFunction = function;
	for (auto& stmt : funcDecl.Body)
	{
		GenerateStatement(*stmt);
	}

	if (returnType->isVoidTy())
	{
		builder.CreateRetVoid();
	}

	currentFunction = nullptr;

	return function;
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateReturn(const ReturnStatement& retDecl)
{
	auto returnValue = GenerateExpression(*retDecl.ReturnValue.get());
	return builder.CreateRet(returnValue);
}

llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function* func, llvm::Type* type, const std::string& varName) { // helper
	llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
	return tmpBuilder.CreateAlloca(type, nullptr, varName);
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateVarDecl(const VariableDeclStatement& varDecl)
{
	auto* varAlloca = CreateEntryBlockAlloca(currentFunction, GetLLVMType(*varDecl.VariableType), "var:"+varDecl.VarName);
	if (varDecl.Defined && varDecl.DefaultValue)
	{
		auto* initValue = GenerateExpression(*varDecl.DefaultValue.get());
		builder.CreateStore(initValue, varAlloca);
	}

	symbolTable[varDecl.VarName] = varAlloca;
	return varAlloca;
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateExpression(Expression& expression)
{
	if (auto invFunc = dynamic_cast<InvokeFunctionExpr*>(&expression))
	{
		return GenerateFunctionCall(*invFunc);
	}
	else if (auto strExpr = dynamic_cast<StringLiteralExpr*>(&expression))
	{
		llvm::Value* strValue = builder.CreateGlobalStringPtr(strExpr->LiteralValue, ".str");
		return strValue;
	}
	else if (auto intExpr = dynamic_cast<IntLiteralExpr*>(&expression))
	{
		llvm::Value* intValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), intExpr->LiteralValue);
		return intValue;
	}
	else if (auto varExpr = dynamic_cast<VariableUseExpr*>(&expression))
	{
		if (symbolTable.find(varExpr->VariableName) == symbolTable.end())
		{
			throw std::runtime_error("Variable " + varExpr->VariableName + " not found in symbol table.");
		}
		if (symbolTable[varExpr->VariableName]->getName().starts_with("var:")) // var check
		{
			return builder.CreateLoad(symbolTable[varExpr->VariableName]->getType(), symbolTable[varExpr->VariableName], varExpr->VariableName);
		}

		return symbolTable[varExpr->VariableName];
	}
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateFunctionCall(const InvokeFunctionExpr& funcCall)
{
	llvm::Function* function = module->getFunction(funcCall.InvokedFunctionName);

	if (funcCall.InvokedFunctionName == "print") // check for the print function, we need to replace that with printf
	{
		function = module->getFunction("printf");
	}

	if (!function)
	{
		throw std::runtime_error("Function " + funcCall.InvokedFunctionName + " not found.");
	}

	std::vector<llvm::Value*> args;
	for (auto& arg : funcCall.Arguments)
	{
		args.push_back(GenerateExpression(*arg));
	}
	return builder.CreateCall(function, args, function->getReturnType()->isVoidTy() ? "" : "calltmp");
}

llvm::Type* Overcast::CodeGen::CGEngine::GetLLVMType(OCType& ocType)
{
	if (auto type = dynamic_cast<PointerType*>(&ocType))
	{
		return llvm::PointerType::get(GetLLVMType(*type->OfType), 0);
	}
	else if (auto type = dynamic_cast<IdentifierType*>(&ocType))
	{
		// handle primitives first:
		if (type->TypeName == "int")
		{
			return llvm::Type::getInt32Ty(context);
		}
		else if (type->TypeName == "float")
		{
			return llvm::Type::getFloatTy(context);
		}
		else if (type->TypeName == "double")
		{
			return llvm::Type::getDoubleTy(context);
		}
		else if (type->TypeName == "void")
		{
			return llvm::Type::getVoidTy(context);
		}
		else if (type->TypeName == "string")
		{
			return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
		}
		else if (type->TypeName == "byte")
		{
			return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0); 
		}
		else if (type->TypeName == "bool")
		{
			return llvm::Type::getInt1Ty(context);
		}
		else if (type->TypeName == "char")
		{
			return llvm::Type::getInt8Ty(context);
		}
		else if (type->TypeName == "void*")
		{
			return llvm::PointerType::get(llvm::Type::getVoidTy(context), 0);
		}
		else
		{
			throw std::runtime_error("Unknown type: " + type->TypeName);
		}
		// later handle user-defined types
	}
}

Overcast::CodeGen::CGEngine::~CGEngine()
{
}