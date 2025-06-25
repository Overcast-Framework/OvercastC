#include "ocpch.h"
#include "ocutils.h"
#include "binder.h"

void Overcast::Semantic::Binder::Binder::BindStatement(const Statement& stmt)
{
	switch (stmt.m_Type)
	{
	case Statement::Type::FunctionDecl:
	{
		const FunctionDeclStatement& funcDecl = static_cast<const FunctionDeclStatement&>(stmt);
		BindFunctionDecl(funcDecl);
		break;
	}
	case Statement::Type::StructDecl:
	{
		const StructDeclStatement& strDecl = static_cast<const StructDeclStatement&>(stmt);
		BindStructDecl(strDecl);
		break;
	}
	case Statement::Type::VariableDecl:
	{
		const VariableDeclStatement& varDecl = static_cast<const VariableDeclStatement&>(stmt);
		BindVariableDecl(varDecl);
		break;
	}
	case Statement::Type::Expression:
	{
		const ExpressionStatement& exprStmt = static_cast<const ExpressionStatement&>(stmt);
		this->BindExpression(*exprStmt.EncapsulatedExpr);
		break;
	}
	case Statement::Type::Assignment:
	{
		const AssignmentStatement& assgStmt = static_cast<const AssignmentStatement&>(stmt);
		Symbol varSymbol = BindExpression(*assgStmt.LHS);

		Symbol valueSymbol = this->BindExpression(*assgStmt.Value);
		if (valueSymbol.Type->to_string() != varSymbol.Type->to_string())
		{
			throw std::runtime_error("Type mismatch in value assignment: expected " + varSymbol.Type->to_string() +
				", but got " + valueSymbol.Type->to_string() + ".");
		}
		break;
	}
	case Statement::Type::If:
	{
		const IfStatement& ifStmt = static_cast<const IfStatement&>(stmt);
		auto& conditionSymbol = this->BindExpression(*ifStmt.Condition);
		if (conditionSymbol.Type->to_string() != "bool")
		{
			throw std::runtime_error("Condition in if statement must be of type bool, but got " + conditionSymbol.Type->to_string() + ".");
		}
		for (const auto& bodyStmt : ifStmt.Body)
		{
			this->BindStatement(*bodyStmt);
		}
		if (!ifStmt.ElseBody.empty())
		{
			for (const auto& elseStmt : ifStmt.ElseBody)
			{
				this->BindStatement(*elseStmt);
			}
		}
		break;
	}
	case Statement::Type::While:
	{
		const WhileStatement& whStmt = static_cast<const WhileStatement&>(stmt);
		auto& conditionSymbol = this->BindExpression(*whStmt.Condition);
		if (conditionSymbol.Type->to_string() != "bool")
		{
			throw std::runtime_error("Condition in while statement must be of type bool, but got " + conditionSymbol.Type->to_string() + ".");
		}
		for (const auto& bodyStmt : whStmt.Body)
		{
			this->BindStatement(*bodyStmt);
		}
		break;
	}
	case Statement::Type::Return:
	{
		const ReturnStatement& retStmt = static_cast<const ReturnStatement&>(stmt);
		if (!CurrentFunction.Type || CurrentFunction.Type->to_string() == "void")
		{
			throw std::runtime_error("Return statement found in a function that does not return a value.");
		}
		if (CurrentFunction.Name == "<INVALID>")
		{
			throw std::runtime_error("Return statement found outside of a function context.");
		}
		if (retStmt.ReturnValue)
		{
			auto& returnSymbol = this->BindExpression(*retStmt.ReturnValue);
			if (returnSymbol.Type->to_string() != CurrentFunction.Type->to_string())
			{
				throw std::runtime_error("Return type mismatch in function " + CurrentFunction.Name + ": expected " +
					CurrentFunction.Type->to_string() + ", but got " + returnSymbol.Type->to_string() + ".");
			}
		}
		break;
	}
	case Statement::Type::Use:
		break;
	case Statement::Type::PackageDecl:
		break;
	default:
	{
		throw std::runtime_error("Unsupported statement type for binding.");
		break;
	}
	}
}

