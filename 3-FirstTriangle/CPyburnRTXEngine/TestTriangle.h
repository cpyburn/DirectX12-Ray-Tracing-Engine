#pragma once

// todo: move to pch file?
#include <dxcapi.h>
#include <sstream>

namespace CPyburnRTXEngine
{
	class TestTriangle
	{
	private:
		std::shared_ptr<DeviceResources> m_deviceResources;
		// Acceleration structure buffers and sizes
		void createAccelerationStructures();
		ComPtr<ID3D12Resource> mpVertexBuffer;
		ComPtr<ID3D12Resource> mpTopLevelAS;
		ComPtr<ID3D12Resource> mpBottomLevelAS;
		UINT mTlasSize;
		// Ray tracing pipeline state and root signature
		void createRtPipelineState();
		
		ComPtr<ID3D12StateObject> mpPipelineState;
		ComPtr<ID3D12RootSignature> mpEmptyRootSig;

		std::vector<uint8_t> LoadBinaryFile(const wchar_t* path);
		ComPtr<IDxcBlob> CompileDXRLibrary(const wchar_t* filename);
		D3D12_STATE_SUBOBJECT CreateDxilSubobject();
		ComPtr<IDxcBlob> CreateDxilSubobjectSample();

		void createShaderResources();
		ComPtr<ID3D12Resource> mpOutputResource;
	public:
		TestTriangle();
		~TestTriangle();
		void CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources);
		//void Update(DX::StepTimer const& timer);
		//void Render();
		void Release();
	};
}
