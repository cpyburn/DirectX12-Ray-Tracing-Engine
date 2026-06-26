#pragma once

#include "AssimpFactory.h"
#include "RtxScene.h"

namespace CPyburnRTXEngine
{
	class CameraBase; // forward declaration
	class Entity; // forward declaration

	class EntitiesManager
	{
	public:
		inline static UINT m_maxEntities = 8192; // 8K //65536; // 64K
		inline static UINT m_startingOffset = 1; // todo: 1 is hard coded for terrain/plane, but needs to be fixed

		struct Batch
		{
			AssimpFactory::Model* model;   // non-owning
			std::vector<XMMATRIX> instances;
			std::vector<UINT> instanceIndices;
		};

		inline static D3D12_RAYTRACING_INSTANCE_DESC* m_instanceDescGpuMapped[DX::DeviceResources::c_backBufferCount] =
		{
			nullptr
		};

		static std::unordered_map<UINT, size_t> m_batchIndexByModelIdStatic;
		static std::vector<Batch> m_visibleBatchesStatic;
		inline static UINT m_instanceCountStatic;
		inline static RtxScene::RtxModelData* m_modelDataGpuMapped = nullptr;

	private:
		void AddVisible(Entity* entity, AssimpFactory::Model* model, UINT modelId, const XMMATRIX& world);

		DX::DeviceResources* m_deviceResources = nullptr;
	public:
		static std::unordered_map<UINT, Entity> LoadedEntities;
		
		EntitiesManager();
		~EntitiesManager();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void CreateBuffers(ID3D12GraphicsCommandList4* commandList);
		void Update(DX::StepTimer const& timer, CameraBase* camera);
		void RenderBounding(ID3D12GraphicsCommandList4* commandList);
		void DispatchAndUpdateBlas(ID3D12GraphicsCommandList4* commandList);
		void LoadJson();
	};
}

