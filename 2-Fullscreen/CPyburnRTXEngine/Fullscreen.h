#pragma once

using namespace DirectX;

namespace CPyburnRTXEngine
{
	class Fullscreen
	{
	private:
		static const float QuadWidth;
		static const float QuadHeight;
		static const float LetterboxColor[4];
		static const float ClearColor[4];

		struct SceneVertex
		{
			XMFLOAT3 position;
			XMFLOAT4 color;
		};

		struct PostVertex
		{
			XMFLOAT4 position;
			XMFLOAT2 uv;
		};

		struct SceneConstantBuffer
		{
			XMFLOAT4X4 transform;
			XMFLOAT4 offset;
			UINT padding[44];
		};

		struct Resolution
		{
			UINT Width;
			UINT Height;
		};

		static const Resolution m_resolutionOptions[];
		static const UINT m_resolutionOptionsCount;
		static UINT m_resolutionIndex; // Index of the current scene rendering resolution from m_resolutionOptions.

		// cbv
		UINT m_cbcbvSrv;
	public:
		Fullscreen();
		~Fullscreen();

		void Update(DX::StepTimer const& timer);
		void Render();

		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources(const std::shared_ptr<DeviceResources>& deviceResource);

		

	private:
		std::shared_ptr<DeviceResources> m_deviceResource;
		ComPtr<ID3D12Device> m_device;

		ComPtr<ID3D12PipelineState> m_scenePipelineState;
		ComPtr<ID3D12PipelineState> m_postPipelineState;
		ComPtr<ID3D12RootSignature> m_sceneRootSignature;
		ComPtr<ID3D12RootSignature> m_postRootSignature;

		ComPtr<ID3D12Resource> m_intermediateRenderTarget;

		CD3DX12_VIEWPORT m_sceneViewport;
		CD3DX12_VIEWPORT m_postViewport;
		CD3DX12_RECT m_sceneScissorRect;
		CD3DX12_RECT m_postScissorRect;

		std::wstring GetAssetFullPath(LPCWSTR assetName);
		// Root assets path.
		std::wstring m_assetsPath;

		// App resources.
		ComPtr<ID3D12Resource> m_sceneVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_sceneVertexBufferView;
		ComPtr<ID3D12Resource> m_postVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_postVertexBufferView;
		ComPtr<ID3D12Resource> m_sceneConstantBuffer;
		SceneConstantBuffer m_sceneConstantBufferData;
		UINT8* m_pCbvDataBegin;

		UINT m_width;
		UINT m_height;
		float m_aspectRatio;

		void UpdatePostViewAndScissor();
		void LoadSizeDependentResources();
		void LoadSceneResolutionDependentResources();
		void UpdateTitle();
	};
} // namespace CPyburnRTXEngine
