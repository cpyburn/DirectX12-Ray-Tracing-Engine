#pragma once

using namespace DirectX;
using namespace std;

namespace CPyburnRTXEngine
{
	class GraphicsContexts
	{
	private:
		static UINT m_heapPositionCounter;
		static vector<UINT> m_availableHeapPositions;
		static map<UINT, UINT> m_multiUseHeapPositions;
		static mutex m_mutexMultiUseHeapPositions;

	public:
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> c_heap;
		static UINT c_descriptorSize;

		GraphicsContexts();
		~GraphicsContexts();

		static D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(const UINT& index);
		static D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(const UINT& index);
		static void AddMultiHeapPosition(UINT heapPosition);
		static bool RemoveHeapPosition(UINT heapPosition);
		static UINT GetAvailableHeapPosition();

		void CreateDeviceDependentResources(ID3D12Device* d3dDevice);
		void Release();
	};
}

