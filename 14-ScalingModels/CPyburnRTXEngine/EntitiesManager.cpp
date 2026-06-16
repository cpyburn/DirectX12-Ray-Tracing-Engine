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

				LoadedEntities[entityProperties->GetId()] = entity;
			}
		}
	}
}