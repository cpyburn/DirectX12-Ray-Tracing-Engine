#pragma once

#include "BufferHeap.h"

namespace CPyburnRTXEngine
{
	class BoundingRendererParent
	{
	protected:
		struct VSVertices
		{
			XMFLOAT3 position;
			XMFLOAT4 color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		};

		bool m_draw = true;

		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
		D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView[DX::DeviceResources::c_backBufferCount] = {};

		BufferHeap<BoundingRendererParent::VSVertices> m_vertexBuffer;
		BufferHeap<UINT> m_indexBuffer;
		BufferHeap<XMMATRIX> m_instanceBuffer[DX::DeviceResources::c_backBufferCount];
		DX::DeviceResources* m_deviceResources = nullptr;
	};
}
