#pragma once

#include "EntityDescription.h"
#include "AssimpFactory.h"
#include "AssimpAnimations.h"

namespace CPyburnRTXEngine
{
	class Entity
	{
	private:
		DX::DeviceResources* m_deviceResources = nullptr;

		EntityDescription m_startingEntityDescription;

		AssimpFactory::Model* m_assimpFactoryModelPtr = nullptr;
		std::unique_ptr<AssimpAnimations> m_assimpAnimationsOwner = nullptr; // Entities will be the owner of the animation that their entity needs

	public:
		void SetAssimpFactoryModel(AssimpFactory::Model* assimpFactoryModel);
		void CreateAssimpAnimations(AssimpFactory* assimpFactory);
		AssimpAnimations* GetAssimpAnimations() { return m_assimpAnimationsOwner.get(); }

		EntityDescription* GetEntityDescription() { return &m_startingEntityDescription;  }
		
		Entity() = default;
		Entity(const Entity&) = delete;
		Entity& operator=(const Entity&) = delete;
		Entity(Entity&&) noexcept = default;
		Entity& operator=(Entity&&) noexcept = default;
		~Entity() = default;

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
	};
}
