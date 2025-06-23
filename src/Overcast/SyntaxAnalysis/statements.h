#pragma once
#include <iostream>
#include <vector>
#include "types.h"
#include "expressions.h"

class Statement
{
public:
	enum class Type
	{
		FunctionDecl,
		VariableDecl,
		ConstDecl,
		Return,
		Assignment,
		If,
		While,
		StructDecl,
		Expression // As in Expression Statements
	};
	Type m_Type;

	Statement(Type type)
		: m_Type(type)
	{
	}

	Statement(const Statement&) = delete;
	Statement& operator=(const Statement&) = delete;

	Statement(Statement&&) noexcept = default;
	Statement& operator=(Statement&&) noexcept = default;

	virtual ~Statement() {}
};

struct Parameter
{
	std::unique_ptr<OCType> ParameterType;
	std::string ParameterName;

	Parameter(std::unique_ptr<OCType>&& type, const std::string& name)
		: ParameterType(std::move(type)), ParameterName(name)
	{
	}

	Parameter(const Parameter& other)
		: ParameterType(other.ParameterType ? other.ParameterType->clone() : nullptr),
		ParameterName(other.ParameterName)
	{
	}

	Parameter& operator=(const Parameter& other)
	{
		if (this != &other)
		{
			ParameterType = other.ParameterType ? other.ParameterType->clone() : nullptr;
			ParameterName = other.ParameterName;
		}
		return *this;
	}

	Parameter(Parameter&&) noexcept = default;
	Parameter& operator=(Parameter&&) noexcept = default;
};

class FunctionDeclStatement : public Statement
{
public:
	std::string FuncName;
	bool IsExtern = false;
	std::unique_ptr<OCType> ReturnType;
	std::vector<Parameter> Parameters;
	std::vector<std::unique_ptr<Statement>> Body;
	bool IsStructMember = false; // only for the binder

	// Disable copy
	FunctionDeclStatement(const FunctionDeclStatement&) = delete;
	FunctionDeclStatement& operator=(const FunctionDeclStatement&) = delete;

	// Enable move
	FunctionDeclStatement(FunctionDeclStatement&&) noexcept = default;
	FunctionDeclStatement& operator=(FunctionDeclStatement&&) noexcept = default;

	FunctionDeclStatement(const std::string& FuncName, std::unique_ptr<OCType>&& retType, const std::vector<Parameter>& Parameters, std::vector<std::unique_ptr<Statement>>&& Body)
		: Statement{ Type::FunctionDecl }, FuncName(FuncName), ReturnType(std::move(retType)), Parameters(Parameters), Body(std::move(Body))
	{
	}

	FunctionDeclStatement() = default;
};

class VariableDeclStatement : public Statement
{
public:
	std::string VarName;
	std::unique_ptr<OCType> VariableType;
	bool Defined;
	std::unique_ptr<Expression> DefaultValue;

	VariableDeclStatement() = default;

	VariableDeclStatement(const std::string& VarName, std::unique_ptr<OCType>&& VariableType, bool Defined, std::unique_ptr<Expression> defaultValue)
		: Statement{ Type::VariableDecl }, VarName(VarName), VariableType(std::move(VariableType)), Defined(Defined), DefaultValue(std::move(defaultValue))
	{
	}
};

class AssignmentStatement : public Statement
{
public:
	std::unique_ptr<Expression> LHS;
	std::unique_ptr<Expression> Value;

	AssignmentStatement() = default;

	AssignmentStatement(std::unique_ptr<Expression> lhs, std::unique_ptr<Expression> value)
		: Statement{ Type::Assignment }, LHS(std::move(lhs)), Value(std::move(value))
	{
	}
};

class StructDeclStatement : public Statement
{
public:
	std::string StructName;
	std::vector<Parameter> Members;
	std::vector<std::unique_ptr<FunctionDeclStatement>> MemberFunctions;

	StructDeclStatement(const std::string& structName, const std::vector<Parameter>& members)
		: Statement{ Type::StructDecl }, StructName(structName), Members(members)
	{
	}
};

class IfStatement : public Statement
{
public:
	std::unique_ptr<Expression> Condition;
	std::vector<std::unique_ptr<Statement>> Body;
	std::vector<std::unique_ptr<Statement>> ElseBody;
	IfStatement(std::unique_ptr<Expression> condition, std::vector<std::unique_ptr<Statement>>&& body, std::vector<std::unique_ptr<Statement>>&& elseBody)
		: Statement{ Type::If }, Condition(std::move(condition)), Body(std::move(body)), ElseBody(std::move(elseBody))
	{
	}
};

class WhileStatement : public Statement
{
public:
	std::unique_ptr<Expression> Condition;
	std::vector<std::unique_ptr<Statement>> Body;
	WhileStatement(std::unique_ptr<Expression> condition, std::vector<std::unique_ptr<Statement>>&& body)
		: Statement{ Type::While }, Condition(std::move(condition)), Body(std::move(body))
	{
	}
};

class ConstDeclStatement : public Statement
{
public:
	std::string VarName;
	std::unique_ptr<OCType> VariableType;
	Expression DefaultValue;

	ConstDeclStatement(const std::string& VarName, std::unique_ptr<OCType>&& VariableType, const Expression& DefaultValue)
		: Statement{ Type::ConstDecl }, VarName(VarName), VariableType(std::move(VariableType)), DefaultValue(DefaultValue)
	{
	}

	ConstDeclStatement() = default;
};

class ReturnStatement : public Statement
{
public:
	std::unique_ptr<Expression> ReturnValue;

	ReturnStatement(std::unique_ptr<Expression> returnValue)
		: ReturnValue(std::move(returnValue)), Statement{ Type::Return }
	{
	}
};

class ExpressionStatement : public Statement
{
public:
	std::unique_ptr<Expression> EncapsulatedExpr;

	explicit ExpressionStatement(std::unique_ptr<Expression> e) : EncapsulatedExpr(std::move(e)),
		Statement{ Type::Expression }
	{
	}
};