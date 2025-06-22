#pragma once
#include <iostream>
#include <vector>
#include <memory>
#include "types.h"

class Expression
{
public:
	enum class Type
	{
		None,
		Int,
		Float,
		String,
		Variable,
		ConstUse,
		Binary,
		FunctionCall,
		StructCtor,
		StructAccess,
		Cast
	};

	Type m_Type = Type::None;

	virtual ~Expression() {}
};

class IntLiteralExpr : public Expression
{
public:
	int LiteralValue;

	IntLiteralExpr(int value)
		: LiteralValue(value)
	{
		m_Type = Type::Int;
	}
};

class FloatLiteralExpr : public Expression
{
public:
	float LiteralValue;

	FloatLiteralExpr(float value)
		: LiteralValue(value)
	{
		m_Type = Type::Float;
	}
};

class StringLiteralExpr : public Expression
{
public:
	std::string LiteralValue;

	StringLiteralExpr(const std::string& value)
		: LiteralValue(value)
	{
		m_Type = Type::String;
	}
};

class VariableUseExpr : public Expression
{
public:
	std::string VariableName;
	bool isFunc = false; // the binder sets this
	VariableUseExpr(const std::string& varName)
		: VariableName(varName)
	{
		m_Type = Type::Variable;
	}
};

class ConstUseExpr : public Expression
{
public:
	std::string ConstName;
};

class BinaryExpr : public Expression
{
public:
	std::unique_ptr<Expression> A;
	std::string Operator;
	std::unique_ptr<Expression> B;

	BinaryExpr(std::unique_ptr<Expression>&& a, const std::string& op, std::unique_ptr<Expression>&& b)
		: A(std::move(a)), Operator(op), B(std::move(b))
	{
		m_Type = Type::Binary;
	}
};

class InvokeFunctionExpr : public Expression
{
public:
	std::unique_ptr<Expression> InvokedFunction;
	std::vector<std::unique_ptr<Expression>> Arguments;
	bool IsStructFunc = false;

	InvokeFunctionExpr(std::unique_ptr<Expression> funcExpr, std::vector<std::unique_ptr<Expression>>&& args)
		: InvokedFunction(std::move(funcExpr)), Arguments(std::move(args))
	{
		m_Type = Type::FunctionCall;
	}
};

class StructAccessExpr : public Expression
{
public:
	std::unique_ptr<Expression> LHS;
	std::string MemberName;

	StructAccessExpr(std::unique_ptr<Expression> lhs, const std::string& memberName)
		: LHS(std::move(lhs)), MemberName(memberName)
	{
		m_Type = Type::StructAccess;
	}
};

class StructCtorExpr : public Expression
{
public:
	std::string StructTypeName;
	std::vector<std::unique_ptr<Expression>> Arguments;

	StructCtorExpr(const std::string& structTypeName, std::vector<std::unique_ptr<Expression>>&& args)
		: StructTypeName(structTypeName), Arguments(std::move(args))
	{
		m_Type = Type::StructCtor;
	}
};

class TypeCastExpr : public Expression
{
public:
	std::unique_ptr<OCType> CastToType;
	Expression CastWhat;

	TypeCastExpr(std::unique_ptr<OCType>&& castType, Expression castWhat)
		: CastToType(std::move(castType)), CastWhat(std::move(castWhat))
	{
		m_Type = Type::Cast;
	}
};