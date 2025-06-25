#include "ocpch.h"
#include "parser.h"

std::vector<std::unique_ptr<Statement>> Overcast::Parser::Parser::Parse()
{
    std::vector<std::unique_ptr<Statement>> ResultVector;
	if (!this->Tokens->empty())
	{
		while (currentToken != Tokens->end())
		{
			ResultVector.push_back(std::move(ParseStatement()));
		}
	}

	return ResultVector;
}

std::unique_ptr<Expression> Overcast::Parser::Parser::ParseExpression(int precedence)
{
	auto lhs = ParsePostfixExpression();
    Token tokenCopy = *currentToken; // to avoid a really weird issue
    while (currentToken->Type == TokenType::OPERATOR && currentToken->Lexeme != "=" && GetPrecedence(tokenCopy) >= precedence)
    {
        auto op = Match(TokenType::OPERATOR);
        auto opPrec = GetPrecedence(op);
        int nextPrecedence;
        if (IsRightAssociative(op.Lexeme)) {
            nextPrecedence = opPrec;
        }
        else {
            nextPrecedence = opPrec + 1;
        }
		auto rhs = ParseExpression(nextPrecedence);
		lhs = std::make_unique<BinaryExpr>(std::move(lhs), op.Lexeme, std::move(rhs));
    }

    return lhs;
}

std::unique_ptr <Expression> Overcast::Parser::Parser::ParsePrimaryExpression()
{
    switch (currentToken->Type)
    {
    case TokenType::INTEGER:
    {
        return ParseIntLiteralExpr();
    }
    case TokenType::STRING:
    {
        return ParseStringLiteralExpr();
    }
    case TokenType::IDENTIFIER:
    {
        return ParseVariableExpr();
    }
	case TokenType::KEYWORD:
		if (currentToken->Lexeme == "new") // boolean literal
		{
            return ParseStructCtorExpr();
		}
		break;
    case TokenType::SYMBOL:
        if (currentToken->Lexeme == "(") // grouped subexpression
        {
            Match(TokenType::SYMBOL);
            auto expr = ParseExpression(0);
            if (currentToken->Lexeme != ")")
                throw std::runtime_error("Expected closing parenthesis");
            Match(TokenType::SYMBOL);
            return expr;
        }
        break;
    default:
        throw std::runtime_error("Unexpected token in expression: " + currentToken->Lexeme);
    }
}

