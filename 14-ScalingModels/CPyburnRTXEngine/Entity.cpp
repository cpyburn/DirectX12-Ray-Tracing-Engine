#include "pchlib.h"
#include "Entity.h"

namespace CPyburnRTXEngine
{
	void Entity::SetAssimpFactoryModel(AssimpFactory::Model* assimpFactoryModel)
	{
		m_assimpFactoryModelPtr = assimpFactoryModel;

		if (assimpFactoryModel->GetAssimpFactoryPtr() && !m_assimpAnimationsOwner)
		{
			CreateAssimpAnimations(m_assimpFactoryModelPtr->GetAssimpFactoryPtr());
		}
	}

	void Entity::CreateAssimpAnimations(AssimpFactory* assimpFactory)
	{
		if (!m_assimpAnimationsOwner)
		{
			m_assimpAnimationsOwner = std::make_unique<AssimpAnimations>(assimpFactory);
		}
	}

	void Entity::Update()
	{
		m_entityDescriptionCurrentState.GetProperties()->Update();
	}

	void Entity::CreateDeviceDependentResources(DX::DeviceResources* deviceResources)
	{
		m_deviceResources = deviceResources;

		if (m_assimpAnimationsOwner)
		{
			m_assimpAnimationsOwner->CreateDeviceDependentResources(deviceResources);
#ifdef _DEBUG
			if (m_assimpFactoryModelPtr)
			{
				m_assimpFactoryModelPtr->GetAssimpFactoryPtr()->GetBoundingBoxRenderer().CreateDeviceDependentResources(deviceResources); // only 1 instance for entity
				m_assimpFactoryModelPtr->GetAssimpFactoryPtr()->GetBoundingBoxRenderer().SetColor(Colors::Blue);
				m_assimpFactoryModelPtr->GetAssimpFactoryPtr()->GetBoundingSphereRenderer().CreateDeviceDependentResources(deviceResources); // only 1 instance for entity
			}
#else

#endif

		}
	}
}
