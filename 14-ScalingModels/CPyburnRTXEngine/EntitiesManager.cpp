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
			const UINT& modelId = entity->GetEntityDescription()->GetProperties()->GetModelId();

			auto it = AssimpFactory::Models.find(modelId);
			if (it != AssimpFactory::Models.end())
			{
				AssimpFactory::Model* model = &it->second;
				if (!model->assimpFactory)
				{
					std::string modelPath = "..\\..\\Assets\\Models\\" + model->contentLocation + model->name;
					model->assimpFactory = std::make_unique<AssimpFactory>(model, modelPath);
					model->assimpFactory->CreateDeviceDependentResources(deviceResources);

					entity->CreateAssimpAnimations(model->GetAssimpFactoryPtr());
					entity->CreateDeviceDependentResources(deviceResources);

					//m_elfAnimated = std::make_unique<AssimpAnimations>(model->assimpFactory.get());
					//m_elfAnimated->CreateDeviceDependentResources(m_deviceResources);
				}
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
				EntityDescription* entityDescription = entity.GetEntityDescription();
				Properties* entityProperties = entityDescription->GetProperties();

				entityProperties->SetId(v["id"].GetUint());
				entityProperties->SetName(v["name"].GetString());
				entityProperties->SetPosition({ v["positionX"].GetFloat(), v["positionY"].GetFloat(), v["positionZ"].GetFloat() });
				entityProperties->SetScale({ v["scaleX"].GetFloat(), v["scaleY"].GetFloat(), v["scaleZ"].GetFloat() });
				entityProperties->SetRotation({ v["rotationX"].GetFloat(), v["rotationY"].GetFloat(), v["rotationZ"].GetFloat() });
				entityProperties->SetModelId(v["modelId"].GetUint());

				// load the model
				AssimpFactory::Model* ptrModel = AssimpFactory::LoadJsonByModelId(entityProperties->GetModelId());
				entity.SetAssimpFactoryModel(ptrModel);

				LoadedEntities.emplace(entityProperties->GetId(), std::move(entity));
			}
		}
	}
}