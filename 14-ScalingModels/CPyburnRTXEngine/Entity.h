#pragma once

#include "EntityDescription.h"
#include "AssimpFactory.h"
#include "AssimpAnimations.h"

namespace CPyburnRTXEngine
{
	class Entity
	{
	private:
		EntityDescription m_startingEntityDescription;

		AssimpFactory::Model* m_ptrAssimpFactoryModel = nullptr;
		AssimpAnimations* m_ptrAssimpAnimations = nullptr;
	public:
		void SetAssimpFactoryModel(AssimpFactory::Model* assimpFactoryModel) { m_ptrAssimpFactoryModel = assimpFactoryModel; }

		void SetAssimpAnimations(AssimpAnimations* assimpAnimations) { m_ptrAssimpAnimations = assimpAnimations; }

		EntityDescription* GetEntityDescription() { return &m_startingEntityDescription;  }
		
		Entity();
		~Entity();
	};
}
