#pragma once

#include "BoundingRendererParent.h"

namespace CPyburnRTXEngine
{
	class BoundingBoxRenderer: public BoundingBox, public BoundingRendererParent
	{
	private:
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView[DX::DeviceResources::c_backBufferCount] = {};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView[DX::DeviceResources::c_backBufferCount] = {};
		D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView = {};
		
		BufferHeap<BoundingRendererParent::VSVertices> m_vertexBuffer[DX::DeviceResources::c_backBufferCount];
		BufferHeap<UINT> m_indexBuffer[DX::DeviceResources::c_backBufferCount];
		BufferHeap<XMMATRIX> m_instanceBuffer;

		UINT m_drawCount = 0;

		void FillVertices(const UINT& i, const DirectX::BoundingBox boundingBox);
		void FillIndices(const UINT& i);
	public:
		BoundingBoxRenderer();
		~BoundingBoxRenderer();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources, const UINT& maxInstances = 1) override;
		void Update(const XMMATRIX& modelTransform, CameraBase* camera, const std::vector<XMMATRIX>* instances = nullptr, const UINT& begin = 0, const UINT& end = 0) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;
	};
}
