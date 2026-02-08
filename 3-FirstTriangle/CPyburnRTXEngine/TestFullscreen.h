#pragma once

#include "ConstantBuffer.h"

using namespace DirectX;

namespace CPyburnRTXEngine
{
	class TestFullscreen
	{
	private:
		static const float QuadWidth;
		static const float QuadHeight;
		static const float ClearColor[4];

		struct SceneVertex
		{
			XMFLOAT3 position;
			XMFLOAT4 color;
		};

		struct SceneConstantBuffer
		{
			XMFLOAT4X4 transform;
			XMFLOAT4 offset;
		};
	public:
		TestFullscreen();
		~TestFullscreen();

		void Update(DX::StepTimer const& timer);
		void Render();

		void CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources);
		void Release();
	private:
		std::shared_ptr<DeviceResources> m_deviceResources;

		ComPtr<ID3D12PipelineState> m_scenePipelineState;
		ComPtr<ID3D12RootSignature> m_sceneRootSignature;

		// App resources.
		ComPtr<ID3D12Resource> m_sceneVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_sceneVertexBufferView;
		
		ConstantBuffer<SceneConstantBuffer, DeviceResources::c_backBufferCount> m_sceneConstantBuffer;
	};
} // namespace CPyburnRTXEngine
