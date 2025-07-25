#include "ocpch.h"
#include "CGEngine.h"

llvm::Module* Overcast::CodeGen::CGEngine::Generate(std::unordered_map<std::string, Overcast::Semantic::Binder::Symbol> globalSymbols, const std::vector<std::unique_ptr<Statement>>& statements)
{
	// import printf from C
	llvm::FunctionType* printType = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(context),
		{ llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
		true
	);

	auto* func = llvm::Function::Create(printType, llvm::Function::ExternalLinkage, "printf", this->module.get());
	symbolTable["func:print"] = func;

	for (const auto& s : globalSymbols)
	{
		auto symbol = s.second;
		if (symbol.Kind == Overcast::Semantic::Binder::SymbolKind::Function)
		{
			std::vector<llvm::Type*> parameters;

			for (const auto& param : symbol.ParamTypes)
			{
				parameters.push_back(GetLLVMType(*param));
			}

			llvm::FunctionType* fType = llvm::FunctionType::get(
				GetLLVMType(*symbol.Type),
				parameters,
				false
			);

			auto* _func = llvm::Function::Create(fType, llvm::Function::ExternalLinkage, "func:" + s.first, this->module.get());
			symbolTable["func:"+s.first] = _func;
			typedSymbolTable["func:" + s.first] = { fType->getReturnType() };
			semanticTypeTable["func:" + s.first] = s.second.Type;
		}
		else if (symbol.Kind == Overcast::Semantic::Binder::SymbolKind::Struct)
		{
			std::vector<llvm::Type*> memberVars;
			std::map<std::string, StructDef::StructMember> StructMembers;

			int idx = 0;
			for (const auto& var : symbol.StructSymbols)
			{
				if (var.Kind == Overcast::Semantic::Binder::SymbolKind::Variable)
				{
					memberVars.push_back(GetLLVMType(*var.Type));
					StructMembers[var.Name] = { memberVars.back(), var.Name, idx++, var.Type };
				}
			}

			auto* structType = llvm::StructType::create(memberVars, symbol.Name, false);
			structDefTable[symbol.Name] = { structType, StructMembers, symbol.Type };

			for (const auto& memFunc : symbol.StructSymbols)
			{
				if (memFunc.Kind == Overcast::Semantic::Binder::SymbolKind::Function)
				{
					std::vector<llvm::Type*> parameters;

					for (const auto& param : memFunc.ParamTypes)
					{
						parameters.push_back(GetLLVMType(*param));
					}

					llvm::FunctionType* fType = llvm::FunctionType::get(
						GetLLVMType(*memFunc.Type),
						parameters,
						false
					);

					auto* _func = llvm::Function::Create(fType, llvm::Function::ExternalLinkage, symbol.Name + "::" + memFunc.Name, this->module.get());
					symbolTable[symbol.Name + "::" + memFunc.Name] = _func;
					typedSymbolTable[symbol.Name + "::" + memFunc.Name] = { fType->getReturnType() };
					semanticTypeTable[symbol.Name + "::" + memFunc.Name] = memFunc.Type;
				}
			}
		}
	}

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

	//module->print(llvm::errs(), nullptr);

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
	else if (auto strDecl = dynamic_cast<StructDeclStatement*>(&statement))
	{
		return GenerateStructDecl(*strDecl);
	}
	else if (auto expr = dynamic_cast<ExpressionStatement*>(&statement))
	{
		GenerateExpression(*expr->EncapsulatedExpr.get());
		return nullptr;
	}
	else if (auto ret = dynamic_cast<ReturnStatement*>(&statement))
	{
		return GenerateReturn(*ret);
	}
	else if (auto varDecl = dynamic_cast<VariableDeclStatement*>(&statement))
	{
		return GenerateVarDecl(*varDecl);
	}
	else if (auto varSet = dynamic_cast<AssignmentStatement*>(&statement))
	{
		return GenerateVarSet(*varSet);
	}
	else if (auto ifStmt = dynamic_cast<IfStatement*>(&statement))
	{
		return GenerateIfStatement(*ifStmt);
	}
	else if (auto whStmt = dynamic_cast<WhileStatement*>(&statement))
	{
		return GenerateWhileStatement(*whStmt);
	}
	else if (auto constDecl = dynamic_cast<ConstDeclStatement*>(&statement))
	{
		throw std::runtime_error("Const declarations are not supported yet.");
	}
	else if (dynamic_cast<UseStatement*>(&statement))
	{
		// irrelevant here
	}
	else if (dynamic_cast<PackageDeclStatement*>(&statement))
	{
		// irrelevant here
	}
	else
	{
		throw std::runtime_error("Unsupported statement type for code generation.");
	}
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateFunction(const FunctionDeclStatement& funcDecl)
{
	llvm::Function* function = nullptr;
	llvm::Type* returnType = nullptr;

	if (symbolTable.find("func:"+funcDecl.FuncName) == symbolTable.end())
	{
		std::vector<llvm::Type*> paramTypes;
		for (auto& param : funcDecl.Parameters)
		{
			paramTypes.push_back(GetLLVMType(*param.ParameterType));
		}

		returnType = GetLLVMType(*funcDecl.ReturnType);
		llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
		function = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, funcDecl.FuncName == "main" ? "main" : funcDecl.IsExtern ? funcDecl.FuncName : "func:" + funcDecl.FuncName, module.get());

		if (funcDecl.IsExtern)
		{
			symbolTable.insert({ funcDecl.FuncName == "main" ? "main" : funcDecl.FuncName, function });
			function->setLinkage(llvm::Function::ExternalLinkage); // just to make sure
			return function;
		}
	}
	else
	{
		function = llvm::dyn_cast<llvm::Function>(symbolTable["func:" + funcDecl.FuncName]);
		returnType = typedSymbolTable["func:" + funcDecl.FuncName].type;
	}

	for (auto& arg : function->args()) {
		arg.setName(funcDecl.Parameters[arg.getArgNo()].ParameterName);
		symbolTable[arg.getName().str()] = &arg;
		typedSymbolTable[arg.getName().str()] = { arg.getType() };
		auto it = std::find_if(funcDecl.Parameters.begin(), funcDecl.Parameters.end(), [&](const Parameter& param) {
			return param.ParameterName == arg.getName().str();
			});
		semanticTypeTable[arg.getName().str()] = it->ParameterType.get();
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

	symbolTable.insert({ funcDecl.FuncName == "main" ? "main" : "func:" + funcDecl.FuncName, function });
	semanticTypeTable["func:"+funcDecl.FuncName] = funcDecl.ReturnType.get();

	currentFunction = nullptr;

	return function;
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateReturn(const ReturnStatement& retDecl)
{
	auto returnValue = GenerateExpression(*retDecl.ReturnValue.get());
	return builder.CreateRet(returnValue.value);
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateStructDecl(const StructDeclStatement& strDecl)
{
	// okay, first make the struct type
	std::vector<llvm::Type*> memberTypes;
	std::map<std::string, StructDef::StructMember> StructMembers;

	int idx = 0;
	for (const auto& member : strDecl.Members)
	{
		auto* type = GetLLVMType(*member.ParameterType);
		auto name = member.ParameterName;

		memberTypes.push_back(type);
		StructMembers.insert({ name, {type, name, idx++, member.ParameterType.get() }});
	}

	auto* structType = llvm::StructType::create(module->getContext(), strDecl.StructName);
	structType->setBody(memberTypes, false);
	structDefTable.insert({ strDecl.StructName, { structType, StructMembers, &IdentifierType(strDecl.StructName) } });

	// then the ~member functions~
	for (auto& fDecl : strDecl.MemberFunctions) {
		fDecl->FuncName = strDecl.StructName + "::" + fDecl->FuncName;
		semanticTypeTable[fDecl->FuncName] = fDecl->ReturnType.get();
		GenerateFunction(*fDecl);
	}

	return nullptr;
}

llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function* func, llvm::Type* type, const std::string& varName) { // helper
	llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
	return tmpBuilder.CreateAlloca(type, nullptr, varName);
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateVarDecl(const VariableDeclStatement& varDecl) // because this can be in a loop, we need a condition
{
	if (symbolTable.find(varDecl.VarName) == symbolTable.end())
	{
		auto* varType = GetLLVMType(*varDecl.VariableType);
		llvm::AllocaInst* varAlloca = nullptr;

		if (varDecl.Defined && varDecl.DefaultValue)
		{
			if (!dynamic_cast<StructCtorExpr*>(varDecl.DefaultValue.get()))
			{
				varAlloca = CreateEntryBlockAlloca(currentFunction, varType, "var:" + varDecl.VarName);
				CGResult initValue = GenerateExpression(*varDecl.DefaultValue.get());
				builder.CreateStore(initValue.value, varAlloca);
			}
			else
			{
				CGResult initValue = GenerateExpression(*varDecl.DefaultValue.get());
				varAlloca = llvm::dyn_cast<llvm::AllocaInst>(initValue.value);
			}
		}

		symbolTable[varDecl.VarName] = varAlloca;
		typedSymbolTable[varDecl.VarName] = { varType };
		semanticTypeTable[varDecl.VarName] = { varDecl.VariableType.get() };
		return varAlloca;
	}
	else // it's already 'defined' just scoping rules make it valid
	{
		auto varAlloca = symbolTable[varDecl.VarName];
		auto type = GetLLVMType(*varDecl.VariableType);
		CGResult initValue;
		// just store it
		if (varDecl.Defined && varDecl.DefaultValue)
		{
			if (!dynamic_cast<StructCtorExpr*>(varDecl.DefaultValue.get()))
			{
				initValue = GenerateExpression(*varDecl.DefaultValue.get());
				builder.CreateStore(initValue.value, varAlloca);
			}
			else
			{
				initValue = GenerateExpression(*varDecl.DefaultValue.get());
				varAlloca = llvm::dyn_cast<llvm::AllocaInst>(initValue.value);
			}
		}

		// update the data, just in case
		symbolTable[varDecl.VarName] = varAlloca;
		typedSymbolTable[varDecl.VarName] = { type };
		semanticTypeTable[varDecl.VarName] = { varDecl.VariableType.get() };

		if (phiTable.find(varDecl.VarName) != phiTable.end())
		{
			auto phiNode = phiTable[varDecl.VarName];
			phiNode->addIncoming(initValue.value, builder.GetInsertBlock());
		}

		return varAlloca;
	}
}

#pragma optimize("", off)
llvm::Value* Overcast::CodeGen::CGEngine::GenerateVarSet(const AssignmentStatement& assign)
{
	bool prevPointerState = RequestPointerAccess;
	RequestPointerAccess = true;
	auto inst = GenerateExpression(*assign.LHS);
	RequestPointerAccess = prevPointerState;
	if (!inst.value)
	{
		throw std::runtime_error("Variable not found in symbol table.");
	}

	if (auto ctorExpr = dynamic_cast<StructCtorExpr*>(assign.Value.get()))
	{
		if (inst.value->getName().starts_with_insensitive(".gep"))
		{
			auto value = GenerateStructCtor(ctorExpr, inst.value);
		}
	}
	else
	{
		auto value = GenerateExpression(*assign.Value);
		if (auto varExpr = dynamic_cast<const VariableUseExpr*>(assign.LHS.get()))
		{
			if (phiTable.find(varExpr->VariableName) != phiTable.end())
			{
				auto phiNode = phiTable[varExpr->VariableName];
				phiNode->addIncoming(value.value, builder.GetInsertBlock());
			}
		}
		builder.CreateStore(value.value, inst.value);
	}

	return inst.value;
}
#pragma optimize("", on)

llvm::Value* Overcast::CodeGen::CGEngine::GenerateIfStatement(
	const IfStatement& ifStmt,
	llvm::BasicBlock* mergeBlock)
{
	llvm::Value* condition = GenerateExpression(*ifStmt.Condition.get()).value;

	if (!condition->getType()->isIntegerTy(1)) {
		throw std::runtime_error("Condition in if statement must be of type bool.");
	}

	llvm::Function* function = builder.GetInsertBlock()->getParent();

	if (!mergeBlock) {
		mergeBlock = llvm::BasicBlock::Create(context, "ifcont", function);
	}

	llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(context, "then", function);
	llvm::BasicBlock* elseBlock = nullptr;
	bool hasElse = !ifStmt.ElseBody.empty();

	if (hasElse) {
		elseBlock = llvm::BasicBlock::Create(context, "else", function);
		builder.CreateCondBr(condition, thenBlock, elseBlock);
	}
	else {
		builder.CreateCondBr(condition, thenBlock, mergeBlock);
	}

	builder.SetInsertPoint(thenBlock);
	for (auto& stmt : ifStmt.Body) {
		if (auto* nestedIf = dynamic_cast<IfStatement*>(stmt.get())) {
			GenerateIfStatement(*nestedIf, mergeBlock);
		}
		else {
			GenerateStatement(*stmt);
		}
	}
	if (!builder.GetInsertBlock()->getTerminator()) {
		builder.CreateBr(mergeBlock);
	}

	if (hasElse) {
		builder.SetInsertPoint(elseBlock);
		for (auto& stmt : ifStmt.ElseBody) {
			if (auto* nestedIf = dynamic_cast<IfStatement*>(stmt.get())) {
				GenerateIfStatement(*nestedIf, mergeBlock);
			}
			else {
				GenerateStatement(*stmt);
			}
		}
		if (!builder.GetInsertBlock()->getTerminator()) {
			builder.CreateBr(mergeBlock);
		}
	}

	builder.SetInsertPoint(mergeBlock);

	return nullptr;
}

std::string Overcast::CodeGen::CGEngine::AnalyzeExpression(Expression& expression)
{
	if (auto varExpr = dynamic_cast<const VariableUseExpr*>(&expression))
	{
		return varExpr->VariableName;
	}

	return "<NO>";
}

std::vector<Overcast::CodeGen::PhiVariable> Overcast::CodeGen::CGEngine::AnalyzePHIVariables(const std::vector<std::unique_ptr<Statement>>& statements)
{
	std::vector<PhiVariable> phiVariables;
	for (const auto& stmt : statements)
	{
		if (auto varDeclStatement = dynamic_cast<const VariableDeclStatement*>(stmt.get()))
		{
			std::cout << "hi" << std::endl;
			if (symbolTable.find(varDeclStatement->VarName) != symbolTable.end())
			{
				PhiVariable var;
				var.value = symbolTable[varDeclStatement->VarName];
				var.type = typedSymbolTable[varDeclStatement->VarName].type;
				var.name = varDeclStatement->VarName;

				phiVariables.push_back(var);
			}
		}
		else if (auto assignStmt = dynamic_cast<const AssignmentStatement*>(stmt.get()))
		{
			std::cout << "hi" << std::endl;
			PhiVariable var;
			var.name = AnalyzeExpression(*assignStmt->LHS);
			if (var.name == "<NO>")
				continue;

			var.value = symbolTable[var.name];
			var.type = typedSymbolTable[var.name].type;

			phiVariables.push_back(var);
		}
	}

	return phiVariables;
}

llvm::Value* Overcast::CodeGen::CGEngine::GenerateWhileStatement(const WhileStatement& whStmt, llvm::BasicBlock* exitTarget)
{
	llvm::Function* function = builder.GetInsertBlock()->getParent();

	auto condBlock = llvm::BasicBlock::Create(context, "conditionBlock", function);
	auto loopBlock = llvm::BasicBlock::Create(context, "loopBlock", function);
	auto mergeBlock = llvm::BasicBlock::Create(context, "whileCont", function);

	builder.CreateBr(condBlock);

	// Condition Block
	builder.SetInsertPoint(condBlock);
	auto phiVarList = AnalyzePHIVariables(whStmt.Body);
	auto oldPHITable = phiTable;
	for (auto& phiVar : phiVarList)
	{
		auto phiNode = builder.CreatePHI(phiVar.type, 2, phiVar.name + "_phi");
		phiNode->addIncoming(builder.CreateLoad(phiVar.type, phiVar.value, "loadInitial"), builder.GetInsertBlock());
		phiTable[phiVar.name] = phiNode;
	}

	llvm::Value* condition = GenerateExpression(*whStmt.Condition.get()).value;
	if (!condition->getType()->isIntegerTy(1))
		throw std::runtime_error("Condition in while statement must be of type bool.");

	builder.CreateCondBr(condition, loopBlock, mergeBlock);

	// Loop Block
	builder.SetInsertPoint(loopBlock);
	for (const auto& stmt : whStmt.Body)
	{
		if (auto whileStmt = dynamic_cast<const WhileStatement*>(stmt.get()))
		{
			auto nestedMerge = llvm::dyn_cast<llvm::BasicBlock>(GenerateWhileStatement(*whileStmt, condBlock));
			builder.SetInsertPoint(nestedMerge);
		}
		else
		{
			GenerateStatement(*stmt);
		}
	}

	// Close Loop Block
	if (!builder.GetInsertBlock()->getTerminator())
		builder.CreateBr(condBlock);

	// Merge Block
	builder.SetInsertPoint(mergeBlock);

	phiTable = oldPHITable;
	return mergeBlock;
}

llvm::Value* Overcast::CodeGen::CGEngine::GetStructMemberPointer(const std::string& structName, llvm::Value* structInst, const std::string& memberName)
{
	auto structDef = structDefTable[structName];
	return builder.CreateStructGEP(structDef.StructType, structInst, structDef.StructMembers[memberName].Index, ".gep" + structName + memberName);
}

Overcast::CodeGen::CGResult Overcast::CodeGen::CGEngine::GenerateExpression(Expression& expression)
{
	if (auto invFunc = dynamic_cast<InvokeFunctionExpr*>(&expression))
	{
		return GenerateFunctionCall(*invFunc);
	}
	else if (auto strExpr = dynamic_cast<StringLiteralExpr*>(&expression))
	{
		llvm::Value* strValue = builder.CreateGlobalString(strExpr->LiteralValue, ".str");
		return { strValue, llvm::PointerType::getInt8Ty(context) };
	}
	else if (auto intExpr = dynamic_cast<IntLiteralExpr*>(&expression))
	{
		llvm::Value* intValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), intExpr->LiteralValue);
		return { intValue, llvm::Type::getInt32Ty(context) };
	}
	else if (auto varExpr = dynamic_cast<VariableUseExpr*>(&expression))
	{
		if (symbolTable.find(varExpr->VariableName) == symbolTable.end() && !varExpr->isFunc)
		{
			throw std::runtime_error("Variable " + varExpr->VariableName + " not found in symbol table.");
		}
		if (!varExpr->isFunc) // var check
		{
			if (symbolTable[varExpr->VariableName]->getName().starts_with("var:"))
			{
				llvm::Value* ptrValue = symbolTable[varExpr->VariableName];
				if (RequestPointerAccess)
				{
					return { ptrValue, llvm::dyn_cast<llvm::AllocaInst>(ptrValue)->getAllocatedType(), semanticTypeTable[varExpr->VariableName] };
				}
				return { builder.CreateLoad(llvm::dyn_cast<llvm::AllocaInst>(ptrValue)->getAllocatedType(), ptrValue, varExpr->VariableName), llvm::dyn_cast<llvm::AllocaInst>(ptrValue)->getAllocatedType(), semanticTypeTable[varExpr->VariableName] };
			}
		}
		else if(varExpr->isFunc) // func check
		{
			return { symbolTable["func:"+varExpr->VariableName], typedSymbolTable[varExpr->VariableName].type, semanticTypeTable[varExpr->VariableName]};
		}

		return { symbolTable[varExpr->VariableName], typedSymbolTable[varExpr->VariableName].type, semanticTypeTable[varExpr->VariableName] };
	}
	else if (auto strCtorExpr = dynamic_cast<StructCtorExpr*>(&expression))
	{
		return GenerateStructCtor(strCtorExpr);
	}
	else if (auto strAccExpr = dynamic_cast<StructAccessExpr*>(&expression))
	{
		auto memberName = strAccExpr->MemberName;
		bool prevPointerState = RequestPointerAccess;
		RequestPointerAccess = true;
		auto structInst = GenerateExpression(*strAccExpr->LHS); // this should be an alloca instance (ex. LHS is a struct access expr, so it goes StrAccExpr->StrAccExpr->VarExpr)
		RequestPointerAccess = prevPointerState;

		std::string structName = structInst.semanticType->getBaseType()->to_string();

		if (this->RequestPointerAccess)
		{
			auto strMemGEP = GetStructMemberPointer(structName, structInst.value, memberName);
			return { strMemGEP, structDefTable[structName].StructMembers[memberName].Type, structDefTable[structName].StructMembers[memberName].SemanticType };
		}
		if (this->RequestFunctionAccess)
		{
			auto structDef = structDefTable[structName];
			auto* func = module->getFunction("func:"+structName + "::" + memberName);

			return { func, func->getReturnType(), semanticTypeTable[structName + "::" + memberName], structInst.value};
		}

		auto structDef = structDefTable[structName];
		auto strMemGEP = GetStructMemberPointer(structName, structInst.value, memberName);
		return { builder.CreateLoad(structDef.StructMembers[memberName].Type, strMemGEP, ".structInstLoad"), structDefTable[structName].StructMembers[memberName].Type, structDefTable[structName].StructMembers[memberName].SemanticType };
	}
	else if (auto binExpr = dynamic_cast<BinaryExpr*>(&expression))
	{
		auto c_lhs = GenerateExpression(*binExpr->A.get());
		auto c_rhs = GenerateExpression(*binExpr->B.get());

		auto* lhs = c_lhs.value;
		auto* rhs = c_rhs.value;

		auto op = binExpr->Operator;

		if (op == "+")
			return { builder.CreateAdd(lhs, rhs, "addtmp"), c_lhs.type };
		else if (op == "-")
			return {builder.CreateSub(lhs, rhs, "subtmp"), c_lhs.type };
		else if (op == "*")
			return {builder.CreateMul(lhs, rhs, "multmp"), c_lhs.type };
		else if (op == "/")
			return {builder.CreateSDiv(lhs, rhs, "divtmp"), c_lhs.type };
		else if (op == "%")
			return {builder.CreateSRem(lhs, rhs, "modtmp"), c_lhs.type };
		else if (op == "==")
			return {builder.CreateICmpEQ(lhs, rhs, "eqtmp"), c_lhs.type };
		else if (op == "!=")
			return {builder.CreateICmpNE(lhs, rhs, "netmp"), c_lhs.type };
		else if (op == "<")
			return {builder.CreateICmpSLT(lhs, rhs, "lttmp"), c_lhs.type };
		else if (op == "<=")
			return {builder.CreateICmpSLE(lhs, rhs, "letmp"), c_lhs.type };
		else if (op == ">")
			return {builder.CreateICmpSGT(lhs, rhs, "gttmp"), c_lhs.type };
		else if (op == ">=")
			return {builder.CreateICmpSGE(lhs, rhs, "getmp"), c_lhs.type };
		else if (op == "&&")
			return {builder.CreateAnd(lhs, rhs, "andtmp"), c_lhs.type };
		else if (op == "||")
			return {builder.CreateOr(lhs, rhs, "ortmp"), c_lhs.type };
		else
			throw std::runtime_error("Unsupported binary operator.");
	}
}

Overcast::CodeGen::CGResult Overcast::CodeGen::CGEngine::GenerateFunctionCall(const InvokeFunctionExpr& funcCall)
{
	auto reqFlag = RequestFunctionAccess;
	RequestFunctionAccess = true;
	auto c_value = GenerateExpression(*funcCall.InvokedFunction);
	RequestFunctionAccess = reqFlag;
	auto* value = c_value.value;

	llvm::Function* function = llvm::dyn_cast<llvm::Function>(value);

	if (!function)
	{
		throw std::runtime_error("Function " + value->getName().str() + " not found.");
	}

	if (function->getName() == "print") // check for the print function, we need to replace that with printf
	{
		function = module->getFunction("printf");
	}

	std::vector<llvm::Value*> args;
	for (auto& arg : funcCall.Arguments)
	{
		args.push_back(GenerateExpression(*arg).value);
	}

	if (funcCall.IsStructFunc)
	{
		args.push_back(c_value.structObject);
	}

	return { builder.CreateCall(function, args, function->getReturnType()->isVoidTy() ? "" : "calltmp"), function->getReturnType(), semanticTypeTable[function->getName().str()] };
}

Overcast::CodeGen::CGResult Overcast::CodeGen::CGEngine::GenerateStructCtor(StructCtorExpr* strCtorExpr, llvm::Value* overridePtr)
{
	// okay time to find the ctor, if I can't find it, then I just "pretend" there's a default one that just makes the object
	auto ctorFunction = module->getFunction("func:" + strCtorExpr->StructTypeName + "::" + "ctor");
	auto structObject = overridePtr == nullptr ? builder.CreateAlloca(structDefTable[strCtorExpr->StructTypeName].StructType, nullptr, "structObj:" + strCtorExpr->StructTypeName) : overridePtr;

	if (ctorFunction)
	{
		std::vector<llvm::Value*> args;
		for (const auto& argExpr : strCtorExpr->Arguments)
		{
			args.push_back(GenerateExpression(*argExpr).value);
		}

		args.push_back(structObject);
		builder.CreateCall(ctorFunction, args, "ctor_call");
	}

	semanticTypeTable[strCtorExpr->StructTypeName] = structDefTable[strCtorExpr->StructTypeName].SemanticType;
	return { structObject, structDefTable[strCtorExpr->StructTypeName].StructType, IdentifierType::GetVoidType() };
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
			return llvm::Type::getInt8Ty(context);
		}
		else if (type->TypeName == "bool")
		{
			return llvm::Type::getInt1Ty(context);
		}
		else if (type->TypeName == "char")
		{
			return llvm::Type::getInt8Ty(context);
		}
		else
		{
			// check struct types
			if (structDefTable.find(type->TypeName) != structDefTable.end())
			{
				return structDefTable[type->TypeName].StructType;
			}
			throw std::runtime_error("Unknown type: " + type->TypeName);
		}
	}
}

Overcast::CodeGen::CGEngine::~CGEngine()
{
}