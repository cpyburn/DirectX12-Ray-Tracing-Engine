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

		XMFLOAT4 m_lineColor = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		bool m_draw = true;
		
		DX::DeviceResources* m_deviceResources = nullptr;

	public:
		void SetColor(const XMVECTOR& color) { XMStoreFloat4(&m_lineColor, color); }
		static void RenderBegin(ID3D12GraphicsCommandList* commandList, CameraBase* camera);
		virtual void Render(ID3D12GraphicsCommandList* commandList) = 0;
	};
}
