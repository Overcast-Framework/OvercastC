#pragma once
#include <iostream>

class IdentifierType; // forward declare

class OCType {
public:
	virtual std::string to_string() const { return "<base>"; };
	virtual ~OCType() = default;

	OCType(const OCType&) = delete;
	OCType& operator=(const OCType&) = delete;

	OCType(OCType&&) noexcept = default;
	OCType& operator=(OCType&&) noexcept = default;


	OCType() = default;
	virtual std::unique_ptr<OCType> clone() const = 0;
	virtual IdentifierType *getBaseType() = 0;
};

class IdentifierType : public OCType
{
public:
	std::string TypeName;

	IdentifierType() = default;
	IdentifierType(std::string tyName) : TypeName(tyName) {}

	std::string to_string() const override
	{
		return this->TypeName;
	}

	std::unique_ptr<OCType> clone() const override {
		return std::make_unique<IdentifierType>(*this);
	}
	IdentifierType(const IdentifierType& other)
		: OCType(), TypeName(other.TypeName) {
	}

	IdentifierType* getBaseType() override;

	static IdentifierType* GetStringType()
	{
		static IdentifierType stringType("string");
		return &stringType;
	}

	static IdentifierType* GetIntType()
	{
		static IdentifierType intType("int");
		return &intType;
	}

	static IdentifierType* GetFloatType()
	{
		static IdentifierType floatType("float");
		return &floatType;
	}

	static IdentifierType* GetBoolType()
	{
		static IdentifierType boolType("bool");
		return &boolType;
	}

	static IdentifierType* GetVoidType()
	{
		static IdentifierType voidType("void");
		return &voidType;
	}
};

class PointerType : public OCType
{
public:
	std::unique_ptr<OCType> OfType;

	PointerType() = default;
	PointerType(std::unique_ptr<OCType> ofType) : OfType(std::move(ofType)) {}
	std::string to_string() const override
	{
		return this->OfType->to_string() + "*";
	}

	std::unique_ptr<OCType> clone() const override {
		return std::make_unique<PointerType>(OfType->clone());
	}

	IdentifierType* getBaseType() override;
};
