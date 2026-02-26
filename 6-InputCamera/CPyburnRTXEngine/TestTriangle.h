#pragma once

namespace CPyburnRTXEngine
{
	class TestTriangle
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
		Microsoft::WRL::ComPtr<ID3D12Resource> mpVertexBuffer;
		AccelerationStructureBuffers mpTopLevelAS[DX::DeviceResources::c_backBufferCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> mpBottomLevelAS;
		
		UINT64 mTlasSize = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> mpVertexBuffer1;
		Microsoft::WRL::ComPtr<ID3D12Resource> mpBottomLevelAS1;

		void RefitOrRebuildTLAS(ID3D12GraphicsCommandList4* commandList, UINT currentFrame, bool update);
		XMMATRIX m_xmIdentity[3] = {};

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
		UINT mSrvPosition[DX::DeviceResources::c_backBufferCount] = {};
		UINT mVertexBufferSrvPosition = 0;

		void createConstantBuffer();
		static const UINT countOfConstantBuffers = 3;
		ConstantBuffer<XMFLOAT4[9]> mpConstantBuffer[countOfConstantBuffers];

	public:
		TestTriangle();
		~TestTriangle();
		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateWindowSizeDependentResources(); // todo: this method when we visit refitting
		void Update(DX::StepTimer const& timer);
		void Render();
		void Release();
	};
}