void Overcast::Semantic::Binder::Binder::BindFunctionDecl(const FunctionDeclStatement& funcDecl)
{
	Symbol funcSymbol(funcDecl.FuncName, SymbolKind::Function, funcDecl.ReturnType.get());
	funcSymbol.ParamCount = funcDecl.Parameters.size();

	for (const auto& param : funcDecl.Parameters)
	{
		funcSymbol.ParamTypeNames.push_back(param.ParameterType->to_string());
	}

	bool passAdd = false;

	Symbol existingSymbol;
	if (LookupSymbol(funcDecl.FuncName, existingSymbol))
	{
		// if the sigs match, then prob just global table conflict:
		if (funcDecl.Parameters.size() != existingSymbol.ParamTypes.size() && !existingSymbol.IsStructMemberFunc) // obv no match
		{
			throw std::runtime_error("Function " + funcDecl.FuncName + " is already defined in this module.");
		}

		bool noMatch = true;
		int idx = 0;
		for (const auto& param : existingSymbol.ParamTypes)
		{
			if (param->to_string() != funcDecl.Parameters[idx].ParameterType->to_string())
			{
				noMatch = false;
			}
			idx++;
		}

		if (noMatch)
		{
			passAdd = true;
		}

		if(!funcDecl.IsStructMember && !passAdd)
			throw std::runtime_error("Function " + funcDecl.FuncName + " is already defined in this module.");
	}

	if (!funcDecl.IsStructMember && !passAdd)
		this->Scopes.back().AddSymbol(funcSymbol);

	this->EnterScope();

	for (const auto& param : funcDecl.Parameters)
	{
		Symbol paramSymbol(param.ParameterName, SymbolKind::Variable, param.ParameterType.get());
		this->Scopes.back().AddSymbol(paramSymbol);
	}

	CurrentFunction = funcSymbol;

	for (const auto& statement : funcDecl.Body)
	{
		this->BindStatement(*statement);
	}

	CurrentFunction = Symbol("<INVALID>", SymbolKind::Function, nullptr);

	this->ExitScope();
}

void Overcast::Semantic::Binder::Binder::BindVariableDecl(const VariableDeclStatement& varDecl)
{
	Symbol varSymbol(varDecl.VarName, SymbolKind::Variable, varDecl.VariableType.get());
	Symbol existingSymbol;
	if (this->Scopes.back().TryGetSymbol(varDecl.VarName, existingSymbol))
	{
		throw std::runtime_error("Variable " + varDecl.VarName + " is already defined in this scope.");
	}

	if (varDecl.VariableType->to_string() == "void")
	{
		throw std::runtime_error("Variable " + varDecl.VarName + " cannot have type void.");
	}

	if (varDecl.Defined)
	{
		auto& exSymbol = BindExpression(*varDecl.DefaultValue);
		if (exSymbol.Type->to_string() != varDecl.VariableType->to_string())
		{
			throw std::runtime_error("Variable " + varDecl.VarName + " is initialized with type " +
				exSymbol.Type->to_string() + ", but expected type is " + varDecl.VariableType->to_string() + ".");
		}
	}

	this->Scopes.back().AddSymbol(varSymbol);
}

