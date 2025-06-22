#include "ocpch.h"
#include "types.h"

IdentifierType* IdentifierType::getBaseType()
{
	return this;
}

IdentifierType* PointerType::getBaseType()
{	
	OCType* type = this->OfType.get();
	while (auto ptrType = dynamic_cast<PointerType*>(type))
	{
		type = ptrType->OfType.get();
	}
	return dynamic_cast<IdentifierType*>(type);
}
