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

		static void AddMultiHeapPosition(UINT heapPosition);
		static bool RemoveHeapPosition(UINT heapPosition);
		static UINT GetAvailableHeapPosition();

		void CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources);
		task<void> CreatePipelines(const std::shared_ptr<DeviceResources>& deviceResources);
	};
}

