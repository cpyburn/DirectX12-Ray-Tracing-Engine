#include "pchlib.h"
#include "EntityDescription.h"

namespace CPyburnRTXEngine
{
	EntityDescription::EntityDescription()
	{

	}

	EntityDescription::EntityDescription(const EntityDescription& other)
	{
		m_properties = other.m_properties;
	}

	EntityDescription& EntityDescription::operator=(const EntityDescription& other)
	{
		m_properties = other.m_properties;
	}

	EntityDescription::~EntityDescription()
	{

	}
}