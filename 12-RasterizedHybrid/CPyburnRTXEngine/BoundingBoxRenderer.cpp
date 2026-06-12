#include "pchlib.h"
#include "BoundingBoxRenderer.h"

namespace CPyburnRTXEngine
{
	BoundingBoxRenderer::BoundingBoxRenderer() : BoundingBox(), BoundingRendererParent()
	{
	
	}

	BoundingBoxRenderer::~BoundingBoxRenderer()
	{
	
	}

	void BoundingBoxRenderer::CreateDeviceDependentResources(DX::DeviceResources* deviceResources, const UINT& maxInstances)
	{}

	void BoundingBoxRenderer::Update(const XMMATRIX & modelTransform, CameraBase * camera, const std::vector<XMMATRIX>*instances, const UINT & begin, const UINT & end)
	{}

	void BoundingBoxRenderer::Render(ID3D12GraphicsCommandList * commandList)
	{}


}
