#pragma once

#include "EntityDescription.h"

namespace CPyburnRTXEngine
{
	class Entity
	{
	private:
		EntityDescription m_startingEntityDescription;
	public:
		EntityDescription* GetEntityDescription() { return &m_startingEntityDescription;  }
		
		Entity();
		~Entity();
	};
}
