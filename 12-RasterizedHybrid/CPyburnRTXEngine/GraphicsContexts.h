#pragma once

namespace CPyburnRTXEngine
{
	class GraphicsContexts
	{
	private:
		static UINT m_heapPositionCounter;
		static std::vector<UINT> m_availableHeapPositions;
		static std::unordered_map<UINT, UINT> m_multiUseHeapPositions;
		static std::mutex m_mutexMultiUseHeapPositions;

#pragma region Position Color
		static Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStatePositionColorLine;
		static Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStatePositionColorTriangle;
		static Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignaturePositionColor;
		static void CreateRootSignatureAndPipelinePositionColorInstanced(DX::DeviceResources* deviceResources);
#pragma endregion

	public:
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> c_heap;
		static UINT c_descriptorSize;

		GraphicsContexts();
		~GraphicsContexts();

		static CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(const UINT& index);
		static CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(const UINT& index);
		static void AddMultiHeapPosition(UINT heapPosition);
		static bool RemoveHeapPosition(UINT heapPosition);
		static UINT GetAvailableHeapPosition();

		static void CreateDeviceDependentResources(ID3D12Device* d3dDevice);
		static Microsoft::WRL::ComPtr<IDxcBlob> CompileHlslLibrary(ID3D12Device* d3dDevice, std::wstring filename, std::wstring shaderType, std::wstring shaderVersion);
		static Microsoft::WRL::ComPtr<IDxcBlob> CompileDXRLibrary(const wchar_t* filename);
		static void CreateRootSignaturesAndPipelines(DX::DeviceResources* deviceResources);


		static void Release();
	};
}

