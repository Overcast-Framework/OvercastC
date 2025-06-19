#pragma once
#include <iostream>
#include "Overcast/lexer.h"
#include "expressions.h"
#include "statements.h"
#include "Overcast/ocutils.h"
#include <iterator>
#include <algorithm>
#include <string>

namespace Overcast::Parser
{
	class Parser
	{
	public:
		explicit Parser(std::vector<Token>& tokens)
			: Tokens(tokens), currentToken(tokens.begin()) {
		}

		std::vector<std::unique_ptr<Statement>> Parse();
	private:
		std::vector<Token>& Tokens;
		std::vector<Token>::iterator currentToken;

		std::unique_ptr<Expression> ParseExpression();
		std::unique_ptr<Statement> ParseStatement();
		
		std::unique_ptr<OCType> ParseType();
		std::unique_ptr<IdentifierType> ParseIdentifierType();
		std::unique_ptr<PointerType> ParsePtrType();

		std::vector<std::unique_ptr<Statement>> ParseBlockStatement();
		std::unique_ptr<FunctionDeclStatement> ParseFunctionDeclStatement();
		std::unique_ptr<VariableDeclStatement> ParseVarDeclStatement();
		std::unique_ptr<ReturnStatement> ParseReturnStatement();
		std::unique_ptr<ConstDeclStatement> ParseConstDeclStatement();

		std::unique_ptr<IntLiteralExpr> ParseIntLiteralExpr();
		std::unique_ptr<FloatLiteralExpr> ParseFloatLiteralExpr();
		std::unique_ptr<StringLiteralExpr> ParseStringLiteralExpr();
		std::unique_ptr<VariableUseExpr> ParseVariableExpr();
		std::unique_ptr<ConstUseExpr> ParseConstUseExpr();
		std::unique_ptr<BinaryExpr> ParseBinaryExpr();
		std::unique_ptr<Expression> ParseUnaryExpr();
		std::unique_ptr<TypeCastExpr> ParseTypeCastExpr();
		std::unique_ptr<InvokeFunctionExpr> ParseFuncInvokeExpr();

		const Token& Peek() {
			// Check if the next token exists
			auto nextToken = currentToken;
			++nextToken;  // Move to the next token

			// If the next token is out of range, throw an exception
			if (nextToken == Tokens.end()) {
				throw std::out_of_range("Reached end of tokens");
			}

			// Return the next token without advancing the current token
			return *nextToken;
		}

		const Token& Match(TokenType type);
		const Token& Match(TokenType type, const std::string& value);

		inline void NextToken()
		{
			if(currentToken != Tokens.end())
				++currentToken;
		}
	};


	class SyntaxError : public std::runtime_error
	{
		std::string what_message;
	public:
		explicit SyntaxError(const std::string& message)
			: std::runtime_error(message), what_message(message) {}

		const char* what() const noexcept override
		{
			return what_message.c_str();
		}
	};
}