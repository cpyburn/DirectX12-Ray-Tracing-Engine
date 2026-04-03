#pragma once

#include "AssimpFactory.h" // todo: move?
#include "Texture.h"

namespace CPyburnRTXEngine
{
	class TestInstances
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

		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		// Acceleration structure buffers and sizes
		void createAccelerationStructures();
		Microsoft::WRL::ComPtr<ID3D12Resource> m_triangleVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_triangleIndicesBuffer;
		AccelerationStructureBuffers mpTopLevelAS[DX::DeviceResources::c_backBufferCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> mpBottomLevelAS;
		
		UINT64 mTlasSize = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_planeVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mpBottomLevelAS1;

		void RefitOrRebuildTLAS(ID3D12GraphicsCommandList4* commandList, UINT currentFrame, bool update);
		void RefitOrRebuildTLASNext();
		void WaitForFrameSlot(UINT frameIndex);
		UINT GetReadyFrameIndex() const;

		// Ray tracing pipeline state and root signature
		void createRtPipelineState();
		
		Microsoft::WRL::ComPtr<ID3D12StateObject> mpPipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mpEmptyRootSig;

		std::vector<uint8_t> LoadBinaryFile(const wchar_t* path);
		Microsoft::WRL::ComPtr<IDxcBlob> CompileDXRLibrary(const wchar_t* filename);

		uint8_t* shaderTableEntryHelper(UINT entry, ID3D12StateObjectProperties* pRtsoProps, uint8_t* pData, const WCHAR* exportName, const Microsoft::WRL::ComPtr<ID3D12Resource>& resource = nullptr);
		void createShaderTable();
		Microsoft::WRL::ComPtr<ID3D12Resource> mpShaderTable;
		uint32_t mShaderTableEntrySize = 0;

		void createShaderResources();
		Microsoft::WRL::ComPtr<ID3D12Resource> mpOutputResource;

		UINT mUavPosition = 0;
		UINT mTlasSrvPosition[DX::DeviceResources::c_backBufferCount] = {};
		UINT mVertexBufferSrvPosition = 0;
		UINT mIndexBufferSrvPosition = 0;
		AssimpFactory m_assimpFactory;
		Texture::HeapTexture m_heapTextureDiffuse = {};

		struct EnvironmentData
		{
			XMFLOAT3 lightDirection = XMFLOAT3(0.5f, 0.5f, -0.5f);
		};
		EnvironmentData m_environmentData = {};
		ConstantBuffer<EnvironmentData> m_EnvironmentCb;

		struct InstanceData
		{
			XMMATRIX world = XMMatrixIdentity();
		};
		std::vector<InstanceData> m_instanceData;

		void createConstantBuffer();
		static const UINT m_instanceCount = 4;
				
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator[DX::DeviceResources::c_backBufferCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList[DX::DeviceResources::c_backBufferCount];
		std::atomic_uint64_t m_fenceValues[DX::DeviceResources::c_backBufferCount] = {};
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		Microsoft::WRL::Wrappers::Event m_fenceEvent;
		std::atomic_uint64_t m_nextFenceValue = 1;

	public:
		TestInstances();
		~TestInstances();
		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateWindowSizeDependentResources(); // todo: this method when we visit refitting
		void Update(DX::StepTimer const& timer);
		void Render(CameraBase* camera);
		void Release();
	};
}
