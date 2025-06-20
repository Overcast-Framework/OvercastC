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
		Int,
		Float,
		String,
		Variable,
		ConstUse,
		Binary,
		FunctionCall,
		StructCtor,
		Cast
	};

	Type m_Type;

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
	std::string InvokedFunctionName;
	std::vector<std::unique_ptr<Expression>> Arguments;

	InvokeFunctionExpr(const std::string& funcName, std::vector<std::unique_ptr<Expression>>&& args)
		: InvokedFunctionName(funcName), Arguments(std::move(args))
	{
		m_Type = Type::FunctionCall;
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