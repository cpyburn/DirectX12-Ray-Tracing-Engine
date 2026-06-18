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

		EntityDescription m_entityDescriptionInitialState; // starting description or respawn or reverting
		EntityDescription m_entityDescriptionCurrentState; // current modified description

		AssimpFactory::Model* m_assimpFactoryModelPtr = nullptr;
		std::unique_ptr<AssimpAnimations> m_assimpAnimationsOwner = nullptr; // Entities will be the owner of the animation that their entity needs

	public:
		void SetAssimpFactoryModel(AssimpFactory::Model* assimpFactoryModel);
		void CreateAssimpAnimations(AssimpFactory* assimpFactory);
		AssimpAnimations* GetAssimpAnimations() { return m_assimpAnimationsOwner.get(); }

		void SetEntityDescriptionInitialState(const EntityDescription& entityDescriptionInitialState) { m_entityDescriptionInitialState = entityDescriptionInitialState; }
		EntityDescription* GetEntityDescriptionCurrentState() { return &m_entityDescriptionCurrentState;  }
		
		Entity() = default;
		Entity(const Entity&) = delete;
		Entity& operator=(const Entity&) = delete;
		Entity(Entity&&) noexcept = default;
		Entity& operator=(Entity&&) noexcept = default;
		~Entity() = default;

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
	};
}
