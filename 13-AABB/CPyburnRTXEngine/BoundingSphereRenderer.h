#pragma once

#include "BoundingRendererParent.h"

namespace CPyburnRTXEngine
{
	class BoundingSphereRenderer : public BoundingSphere, public BoundingRendererParent
	{
	private:
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
		D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView[DX::DeviceResources::c_backBufferCount] = {};

		BufferHeap<BoundingRendererParent::VSVertices> m_vertexBuffer;
		BufferHeap<UINT> m_indexBuffer;
		BufferHeap<XMMATRIX> m_instanceBuffer[DX::DeviceResources::c_backBufferCount];
	public:
		BoundingSphereRenderer();
		~BoundingSphereRenderer();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources, const UINT& maxInstances = 1);
		void Update(const XMMATRIX& modelTransform, CameraBase* camera, const std::vector<XMMATRIX>* instances = nullptr, const UINT& begin = 0, const UINT& end = 0);
		void Render(ID3D12GraphicsCommandList* commandList) override;
	};
}

