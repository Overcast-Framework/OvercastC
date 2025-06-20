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

		std::unique_ptr<Expression> ParseExpression(int precedence = 0);
		std::unique_ptr<Expression> ParsePrimaryExpression();
		std::unique_ptr<Statement> ParseStatement();
		
		std::unique_ptr<OCType> ParseType();
		std::unique_ptr<IdentifierType> ParseIdentifierType();
		std::unique_ptr<PointerType> ParsePtrType();

		std::vector<std::unique_ptr<Statement>> ParseBlockStatement();
		std::unique_ptr<FunctionDeclStatement> ParseFunctionDeclStatement();
		std::unique_ptr<VariableDeclStatement> ParseVarDeclStatement();
		std::unique_ptr<VariableSetStatement> ParseVarSetStatement();
		std::unique_ptr<StructDeclStatement> ParseStructDeclStatement();
		std::unique_ptr<IfStatement> ParseIfStatement();
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
		std::unique_ptr<StructCtorExpr> ParseStructCtorExpr();

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

		inline int GetPrecedence(Token token)
		{
			std::cout << "currentToken pointer: " << &currentToken << std::endl;
			if (token.Type != TokenType::OPERATOR) return -1;
			std::cout << "currentToken pointer: " << &currentToken << std::endl;
			std::string op = token.Lexeme;
			if (op == "=") return 1;
			else if (op == "->" || op == "<-") return 2;
			else if (op == "||") return 3;
			else if (op == "&&") return 4;
			else if (op == "==") return 5;
			else if (op == "!=") return 6;
			else if (op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "&=" || op == "|=" || op == "^=") return 11;
			else if (op == "^") return 12;
			else if (op == "++" || op == "--") return 13;
			else if (op == "<=" || op == ">=") return 7;
			else if (op == "+" || op == "-") return 9;
			else if (op == "*" || op == "/") return 10;
			else if (op == "<" || op == ">") return 8;

			return -1;
		}

		inline bool IsRightAssociative(const std::string& op) {
			static const std::unordered_set<std::string> rightAssociativeOps = {
				"=",
				"+=", "-=", "*=", "/=", "%=",
				"^"
			};

			return rightAssociativeOps.count(op) > 0;
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