#pragma once

// todo: move to pch file?
#include <dxcapi.h>
#include <sstream>
#include "ConstantBuffer.h"

namespace CPyburnRTXEngine
{
	class TestTriangle
	{
	private:
		// 14.3.b bottom-level acceleration structure
		struct AccelerationStructureBuffers
		{
			ComPtr<ID3D12Resource> pScratch;
			ComPtr<ID3D12Resource> pResult;
			ComPtr<ID3D12Resource> pInstanceDescResource;    // Used only for top-level AS

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

		std::shared_ptr<DeviceResources> m_deviceResources;
		// Acceleration structure buffers and sizes
		void createAccelerationStructures();
		ComPtr<ID3D12Resource> mpVertexBuffer;
		AccelerationStructureBuffers mpTopLevelAS[DeviceResources::c_backBufferCount];
		ComPtr<ID3D12Resource> mpBottomLevelAS;
		
		UINT64 mTlasSize = 0;

		ComPtr<ID3D12Resource> mpVertexBuffer1;
		ComPtr<ID3D12Resource> mpBottomLevelAS1;

		void RefitOrRebuildTLAS(ID3D12GraphicsCommandList4* commandList, UINT currentFrame, bool update);
		XMMATRIX m_xmIdentity[3] = {};

		// Ray tracing pipeline state and root signature
		void createRtPipelineState();
		
		ComPtr<ID3D12StateObject> mpPipelineState;
		ComPtr<ID3D12RootSignature> mpEmptyRootSig;

		std::vector<uint8_t> LoadBinaryFile(const wchar_t* path);
		ComPtr<IDxcBlob> CompileDXRLibrary(const wchar_t* filename);

		uint8_t* shaderTableEntryHelper(UINT entry, ID3D12StateObjectProperties* pRtsoProps, uint8_t* pData, const WCHAR* exportName, const ComPtr<ID3D12Resource>& resource = nullptr);
		void createShaderTable();
		ComPtr<ID3D12Resource> mpShaderTable;
		uint32_t mShaderTableEntrySize = 0;

		void createShaderResources();
		ComPtr<ID3D12Resource> mpOutputResource;
		UINT mUavPosition = 0;
		UINT mSrvPosition[DeviceResources::c_backBufferCount] = {};
		UINT mVertexBufferSrvPosition = 0;

		void createConstantBuffer();
		static const UINT countOfConstantBuffers = 3;
		ConstantBuffer<XMFLOAT4[9]> mpConstantBuffer[countOfConstantBuffers];

	public:
		TestTriangle();
		~TestTriangle();
		void CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources);
		void CreateWindowSizeDependentResources(); // todo: this method when we visit refitting
		void Update(DX::StepTimer const& timer);
		void Render();
		void Release();
	};
}
