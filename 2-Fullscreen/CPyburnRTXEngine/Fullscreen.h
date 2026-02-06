#pragma once

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
		SceneConstantBuffer m_sceneConstantBufferData;

		static const UINT c_alignedSceneConstantBuffer = (sizeof(SceneConstantBuffer) + 255) & ~255;
		UINT m_heapPositionSceneConstantBuffer[DeviceResources::c_backBufferCount];
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_gpuHandleSceneConstantBuffer[DeviceResources::c_backBufferCount];
		ComPtr<ID3D12Resource> m_sceneConstantBuffer;
		UINT8* m_pSceneConstantBufferDataBegin;

		UINT m_width = 1280;
		UINT m_height = 720;
	};
} // namespace CPyburnRTXEngine
