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
		template<typename T>
		struct ConstantBuffer
		{
		public:
			static constexpr UINT AlignedSize = (sizeof(T) + 255) & ~255;

			T										Data{};
			CD3DX12_GPU_DESCRIPTOR_HANDLE			GpuHandle[DeviceResources::c_backBufferCount]{};
			Microsoft::WRL::ComPtr<ID3D12Resource>	Resource;
			UINT8*									MappedData = nullptr;
		};

		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> c_heap[DeviceResources::c_backBufferCount];
		static UINT c_descriptorSize;

		GraphicsContexts();
		~GraphicsContexts();

		static void AddMultiHeapPosition(UINT heapPosition);
		static bool RemoveHeapPosition(UINT heapPosition);
		static UINT GetAvailableHeapPosition();

		void CreateDeviceDependentResources(ID3D12Device* d3dDevice);
	};
}

