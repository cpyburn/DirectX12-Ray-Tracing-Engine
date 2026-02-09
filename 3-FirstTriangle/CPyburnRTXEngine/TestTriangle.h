#pragma once

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
	public:
		TestTriangle();
		~TestTriangle();
		void CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources);
		//void Update(DX::StepTimer const& timer);
		//void Render();
		void Release();
	};
}
