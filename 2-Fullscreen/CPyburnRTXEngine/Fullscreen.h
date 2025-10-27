#pragma once

namespace CPyburnRTXEngine
{
	class Fullscreen
	{
	public:
		Fullscreen();
		~Fullscreen();

		void Update(DX::StepTimer const& timer);
		void Render();

		void CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResource);
		void CreateWindowSizeDependentResources();

	private:
		std::shared_ptr<DeviceResources> m_deviceResource;
		ComPtr<ID3D12PipelineState> m_scenePipelineState;
		ComPtr<ID3D12PipelineState> m_postPipelineState;
		ComPtr<ID3D12RootSignature> m_sceneRootSignature;
		ComPtr<ID3D12RootSignature> m_postRootSignature;

		std::wstring GetAssetFullPath(LPCWSTR assetName);
		// Root assets path.
		std::wstring m_assetsPath;

		ComPtr<ID3D12CommandAllocator> m_sceneCommandAllocators[DeviceResources::c_backBufferCount];
		ComPtr<ID3D12CommandAllocator> m_postCommandAllocators[DeviceResources::c_backBufferCount];

		ComPtr<ID3D12GraphicsCommandList> m_sceneCommandList;
		ComPtr<ID3D12GraphicsCommandList> m_postCommandList;
	};
} // namespace CPyburnRTXEngine
