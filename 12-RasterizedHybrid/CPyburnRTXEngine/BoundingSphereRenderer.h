#pragma once

#include "BufferHeap.h"

namespace CPyburnRTXEngine
{
	class BoundingSphereRenderer : public BoundingSphere
	{
	private:
		struct VSVertices
		{
			XMFLOAT3 position;
			XMFLOAT4 color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		};

		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

		BufferHeap<BoundingSphereRenderer::VSVertices> m_vertexBuffer;
		BufferHeap<UINT> m_indexBuffer;
	public:
		BoundingSphereRenderer();
		~BoundingSphereRenderer();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
	};
}