void Overcast::Semantic::Binder::Binder::BindStructDecl(const StructDeclStatement& structDecl)
{
	Symbol structSymbol(structDecl.StructName, SymbolKind::Struct, new IdentifierType(structDecl.StructName));
	Symbol strCheck("INTERNAL_STRUCT_CHECK", SymbolKind::Variable, &IdentifierType{ "bool" }); // this doesnt matter
	if (LookupSymbol(structDecl.StructName, strCheck))
	{
		throw std::runtime_error("Struct " + structDecl.StructName + " is already defined in this scope.");
	}

	for (const auto& member : structDecl.Members)
	{
		Symbol memberSymbol(member.ParameterName, SymbolKind::Variable, member.ParameterType.get());
		structSymbol.StructSymbols.push_back(memberSymbol);
	}

	for (const auto& memberFunc : structDecl.MemberFunctions)
	{
		Symbol memberFuncSymbol(memberFunc->FuncName, SymbolKind::Function, memberFunc->ReturnType.get());

		auto regType = std::make_unique<IdentifierType>(structDecl.StructName);
		auto pointerType = std::make_unique<PointerType>(std::move(regType));

		memberFunc->Parameters.push_back({ std::move(pointerType), "this" });
		memberFuncSymbol.ParamCount = memberFunc->Parameters.size();
		memberFuncSymbol.IsStructMemberFunc = true;
		memberFunc->IsStructMember = true;
		for (const auto& param : memberFunc->Parameters)
		{
			memberFuncSymbol.ParamTypeNames.push_back(param.ParameterType->to_string());
		}

		structSymbol.StructSymbols.push_back(memberFuncSymbol);
	}
	this->Scopes.back().AddSymbol(structSymbol);
	for (const auto& memberFunc : structDecl.MemberFunctions)
	{
		BindStatement(*memberFunc);
	}
}

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindExpression(Expression& expr)
{
	if (dynamic_cast<InvokeFunctionExpr*>(&expr))
	{
		InvokeFunctionExpr& funcInv = static_cast<InvokeFunctionExpr&>(expr);
		return this->BindFuncInvoke(funcInv);
	}
	else if (dynamic_cast<const VariableUseExpr*>(&expr))
	{
		VariableUseExpr& varUse = static_cast<VariableUseExpr&>(expr);
		return this->BindVariableUse(varUse);
	}
	else if (dynamic_cast<const StringLiteralExpr*>(&expr))
	{
		return Symbol(std::string("<string_literal>"), SymbolKind::Variable, IdentifierType::GetStringType());
	}
	else if (dynamic_cast<const IntLiteralExpr*>(&expr))
	{
		return Symbol(std::string("<int_literal>"), SymbolKind::Variable, IdentifierType::GetIntType());
	}
	else if (dynamic_cast<const FloatLiteralExpr*>(&expr))
	{
		// Boolean literals do not require binding, they are handled in code generation.
	}
	else if (dynamic_cast<const BinaryExpr*>(&expr))
	{
		return this->BindBinaryExpr(static_cast<const BinaryExpr&>(expr));
	}
	else if (dynamic_cast<const StructCtorExpr*>(&expr))
	{
		return this->BindStructCtor(static_cast<const StructCtorExpr&>(expr));
	}
	else if (dynamic_cast<const StructAccessExpr*>(&expr))
	{
		return this->BindStructAccess(static_cast<const StructAccessExpr&>(expr));
	}
	else
	{
		throw std::runtime_error("Unsupported expression type for binding.");
	}
}

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindFuncInvoke(InvokeFunctionExpr& funcInv)
{
	Symbol funcSymbol;
	funcSymbol = BindExpression(*funcInv.InvokedFunction);

	if (funcSymbol.Kind != SymbolKind::Function)
	{
		throw std::runtime_error("Symbol " + funcSymbol.Name + " is not a function, or is undefined.");
	}

	if (funcSymbol.IsStructMemberFunc)
		funcInv.IsStructFunc = true;

	if (!funcSymbol.Variadic)
	{
		auto tsFnArgC = funcSymbol.ParamCount;
		if (funcSymbol.IsStructMemberFunc)
			tsFnArgC--;

		if (funcInv.Arguments.size() != tsFnArgC)
		{
			throw std::runtime_error("Function " + funcSymbol.Name + " expects " +
				std::to_string(funcSymbol.ParamCount) + " arguments, but got " + std::to_string(funcInv.Arguments.size()) + ".");
		}

		for (int i = 0; i < tsFnArgC; i++)
		{
			auto& arg = BindExpression(*funcInv.Arguments[i]);
			if (arg.Type->to_string() != funcSymbol.ParamTypeNames[i])
			{
				throw std::runtime_error("Argument " + std::to_string(i + 1) + " of function " +
					funcSymbol.Name + " is of type " + arg.Type->to_string() +
					", but expected type is " + funcSymbol.ParamTypeNames[i] + ".");
			}
		}
	}

	for (const auto& arg : funcInv.Arguments)
	{
		this->BindExpression(*arg);
	}

	return funcSymbol;
}

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindVariableUse(VariableUseExpr& varUse)
{
	Symbol varSymbol;
	if (!LookupSymbol(varUse.VariableName, varSymbol))
	{
		throw std::runtime_error(varUse.VariableName + " is not defined in this scope.");
	}

	if (varSymbol.Kind != SymbolKind::Variable) // ik I could've slammed that into one if statement, but I prefer this over a long condition lol
	{
		if (varSymbol.Kind != SymbolKind::Function)
		{
			throw std::runtime_error(varUse.VariableName + " is not defined in this scope.");
		}
	}

	varUse.isFunc = varSymbol.Kind == SymbolKind::Function;
	return varSymbol;
}

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindBinaryExpr(const BinaryExpr& binExpr)
{
	Symbol leftSymbol = BindExpression(*binExpr.A);
	Symbol rightSymbol = BindExpression(*binExpr.B);
	if (leftSymbol.Type->to_string() != rightSymbol.Type->to_string())
	{
		throw std::runtime_error("Binary expression operands must be of the same type.");
	}
	if (binExpr.Operator == ">" || binExpr.Operator == "<" ||
		binExpr.Operator == ">=" || binExpr.Operator == "<=" ||
		binExpr.Operator == "==" || binExpr.Operator == "!=")
	{
		return Symbol("<binary_expr>", SymbolKind::Variable, IdentifierType::GetBoolType());
	}
	return Symbol("<binary_expr>", SymbolKind::Variable, leftSymbol.Type);
}

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindStructCtor(const StructCtorExpr& structCtor)
{
	Symbol structSymbol;
	if (!LookupSymbol(structCtor.StructTypeName, structSymbol))
	{
		throw std::runtime_error("Struct " + structCtor.StructTypeName + " is not defined.");
	}

	if (structSymbol.Kind != SymbolKind::Struct)
	{
		throw std::runtime_error("Identifier " + structCtor.StructTypeName + " is not a struct.");
	}

	Symbol ctorSymbol("<INVALID>", SymbolKind::Variable, structSymbol.Type);
	for (auto& symbol : structSymbol.StructSymbols)
	{
		if (symbol.Kind == SymbolKind::Function && symbol.Name == "ctor") // that means there's a ctor
		{
			ctorSymbol = symbol;
			break;
		}
	}

	if (ctorSymbol.Name != "<INVALID>")
	{
		if (structCtor.Arguments.size() != ctorSymbol.ParamTypeNames.size()-1)
		{
			throw std::runtime_error("No overload of struct " + structCtor.StructTypeName + "'s constructors take " + std::to_string(structCtor.Arguments.size()) + " arguments.");
		}

		for (int i = 0; i < ctorSymbol.ParamTypeNames.size()-1; i++)
		{
			auto& param = ctorSymbol.ParamTypeNames[i];
			auto& arg = BindExpression(*structCtor.Arguments[i]).Type->to_string();

			if (param != arg)
			{
				throw std::runtime_error("Struct constructor argument type mismatch. Expected type " + param + ", got " + arg + ".");
			}
		}
	}
	else
	{
		if (structCtor.Arguments.size() != ctorSymbol.ParamTypeNames.size())
		{
			throw std::runtime_error("No overload of struct " + structCtor.StructTypeName + "'s constructors take " + std::to_string(structCtor.Arguments.size()) + " arguments.");
		}
	}

	return structSymbol;
}

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindStructAccess(const StructAccessExpr& structAcc)
{
	Symbol structObject = BindExpression(*structAcc.LHS);
	Symbol structSymbol;

	auto typeName = structObject.Type->getBaseType()->to_string();

	if (!LookupSymbol(typeName, structSymbol))
	{
		throw std::runtime_error("Struct " + typeName + " was not defined in this program.");
	}
	if (structSymbol.Kind != SymbolKind::Struct)
	{
		throw std::runtime_error(structObject.Name + " is not a struct-type symbol.");
	}

	auto& members = structSymbol.StructSymbols;
	auto it = std::find_if(members.begin(), members.end(), [&](const Symbol& sym) {
		return sym.Name == structAcc.MemberName;
		});
	if (it == members.end()) {
		throw std::runtime_error(structAcc.MemberName + " is not a valid member of struct " + structSymbol.Name + ".");
	}

	return *it;
}
