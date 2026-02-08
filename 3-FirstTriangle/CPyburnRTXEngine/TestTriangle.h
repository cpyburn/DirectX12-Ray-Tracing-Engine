#pragma once

namespace CPyburnRTXEngine
{
	class TestTriangle
	{
	private:
		void createAccelerationStructures();
		ComPtr<ID3D12Resource> mpVertexBuffer;
		ComPtr<ID3D12Resource> mpTopLevelAS;
		ComPtr<ID3D12Resource> mpBottomLevelAS;
		UINT mTlasSize;
	public:
		TestTriangle();
		~TestTriangle();
		void Update(DX::StepTimer const& timer);
		void Render();
		void Release();
	};
}
