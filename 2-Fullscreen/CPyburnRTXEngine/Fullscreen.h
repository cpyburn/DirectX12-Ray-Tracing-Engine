#pragma once

#include "GraphicsContexts.h"

using namespace DirectX;

namespace CPyburnRTXEngine
{
	class Fullscreen
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
		Fullscreen();
		~Fullscreen();

		void Update(DX::StepTimer const& timer);
		void Render();

		void CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResource);

	private:
		std::shared_ptr<DeviceResources> m_deviceResource;

		ComPtr<ID3D12PipelineState> m_scenePipelineState;
		ComPtr<ID3D12RootSignature> m_sceneRootSignature;

		// App resources.
		ComPtr<ID3D12Resource> m_sceneVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_sceneVertexBufferView;

		UINT m_heapPosition[DeviceResources::c_backBufferCount]{};
		GraphicsContexts::ConstantBuffer<SceneConstantBuffer> m_sceneConstantBuffer[DeviceResources::c_backBufferCount];
	};
} // namespace CPyburnRTXEngine
