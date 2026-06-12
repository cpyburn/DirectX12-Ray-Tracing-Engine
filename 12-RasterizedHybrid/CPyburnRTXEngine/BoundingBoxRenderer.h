#pragma once

#include "BoundingRendererParent.h"

namespace CPyburnRTXEngine
{
	class BoundingBoxRenderer: public BoundingBox, public BoundingRendererParent
	{
		BoundingBoxRenderer();
		~BoundingBoxRenderer();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources, const UINT& maxInstances = 1) override;
		void Update(const XMMATRIX& modelTransform, CameraBase* camera, const std::vector<XMMATRIX>* instances = nullptr, const UINT& begin = 0, const UINT& end = 0) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;
	};
}
