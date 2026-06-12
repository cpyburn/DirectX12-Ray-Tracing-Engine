#include "pchlib.h"
#include "BoundingRendererParent.h"

namespace CPyburnRTXEngine
{
	void BoundingRendererParent::RenderBegin(ID3D12GraphicsCommandList* commandList, CameraBase* camera)
	{
		commandList->SetGraphicsRootSignature(GraphicsContexts::GetRootSignaturePositionColorInstanced());
		commandList->SetPipelineState(GraphicsContexts::GetPipelinePositionColorInstancedLine());
		commandList->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		commandList->SetGraphicsRootConstantBufferView(0, camera->GetCbv()->GetGPUVirtualAddressBuffered(camera->GetDeviceResources()->GetCurrentFrameIndex()));
	}
}