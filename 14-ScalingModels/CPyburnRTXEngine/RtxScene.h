#pragma once

#include "AssimpAnimations.h" // todo: move?
#include "Texture.h"

#include "BufferConstant.h"
#include "BufferHeap.h"
#include "BufferBlas.h"
#include "Environment.h"

namespace CPyburnRTXEngine
{
	class RtxScene
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

		UINT mUavPosition = MAXUINT;
		UINT mTlasSrvPosition[DX::DeviceResources::c_backBufferCount] = {};

		struct InstanceData
		{
			XMMATRIX world = XMMatrixIdentity();
		};
		std::vector<InstanceData> m_instanceData;
				
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator[DX::DeviceResources::c_backBufferCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList[DX::DeviceResources::c_backBufferCount];
		std::atomic_uint64_t m_fenceValues[DX::DeviceResources::c_backBufferCount] = {};
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		Microsoft::WRL::Wrappers::Event m_fenceEvent;
		std::atomic_uint64_t m_nextFenceValue = 1;

		struct RtxModelData
		{
			UINT verticesSrvIndex = MAXUINT;
			UINT indicesSrvIndex = MAXUINT;
			UINT baseColorTexIndex = 0;
			UINT normalTexIndex = 0;
			UINT ormTexIndex = 0;
		};
		std::vector<RtxModelData> m_modelDataPerInstance; // todo: always writing to this, needs to be multibuffered?
		BufferHeap<RtxModelData> m_modelDataPerInstanceBuffer;

		BufferBlas<XMFLOAT3> m_planeBlas;
		
		Environment m_environment;

	public:
		RtxScene();
		~RtxScene();
		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void CreateWindowSizeDependentResources(); // todo: this method when we visit refitting
		void Update(DX::StepTimer const& timer, CameraBase* camera);
		void Render(CameraBase* camera);
		void Release();
	};
}
