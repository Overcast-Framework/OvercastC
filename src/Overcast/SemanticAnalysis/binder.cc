#include "ocpch.h"
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

	Symbol existingSymbol;
	if (LookupSymbol(funcDecl.FuncName, existingSymbol))
	{
		throw std::runtime_error("Function " + funcDecl.FuncName + " is already defined in this module.");
	}

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

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindExpression(const Expression& expr)
{
	if (dynamic_cast<const InvokeFunctionExpr*>(&expr))
	{
		const InvokeFunctionExpr& funcInv = static_cast<const InvokeFunctionExpr&>(expr);
		return this->BindFuncInvoke(funcInv);
	}
	else if (dynamic_cast<const VariableUseExpr*>(&expr))
	{
		const VariableUseExpr& varUse = static_cast<const VariableUseExpr&>(expr);
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
	else
	{
		throw std::runtime_error("Unsupported expression type for binding.");
	}
}

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindFuncInvoke(const InvokeFunctionExpr& funcInv)
{
	Symbol funcSymbol;
	if (!LookupSymbol(funcInv.InvokedFunctionName, funcSymbol) || funcSymbol.Kind != SymbolKind::Function)
	{
		throw std::runtime_error("Function " + funcInv.InvokedFunctionName + " is not defined in this scope.");
	}

	if (!funcSymbol.Variadic)
	{
		if (funcInv.Arguments.size() != funcSymbol.ParamCount)
		{
			throw std::runtime_error("Function " + funcInv.InvokedFunctionName + " expects " +
				std::to_string(funcSymbol.ParamCount) + " arguments, but got " + std::to_string(funcInv.Arguments.size()) + ".");
		}

		for (int i = 0; i < funcSymbol.ParamCount; i++)
		{
			auto& arg = BindExpression(*funcInv.Arguments[i]);
			if (arg.Type->to_string() != funcSymbol.ParamTypeNames[i])
			{
				throw std::runtime_error("Argument " + std::to_string(i + 1) + " of function " +
					funcInv.InvokedFunctionName + " is of type " + arg.Type->to_string() +
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

Overcast::Semantic::Binder::Symbol Overcast::Semantic::Binder::Binder::BindVariableUse(const VariableUseExpr& varUse)
{
	Symbol varSymbol;
	if (!LookupSymbol(varUse.VariableName, varSymbol) || varSymbol.Kind != SymbolKind::Variable)
	{
		throw std::runtime_error("Variable " + varUse.VariableName + " is not defined in this scope.");
	}
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
	return Symbol("<binary_expr>", SymbolKind::Variable, leftSymbol.Type);
}
