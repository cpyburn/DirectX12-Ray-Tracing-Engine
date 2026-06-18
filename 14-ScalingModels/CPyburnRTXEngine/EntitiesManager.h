#pragma once

#include "AssimpFactory.h"

namespace CPyburnRTXEngine
{
	class CameraBase;
	class Entity;

	class EntitiesManager
	{
	public:
		struct VisibleInstance
		{
			Entity* entity;      // non-owning
			XMMATRIX world;
		};

		struct Batch
		{
			AssimpFactory::Model* model;   // non-owning
			std::vector<VisibleInstance> instances;
		};

		static std::unordered_map<UINT, size_t> m_batchIndexByModelId;
		static std::vector<Batch> m_visibleBatches;

		static void AddVisible(Entity* entity, AssimpFactory::Model* model, UINT modelId, const XMMATRIX& world);

	private:
		DX::DeviceResources* m_deviceResources = nullptr;
	public:
		static std::unordered_map<UINT, Entity> LoadedEntities;
		

		EntitiesManager();
		~EntitiesManager();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		static void CreateBuffers(ID3D12GraphicsCommandList4* commandList);
		static void Update(DX::StepTimer const& timer, CameraBase* camera);
		static void RenderBounding(ID3D12GraphicsCommandList4* commandList);
		static void DispatchAndUpdateBlas(ID3D12GraphicsCommandList4* commandList);
		static void LoadJson();
	};
}

