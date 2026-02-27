#pragma once

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

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void Release();
	private:
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_scenePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_sceneRootSignature;

		// App resources.
		Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_sceneVertexBufferView = {};
		
		ConstantBuffer<SceneConstantBuffer> m_sceneConstantBuffer;
	};
} // namespace CPyburnRTXEngine
