#include "pchlib.h"
#include "GraphicsContexts.h"

using namespace CPyburnRTXEngine;

UINT GraphicsContexts::c_descriptorSize;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GraphicsContexts::c_heap;

UINT GraphicsContexts::m_heapPositionCounter;
vector<UINT> GraphicsContexts::m_availableHeapPositions;
map<UINT, UINT> GraphicsContexts::m_multiUseHeapPositions;
mutex GraphicsContexts::m_mutexMultiUseHeapPositions;

GraphicsContexts::GraphicsContexts()
{

}

GraphicsContexts::~GraphicsContexts()
{

}

void GraphicsContexts::AddMultiHeapPosition(UINT heapPosition)
{
	m_mutexMultiUseHeapPositions.lock();
	auto iter = m_multiUseHeapPositions.find(heapPosition);
	if (iter != m_multiUseHeapPositions.end())
	{
		iter->second++;
	}
	else
	{
		m_multiUseHeapPositions.insert(std::pair<UINT, UINT>(heapPosition, 1));
	}
	m_mutexMultiUseHeapPositions.unlock();
}

bool GraphicsContexts::RemoveHeapPosition(UINT heapPosition)
{
	bool didErase = false;

	m_mutexMultiUseHeapPositions.lock();
	auto iter = m_multiUseHeapPositions.find(heapPosition);
	if (iter != m_multiUseHeapPositions.end())
	{
		// remove one from multiheap
		iter->second--;
		if (iter->second == 0)
		{
			// remove from multiheap if all are gone
			m_multiUseHeapPositions.erase(iter);
			DebugTrace("Heap position reclaimed: %s\n", (to_string(heapPosition)).c_str());
			m_availableHeapPositions.push_back(heapPosition);
			didErase = true;
		}
	}
	else
		m_availableHeapPositions.push_back(heapPosition); // if it wasn't a multiuse then reuse

	m_mutexMultiUseHeapPositions.unlock();

	return didErase;
}

UINT GraphicsContexts::GetAvailableHeapPosition()
{
	m_mutexMultiUseHeapPositions.lock();
	// NOTE: was having issues tying to claim previously used heap so commented out for now, just incriments until it runs out for now but added a catch
	assert(m_heapPositionCounter != UINT32_MAX);

	UINT value = m_heapPositionCounter;
	if (m_availableHeapPositions.size() > 0)
	{
		value = m_availableHeapPositions[0];
		DebugTrace("Heap position reused: %s\n", (to_string(value)).c_str());
		m_availableHeapPositions.erase(m_availableHeapPositions.begin());
	}
	else
	{
		m_heapPositionCounter++;
		DebugTrace("Heap position used: %s\n", (to_string(value)).c_str());
	}

	m_mutexMultiUseHeapPositions.unlock();

	return value;
}

void GraphicsContexts::CreateDeviceDependentResources(ID3D12Device* d3dDevice)
{
	c_descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = DeviceResources::c_backBufferCount // Vertex constant buffers per frame 
		+ 1; // Arbitrary large number for now.
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&c_heap)));
	c_heap->SetName(L"Descriptor Heap from GraphicsContexts");
}


