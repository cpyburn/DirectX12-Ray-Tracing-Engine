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
			XMFLOAT4 color;
		};

		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

		BufferHeap<BoundingSphereRenderer::VSVertices> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferUpload;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferUpload;

	public:
		BoundingSphereRenderer();
		~BoundingSphereRenderer();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
	};
}

