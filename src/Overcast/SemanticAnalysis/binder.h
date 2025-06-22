#pragma once
#include <iostream>
#include <unordered_map>
#include <stack>
#include <string>
#include "Overcast/SyntaxAnalysis/statements.h"
#include "Overcast/SyntaxAnalysis/expressions.h"

namespace Overcast::Semantic::Binder
{
	enum class SymbolKind
	{
		Variable,
		Struct,
		Function
	};

	struct Symbol
	{
		std::string Name;
		SymbolKind Kind;

		OCType* Type; 

		int ParamCount = 0; // for functions
		std::vector<std::string> ParamTypeNames; // for functions, names of parameters
		std::vector<Symbol> StructSymbols; // for structs, members of the struct
		bool Variadic = false;
		bool IsStructMemberFunc = false;

		Symbol() : Name(""), Kind(SymbolKind::Variable), Type(nullptr) {}
		Symbol(const std::string& name, SymbolKind kind, OCType* type)
			: Name(name), Kind(kind), Type(type)
		{
		}
	};

	struct Scope
	{
		std::unordered_map<std::string, Symbol> Symbols;
		void AddSymbol(const Symbol& symbol)
		{
			Symbols.insert({ symbol.Name, symbol });
		}

		bool TryGetSymbol(const std::string& name, Symbol& outSymbol) const
		{
			auto it = Symbols.find(name);
			if (it != Symbols.end())
			{
				outSymbol = it->second;
				return true;
			}
			return false;
		}
	};

	class Binder
	{
	public:
		void Run(const std::vector<std::unique_ptr<Statement>>& statements)
		{
			EnterScope();
			Symbol printFunc("print", SymbolKind::Function, new IdentifierType("int"));
			printFunc.Variadic = true;

			this->Scopes.back().AddSymbol(printFunc);

			for (const auto& statement : statements)
			{
				BindStatement(*statement);
			}
			ExitScope();
		}
	private:
		std::vector<Scope> Scopes;
		Symbol CurrentFunction;

		void BindStatement(const Statement& stmt);
		void BindFunctionDecl(const FunctionDeclStatement& funcDecl);
		void BindVariableDecl(const VariableDeclStatement& varDecl);
		void BindStructDecl(const StructDeclStatement& structDecl);

		Symbol BindExpression(Expression& expr);
		Symbol BindFuncInvoke(InvokeFunctionExpr& funcInv);
		Symbol BindVariableUse(VariableUseExpr& varUse);
		Symbol BindBinaryExpr(const BinaryExpr& binExpr);
		Symbol BindStructCtor(const StructCtorExpr& structCtor);
		Symbol BindStructAccess(const StructAccessExpr& structAcc);

		void EnterScope()
		{
			Scopes.push_back(Scope{});
		}

		void ExitScope()
		{
			if (Scopes.empty())
			{
				std::cerr << "Error: Attempted to exit scope when no scopes are active." << std::endl;
				return;
			}
			Scopes.pop_back();
		}

		bool LookupSymbol(const std::string& name, Symbol& outSymbol) const
		{
			for (auto it = Scopes.rbegin(); it != Scopes.rend(); ++it)
			{
				if (it->TryGetSymbol(name, outSymbol))
					return true;
			}
			return false;
		}
	};
}