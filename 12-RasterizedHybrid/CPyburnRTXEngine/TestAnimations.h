#pragma once

#include "AssimpAnimations.h" // todo: move?
#include "Texture.h"

#include "BufferConstant.h"
#include "BufferHeap.h"
#include "BufferBlas.h"

#include "BoundingSphereRenderer.h"

namespace CPyburnRTXEngine
{
	class TestAnimations
	{
	private:
		// 14.3.b bottom-level acceleration structure
		struct AccelerationStructureBuffers
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
			Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
			Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDescResource;    // Used only for top-level AS

			void Release()
			{
				if (pScratch)
				{
					pScratch.Reset();
				}
				if (pResult)
				{
					pResult.Reset();
				}
				if (pInstanceDescResource)
				{
					pInstanceDescResource.Reset();
				}
			}
		};

		DX::DeviceResources* m_deviceResources = nullptr;

		void CreateCommandObjects();
		void CreateBuffers();
		void createAccelerationStructures();
		
		AccelerationStructureBuffers mpTopLevelAS[DX::DeviceResources::c_backBufferCount];
		
		UINT64 mTlasSize = 0;

		BufferHeap<XMFLOAT3> m_planeVertexBuffer;

		void RefitOrRebuildTLAS(ID3D12GraphicsCommandList4* commandList, UINT currentFrame, bool update);
		void RefitOrRebuildTLASNext();
		void WaitForFrameSlot(UINT frameIndex);
		UINT GetReadyFrameIndex() const;

		// Ray tracing pipeline state and root signature
		void createRtPipelineState();
		
		Microsoft::WRL::ComPtr<ID3D12StateObject> mpPipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mpEmptyRootSig;

		uint8_t* shaderTableEntryHelper(UINT entry, ID3D12StateObjectProperties* pRtsoProps, uint8_t* pData, const WCHAR* exportName, const Microsoft::WRL::ComPtr<ID3D12Resource>& resource = nullptr);
		void createShaderTable();
		Microsoft::WRL::ComPtr<ID3D12Resource> mpShaderTable;
		uint32_t mShaderTableEntrySize = 0;

		void createShaderResources();
		void createShaderResourcesForWindowSize();
		Microsoft::WRL::ComPtr<ID3D12Resource> mpOutputResource;

		UINT mUavPosition = 0;
		UINT mTlasSrvPosition[DX::DeviceResources::c_backBufferCount] = {};

		std::unique_ptr<AssimpAnimations> m_elfAnimated = nullptr;
		Texture::HeapTexture m_heapTextureDiffuse = {};

		struct EnvironmentData
		{
			XMFLOAT3 lightDirection = XMFLOAT3(0.5f, 0.5f, -0.5f);
		};
		EnvironmentData m_environmentData = {};
		BufferConstant<EnvironmentData> m_EnvironmentCb;

		struct InstanceData
		{
			XMMATRIX world = XMMatrixIdentity();
		};
		std::vector<InstanceData> m_instanceData;

		void createConstantBuffer();
				
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator[DX::DeviceResources::c_backBufferCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList[DX::DeviceResources::c_backBufferCount];
		std::atomic_uint64_t m_fenceValues[DX::DeviceResources::c_backBufferCount] = {};
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		Microsoft::WRL::Wrappers::Event m_fenceEvent;
		std::atomic_uint64_t m_nextFenceValue = 1;

		struct MaterialData
		{
			UINT baseColorTexIndex = 0;
			UINT normalTexIndex = 0;
			UINT ormTexIndex = 0;
		};
		std::vector<MaterialData> m_materialData;
		BufferHeap<MaterialData> m_materialDataBuffer;

		BufferBlas<AssimpFactory::VSVertices> m_blas;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_planeBlas;

		// todo: this will move eventually, but for now we can just load the model in the test class
		struct Model
		{
			UINT modelId;
			std::string name;
			UINT meshEntryLocation;
			std::string contentLocation;
			std::vector<std::string> textures;

			std::shared_ptr<AssimpFactory> assimpFactory = nullptr; // pointer to the ONE copy of the static model and resources
		};

		
		void LoadJson();

		BoundingSphereRenderer m_boundingSphereTest;

	public:
		static std::unordered_map<UINT, Model> Models;

		TestAnimations();
		~TestAnimations();
		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void CreateWindowSizeDependentResources(); // todo: this method when we visit refitting
		void Update(DX::StepTimer const& timer);
		void Render(CameraBase* camera);
		void Release();
	};
}