std::unique_ptr<Expression> Overcast::Parser::Parser::ParsePostfixExpression()
{
    auto expr = ParsePrimaryExpression();

    while (true)
    {
        if (currentToken->Lexeme == "->")
        {
            Match(TokenType::ARROW, "->");
            auto memberName = Match(TokenType::IDENTIFIER).Lexeme;
            expr = std::make_unique<StructAccessExpr>(std::move(expr), memberName);
        }
        else if (currentToken->Lexeme == "(")
        {
            std::vector<std::unique_ptr<Expression>> arguments;
            Match(TokenType::SYMBOL, "(");

            while (currentToken->Lexeme != ")")
            {
                auto expr = ParseExpression();
                arguments.push_back(std::move(expr));
                if (currentToken->Lexeme == ",")
                    Match(TokenType::SYMBOL);
                else if (currentToken->Lexeme != ")")
                    Match(TokenType::SYMBOL, ","); // to cause the syntax error to pop up
            }

            Match(TokenType::SYMBOL, ")");
            expr = std::make_unique<InvokeFunctionExpr>(std::move(expr), std::move(arguments));
        }
        else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<Statement> Overcast::Parser::Parser::ParseStatement()
{
    switch (currentToken->Type)
    {
        case TokenType::KEYWORD:
        {
            if (currentToken->Lexeme == "func" || currentToken->Lexeme == "extern") // function decl statement
            {
                return ParseFunctionDeclStatement();
            }
            else if (currentToken->Lexeme == "var" || currentToken->Lexeme == "let") // variable decl statement
            {
                return ParseVarDeclStatement();
            }
            else if (currentToken->Lexeme == "return") // return statement
            {
                return ParseReturnStatement();
            }
            else if (currentToken->Lexeme == "const") // const decl statement
            {
                return ParseConstDeclStatement();
            }
			else if (currentToken->Lexeme == "if") // if statement
			{
				return ParseIfStatement();
			}
			else if (currentToken->Lexeme == "while") // loop statement
			{
				return ParseWhileStatement();
			}
			else if (currentToken->Lexeme == "use") 
			{
                Match(TokenType::KEYWORD, "use");
                return std::make_unique<UseStatement>(Match(TokenType::IDENTIFIER).Lexeme);
			}
            else if (currentToken->Lexeme == "package")
            {
                Match(TokenType::KEYWORD, "package");
                return std::make_unique<PackageDeclStatement>(Match(TokenType::IDENTIFIER).Lexeme);
            }
            break;
        }
        case TokenType::IDENTIFIER:
            if (Peek().Lexeme == "(") // expr statement of invoke func
            {
                return std::make_unique<ExpressionStatement>(std::move(ParseExpression()));
            }
            else if (Peek().Lexeme == "=")
            {
				return ParseAssignmentStatement();
			}
            else if (Peek().Lexeme == "->")
            {
                if (Peek(1).Lexeme == "struct") // look two ahead
                    return ParseStructDeclStatement();
                else
                    return ParseAssignmentStatement(); // cuz then it's this->x = a lol
            }
            break;
    }

    throw std::runtime_error("Failed to find a valid statement.");
}

// TYPE PARSING

std::unique_ptr<OCType> Overcast::Parser::Parser::ParseType()
{
    std::unique_ptr<OCType> baseType = ParseIdentifierType();

    while (currentToken->Lexeme == "*")
    {
        Match(TokenType::OPERATOR, "*");
        std::unique_ptr<PointerType> ptrType = std::make_unique<PointerType>(std::move(baseType));
        baseType = std::move(ptrType);
    }

    return baseType;
}

std::unique_ptr<IdentifierType> Overcast::Parser::Parser::ParseIdentifierType()
{
    return std::make_unique<IdentifierType>(Match(TokenType::IDENTIFIER).Lexeme);
}

std::unique_ptr<PointerType> Overcast::Parser::Parser::ParsePtrType()
{
	Match(TokenType::OPERATOR, "*");
    return std::make_unique<PointerType>(std::move(ParseType()));
}

// STATEMENT PARSING

std::vector<std::unique_ptr<Statement>> Overcast::Parser::Parser::ParseBlockStatement()
{
    std::vector<std::unique_ptr<Statement>> blockContent;
    Match(TokenType::SYMBOL, "{");

    while (currentToken->Lexeme != "}")
    {
        auto statement = ParseStatement();
        if (statement->m_Type != Statement::Type::If && statement->m_Type != Statement::Type::While)
        {
            Match(TokenType::SYMBOL, ";");
        }
        blockContent.push_back(std::move(statement));
    }

    Match(TokenType::SYMBOL, "}");

    return blockContent;
}

std::unique_ptr<FunctionDeclStatement> Overcast::Parser::Parser::ParseFunctionDeclStatement()
{
    // keyword identifier '(' params?... ')' arrow(->) (body?) (;?)
    bool externFunc = false;
	if (currentToken->Lexeme == "extern")
	{
		Match(TokenType::KEYWORD, "extern");
        externFunc = true;
	}
    else
    {
        Match(TokenType::KEYWORD, "func");
    }
    std::string name = Match(TokenType::IDENTIFIER).Lexeme;

    std::vector<Parameter> params;
    Match(TokenType::SYMBOL, "(");

    while (currentToken->Lexeme != ")") // name ':' type
    {
        auto name = Match(TokenType::IDENTIFIER).Lexeme;
        Match(TokenType::SYMBOL, ":");
        auto type = ParseType();

        params.push_back({ std::move(type), name });
        if (currentToken->Lexeme == ",")
            Match(TokenType::SYMBOL);
        else if (currentToken->Lexeme != ")")
            Match(TokenType::SYMBOL, ",");
    }

    Match(TokenType::SYMBOL, ")");
    Match(TokenType::ARROW, "->");

    auto returnType = ParseType();

    if (!externFunc)
    {
        auto body = ParseBlockStatement();
        return std::make_unique<FunctionDeclStatement>(name, std::move(returnType), params, std::move(body));
    }
    else
    {
		Match(TokenType::SYMBOL, ";"); // extern functions end with a semicolon
        auto func = std::make_unique<FunctionDeclStatement>(name, std::move(returnType), params, std::vector<std::unique_ptr<Statement>>());
		func->IsExtern = true;

		return std::move(func);
    }
}

std::unique_ptr<VariableDeclStatement> Overcast::Parser::Parser::ParseVarDeclStatement()
{
	// keyword identifier ':' type '=' expr

	Match(TokenType::KEYWORD, "var");
	auto varName = Match(TokenType::IDENTIFIER).Lexeme;
	Match(TokenType::SYMBOL, ":");
	auto varType = ParseType();
    if (currentToken->Lexeme == "=") // if the variable is initialized
    {
		Match(TokenType::OPERATOR, "=");
		auto defaultValue = ParseExpression();
		return std::make_unique<VariableDeclStatement>(varName, std::move(varType), true, std::move(defaultValue));
	}
    else if (currentToken->Lexeme == ";") // if the variable is not initialized
    {
        Match(TokenType::SYMBOL, ";");
        return std::make_unique<VariableDeclStatement>(varName, std::move(varType), false, nullptr);
	}
	else // if the variable declaration is malformed
    {
        throw SyntaxError("Expected '=' or ';' after variable declaration, got " + currentToken->Lexeme + " at line " + std::to_string(currentToken->line) + ", column " + std::to_string(currentToken->col) + ".");
    }
}

std::unique_ptr<AssignmentStatement> Overcast::Parser::Parser::ParseAssignmentStatement()
{
    auto assignee = ParseExpression();
	Match(TokenType::OPERATOR, "=");
	auto value = ParseExpression();
	return std::make_unique<AssignmentStatement>(std::move(assignee), std::move(value));
}

std::unique_ptr<StructDeclStatement> Overcast::Parser::Parser::ParseStructDeclStatement()
{
    auto structName = Match(TokenType::IDENTIFIER).Lexeme;
    Match(TokenType::ARROW, "->");
	Match(TokenType::KEYWORD, "struct");
	Match(TokenType::SYMBOL, "{");

    std::vector<Parameter> members;
	std::vector<std::unique_ptr<FunctionDeclStatement>> memberFunctions;

    while (currentToken->Lexeme != "func" && currentToken->Lexeme != "}")
    {
        auto memberName = Match(TokenType::IDENTIFIER).Lexeme;
        Match(TokenType::SYMBOL, ":");
        auto memberType = ParseType();
        members.push_back({ std::move(memberType), memberName });
        if (currentToken->Lexeme == "")
            Match(TokenType::SYMBOL);
        Match(TokenType::SYMBOL, ";");
    }

	if (currentToken->Lexeme == "func")
	{
		while (currentToken->Lexeme == "func")
		{
			memberFunctions.push_back(ParseFunctionDeclStatement());
		}

        if (currentToken->Lexeme != "func" && currentToken->Lexeme != "}") {
            if (!memberFunctions.empty()) {
                throw SyntaxError("Cannot declare fields after member functions.");
            }
        }
	}

	Match(TokenType::SYMBOL, "}");
	auto structDecl = std::make_unique<StructDeclStatement>(structName, members);
	structDecl->MemberFunctions = std::move(memberFunctions);
	return structDecl;
}

std::unique_ptr<IfStatement> Overcast::Parser::Parser::ParseIfStatement()
{
	Match(TokenType::KEYWORD, "if");
    Match(TokenType::SYMBOL, "(");
	auto condition = ParseExpression();
	Match(TokenType::SYMBOL, ")");
	auto body = ParseBlockStatement();
	std::vector<std::unique_ptr<Statement>> elseBody;
	if (currentToken->Lexeme == "else")
	{
		Match(TokenType::KEYWORD, "else");
		if (currentToken->Lexeme == "if")
		{
			elseBody.push_back(ParseIfStatement());
		}
		elseBody = ParseBlockStatement();
	}
	return std::make_unique<IfStatement>(std::move(condition), std::move(body), std::move(elseBody));
}

std::unique_ptr<WhileStatement> Overcast::Parser::Parser::ParseWhileStatement()
{
    Match(TokenType::KEYWORD, "while");
    Match(TokenType::SYMBOL, "(");
    auto condition = ParseExpression();
    Match(TokenType::SYMBOL, ")");
    auto body = ParseBlockStatement();

    return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
}

std::unique_ptr<ReturnStatement> Overcast::Parser::Parser::ParseReturnStatement()
{
	Match(TokenType::KEYWORD, "return");
	std::unique_ptr<Expression> returnValue = ParseExpression();
    return std::make_unique<ReturnStatement>(std::move(returnValue));
}

std::unique_ptr<ConstDeclStatement> Overcast::Parser::Parser::ParseConstDeclStatement()
{
    return std::unique_ptr<ConstDeclStatement>();
}

// EXPRESSION PARSING

std::unique_ptr<IntLiteralExpr> Overcast::Parser::Parser::ParseIntLiteralExpr()
{
    return std::make_unique<IntLiteralExpr>(std::atoi(Match(TokenType::INTEGER).Lexeme.c_str()));
}

std::unique_ptr<FloatLiteralExpr> Overcast::Parser::Parser::ParseFloatLiteralExpr()
{
    return nullptr;
}

std::unique_ptr<StringLiteralExpr> Overcast::Parser::Parser::ParseStringLiteralExpr()
{
    std::string strContent = Match(TokenType::STRING).Lexeme;
    OCUtils::ReplaceAll(strContent, "\"", "");
    OCUtils::ReplaceAll(strContent, "\\n", "\n");
	OCUtils::ReplaceAll(strContent, "\\t", "\t");
	OCUtils::ReplaceAll(strContent, "\\r", "\r");
	OCUtils::ReplaceAll(strContent, "\\\\", "\\");
	OCUtils::ReplaceAll(strContent, "\\\"", "\"");
    return std::make_unique<StringLiteralExpr>(strContent);
}

std::unique_ptr<VariableUseExpr> Overcast::Parser::Parser::ParseVariableExpr()
{
    return std::make_unique<VariableUseExpr>(Match(TokenType::IDENTIFIER).Lexeme);
}

std::unique_ptr<ConstUseExpr> Overcast::Parser::Parser::ParseConstUseExpr()
{
    return nullptr;
}

std::unique_ptr<BinaryExpr> Overcast::Parser::Parser::ParseBinaryExpr()
{
    return nullptr;
}

std::unique_ptr<Expression> Overcast::Parser::Parser::ParseUnaryExpr()
{
    return nullptr;
}

std::unique_ptr<TypeCastExpr> Overcast::Parser::Parser::ParseTypeCastExpr()
{
    return nullptr;
}

std::unique_ptr<InvokeFunctionExpr> Overcast::Parser::Parser::ParseFuncInvokeExpr()
{
    // EXPRESSION '(' args ')'
    return std::unique_ptr<InvokeFunctionExpr>(); // it was very much made redundant!
}

std::unique_ptr<StructCtorExpr> Overcast::Parser::Parser::ParseStructCtorExpr()
{
	Match(TokenType::KEYWORD, "new");
    auto structName = Match(TokenType::IDENTIFIER).Lexeme;

    std::vector<std::unique_ptr<Expression>> arguments;
    Match(TokenType::SYMBOL, "(");

    while (currentToken->Lexeme != ")")
    {
        auto expr = ParseExpression();
        arguments.push_back(std::move(expr));
        if (currentToken->Lexeme == ",")
            Match(TokenType::SYMBOL);
        else if (currentToken->Lexeme != ")")
            Match(TokenType::SYMBOL, ","); // to cause the syntax error to pop up
    }

    Match(TokenType::SYMBOL, ")");

	return std::make_unique<StructCtorExpr>(structName, std::move(arguments));
}

std::string getTokenName(TokenType tok)
{
    auto it = tokenNames.find(tok);
    if (it != tokenNames.end()) {
        std::string tokenStr = it->second;
        return tokenStr;
    }
    else {
        return "<unknown/eof>";
    }
}

const Token& Overcast::Parser::Parser::Match(TokenType type) {
    if (currentToken != Tokens->end() && currentToken->Type == type) {
        const Token& toReturn = *currentToken;
        NextToken();
        return toReturn;
    }

    throw SyntaxError("expected " + getTokenName(type) + ", got " + getTokenName(currentToken->Type) + " at line " + std::to_string(currentToken->line) + ", column " + std::to_string(currentToken->col) + ".");
}

const Token& Overcast::Parser::Parser::Match(TokenType type, const std::string& value) {
    if (currentToken != Tokens->end() && currentToken->Type == type && currentToken->Lexeme == value) {
        const Token& toReturn = *currentToken;
        NextToken();
        return toReturn;
    }
    
    throw SyntaxError("expected " + getTokenName(type) + " of value \'" + value + "\', got " + getTokenName(currentToken->Type) + " of value \'" + currentToken->Lexeme + "\' at line " + std::to_string(currentToken->line) + ", column " + std::to_string(currentToken->col) + ".");
}