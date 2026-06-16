#pragma once

#include "Properties.h"

namespace CPyburnRTXEngine
{
	class EntityDescription
	{
	private:
		Properties m_properties;

	public:
		Properties* GetProperties() { return &m_properties; }

		EntityDescription();
		EntityDescription(const EntityDescription&) = default;
		EntityDescription& operator=(const EntityDescription&) = default;
		~EntityDescription();
	};
}

