#pragma once

#include "Properties.h"

namespace CPyburnRTXEngine
{
	class EntityDescription
	{
	private:
		Properties m_properties;
	public:
		EntityDescription();
		EntityDescription(const EntityDescription& other);
		EntityDescription& EntityDescription::operator=(const EntityDescription& other);
		~EntityDescription();
	};
}

