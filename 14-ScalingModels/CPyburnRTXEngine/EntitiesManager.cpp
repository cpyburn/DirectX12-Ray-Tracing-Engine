#include "pchlib.h"
#include "EntitiesManager.h"

#include "Entity.h"

namespace CPyburnRTXEngine
{
	std::unordered_map<UINT, Entity> EntitiesManager::LoadedEntities;

	EntitiesManager::EntitiesManager()
	{
		LoadJson();
	}

	EntitiesManager::~EntitiesManager()
	{
	}

	void EntitiesManager::CreateDeviceDependentResources(DX::DeviceResources* deviceResources)
	{
		m_deviceResources = deviceResources;

		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			Entity* entity = &loadedEntity.second;
			const UINT& modelId = entity->GetEntityDescriptionCurrentState()->GetProperties()->GetModelId();

			auto it = AssimpFactory::Models.find(modelId);
			if (it != AssimpFactory::Models.end())
			{
				AssimpFactory::Model* model = &it->second;
				if (!model->assimpFactory)
				{
					std::string modelPath = "..\\..\\Assets\\Models\\" + model->contentLocation + model->name;
					model->assimpFactory = std::make_unique<AssimpFactory>(model, modelPath);
					model->assimpFactory->CreateDeviceDependentResources(deviceResources);
				}

				entity->CreateAssimpAnimations(model->GetAssimpFactoryPtr());
				entity->CreateDeviceDependentResources(deviceResources);
			}
		}
	}

	// todo: not sure I want to keep this static, need to think about it
	void EntitiesManager::CreateBuffers(ID3D12GraphicsCommandList4* commandList)
	{
		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			AssimpAnimations* animation = loadedEntity.second.GetAssimpAnimations();
			animation->CreateBuffers(commandList);
			animation->CreateShaderResources();
		}
	}

	void EntitiesManager::Update(DX::StepTimer const& timer, CameraBase* camera)
	{
		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			Entity* entity = &loadedEntity.second;
			entity->Update();

			AssimpAnimations* animation = entity->GetAssimpAnimations();
			animation->Update(timer);

			AssimpFactory* model = animation->GetAssimpFactory();
			model->GetBoundingBoxRenderer().Update(XMMatrixIdentity(), camera);
			model->GetBoundingSphereRenderer().Update(XMMatrixIdentity(), camera);
		}
	}

	void EntitiesManager::RenderBounding(ID3D12GraphicsCommandList4* commandList)
	{
		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			AssimpAnimations* animation = loadedEntity.second.GetAssimpAnimations();
			animation->GetAssimpFactory()->GetBoundingBoxRenderer().Render(commandList);
			animation->GetAssimpFactory()->GetBoundingSphereRenderer().Render(commandList);
		}
	}

	void EntitiesManager::DispatchAndUpdateBlas(ID3D12GraphicsCommandList4* commandList)
	{
		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			AssimpAnimations* animation = loadedEntity.second.GetAssimpAnimations();
			animation->GetAnimationCompute()->Dispatch(commandList);

			D3D12_RESOURCE_BARRIER uavBarrier = {};
			uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uavBarrier.UAV.pResource = animation->GetAnimationCompute()->GetVertexOutputBuffer().DefaultHeapResource.Get();
			commandList->ResourceBarrier(1, &uavBarrier);

			animation->GetAnimationBlas()->UpdateDynamicBlas(commandList);
		}
	}

	void EntitiesManager::LoadJson()
	{
		// see if entities have already been loaded
		if (LoadedEntities.size() > 0)
		{
			return;
		}

		// load json
		{
			std::string filePath = "../../Assets/Json/Entities.json";
			rapidjson::Document doc = LoadJsonDocument(filePath);

			const auto& arr = doc["entities"];

			for (auto& v : arr.GetArray()) {
				Entity entity;
				EntityDescription* entityDescription = entity.GetEntityDescriptionCurrentState();
				Properties* entityProperties = entityDescription->GetProperties();

				entityProperties->SetId(v["id"].GetUint());
				entityProperties->SetName(v["name"].GetString());
				entityProperties->SetPosition({ v["positionX"].GetFloat(), v["positionY"].GetFloat(), v["positionZ"].GetFloat() });
				entityProperties->SetScale({ v["scaleX"].GetFloat(), v["scaleY"].GetFloat(), v["scaleZ"].GetFloat() });
				entityProperties->SetRotation({ v["rotationX"].GetFloat(), v["rotationY"].GetFloat(), v["rotationZ"].GetFloat() });
				entityProperties->SetModelId(v["modelId"].GetUint());

				entity.SetEntityDescriptionInitialState(*entityDescription); // set the initial entity description, this should always match the created stated

				// load the model
				AssimpFactory::Model* ptrModel = AssimpFactory::LoadJsonByModelId(entityProperties->GetModelId());
				entity.SetAssimpFactoryModel(ptrModel);

				LoadedEntities.emplace(entityProperties->GetId(), std::move(entity));
			}
		}
	}
}