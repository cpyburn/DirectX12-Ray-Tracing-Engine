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

		bool m_draw = true;

		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
		D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView[DX::DeviceResources::c_backBufferCount] = {};

		BufferHeap<BoundingSphereRenderer::VSVertices> m_vertexBuffer;
		BufferHeap<UINT> m_indexBuffer;
		BufferHeap<XMMATRIX> m_instanceBuffer[DX::DeviceResources::c_backBufferCount];
		DX::DeviceResources* m_deviceResources = nullptr;
	public:
		BoundingSphereRenderer();
		~BoundingSphereRenderer();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);

		void Update(const XMMATRIX& modelTransform, CameraBase* camera);
		void Render(ID3D12GraphicsCommandList* commandList, CameraBase* camera);
	};
}

