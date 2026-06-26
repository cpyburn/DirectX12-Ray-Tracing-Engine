#include "pchlib.h"
#include "EntitiesManager.h"

#include "Entity.h"

namespace CPyburnRTXEngine
{
	std::unordered_map<UINT, Entity> EntitiesManager::LoadedEntities;

	std::unordered_map<UINT, size_t> EntitiesManager::m_batchIndexByModelIdStatic;
	std::vector<EntitiesManager::Batch> EntitiesManager::m_visibleBatchesStatic;

	void EntitiesManager::AddVisible(Entity* entity, AssimpFactory::Model* model, UINT modelId, const XMMATRIX& world)
	{
		if (entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr()->IsSkinned())
			return;

		auto [it, inserted] = m_batchIndexByModelIdStatic.try_emplace(modelId, m_visibleBatchesStatic.size());

		if (inserted)
		{
			m_visibleBatchesStatic.push_back(Batch{ model, {} });
			m_visibleBatchesStatic.back().instanceIndices.reserve(64);
		}

		Batch& batch = m_visibleBatchesStatic[it->second];

		UINT instanceIndex = m_startingOffset + m_instanceCountStatic++;

		D3D12_RAYTRACING_INSTANCE_DESC& instance = m_instanceDescGpuMapped[instanceIndex];

		instance.InstanceID = instanceIndex;
		instance.InstanceContributionToHitGroupIndex = 0;
		instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

		XMMATRIX transpose = XMMatrixTranspose(world);
		memcpy(instance.Transform, &transpose, sizeof(instance.Transform));

		instance.AccelerationStructure = entity->GetAssimpFactoryModel()->GetBlasPtr()->GetResult()->GetGPUVirtualAddress();
		instance.InstanceMask = 0xFF;

		RtxScene::RtxModelData data{};

		if (auto* anims = entity->GetAssimpAnimations())
		{
			data.verticesSrvIndex = anims->GetAnimationCompute()->GetVertexOutputBuffer().HeapIndex;
			data.indicesSrvIndex = anims->GetAssimpFactory()->GetIndexBuffer()->HeapIndex;
		}
		else
		{
			auto* factory = entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr();
			data.verticesSrvIndex = factory->GetVertexBuffer()->HeapIndex;
			data.indicesSrvIndex = factory->GetIndexBuffer()->HeapIndex;
		}

		auto* modelPtr = entity->GetAssimpFactoryModel();
		data.baseColorTexIndex = modelPtr->texturesHeap[0].indexInMaterialBuffer;
		data.normalTexIndex = modelPtr->texturesHeapNrm[0].indexInMaterialBuffer;
		data.ormTexIndex = modelPtr->texturesHeapOrm[0].indexInMaterialBuffer;

		m_modelDataGpuMapped[instanceIndex] = data;

		batch.instanceIndices.push_back(instanceIndex);
	}

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
				if (!model->GetAssimpFactoryPtr())
				{
					std::string modelPath = "..\\..\\Assets\\Models\\" + model->contentLocation + model->name;
					model->CreateAssimpFactory(modelPath);
					model->GetAssimpFactoryPtr()->CreateDeviceDependentResources(deviceResources);
				}

				entity->CreateAssimpAnimations(model->GetAssimpFactoryPtr());
				entity->CreateDeviceDependentResources(deviceResources);
			}
		}

		// todo: move this eventually to game.cpp
		{
			// initialize Gpu resources
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(0, nullptr);

			CreateBuffers(commandList.Get());

			DX::ThrowIfFailed(commandList->Close());
			ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
			m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
			m_deviceResources->WaitForGpu();

			//m_deviceResources->GetCurrentFrameResource()->ResetCommandList(0);
		}
	}

	// todo: not sure I want to keep this static, need to think about it
	void EntitiesManager::CreateBuffers(ID3D12GraphicsCommandList4* commandList)
	{
		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			Entity* entity = &loadedEntity.second;
			AssimpAnimations* animation = entity->GetAssimpAnimations();
			if (animation)
			{
				animation->CreateBuffers(commandList);
				animation->CreateShaderResources();
			}
			else if(!entity->GetAssimpFactoryModel()->GetBlasPtr())
			{
				entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr()->CreateBuffers(commandList);
				entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr()->CreateShaderResources();
			}
		}
	}

	void EntitiesManager::Update(DX::StepTimer const& timer, CameraBase* camera)
	{
		m_instanceCountStatic = 0;
		m_startingOffset = 1; // todo: make this dynamic based on terrain, right now 1 is fine
		m_batchIndexByModelIdStatic.clear();
		m_visibleBatchesStatic.clear();

		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			Entity* entity = &loadedEntity.second;
			entity->Update();

			AssimpAnimations* animation = entity->GetAssimpAnimations();
			if (animation)
			{
				animation->Update(timer);
			}
			
			AssimpFactory* model = entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr();
			model->GetBoundingBoxRenderer().Update(entity->GetEntityDescriptionCurrentState()->GetProperties()->GetXMTransform(), camera);
			model->GetBoundingSphereRenderer().Update(entity->GetEntityDescriptionCurrentState()->GetProperties()->GetXMTransform(), camera);

			AddVisible(entity, model->GetModel(), model->GetModel()->modelId, entity->GetEntityDescriptionCurrentState()->GetProperties()->GetXMTransform());
		}
	}

	void EntitiesManager::RenderBounding(ID3D12GraphicsCommandList4* commandList)
	{
		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			Entity* entity = &loadedEntity.second;
			AssimpFactory* model = entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr();

			model->GetBoundingBoxRenderer().Render(commandList);
			model->GetBoundingSphereRenderer().Render(commandList);
		}
	}

	void EntitiesManager::DispatchAndUpdateBlas(ID3D12GraphicsCommandList4* commandList)
	{
		for (auto& loadedEntity : EntitiesManager::LoadedEntities)
		{
			Entity* entity = &loadedEntity.second;
			AssimpAnimations* animation = loadedEntity.second.GetAssimpAnimations();
			if (animation)
			{
				animation->GetAnimationCompute()->Dispatch(commandList);

				D3D12_RESOURCE_BARRIER uavBarrier = {};
				uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				uavBarrier.UAV.pResource = animation->GetAnimationCompute()->GetVertexOutputBuffer().DefaultHeapResource.Get();
				commandList->ResourceBarrier(1, &uavBarrier);

				animation->GetAnimationBlasPtr()->UpdateBlas(commandList);
			}
			else if (entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr())
			{
				//D3D12_RESOURCE_BARRIER uavBarrier = {};
				//uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				//uavBarrier.UAV.pResource = entity->GetAssimpFactoryModel()->GetAssimpFactoryPtr()->GetVertexBuffer()->DefaultHeapResource.Get();
				//commandList->ResourceBarrier(1, &uavBarrier);

				//entity->GetAssimpFactoryModel()->GetBlasPtr()->UpdateBlas(commandList);
			}
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