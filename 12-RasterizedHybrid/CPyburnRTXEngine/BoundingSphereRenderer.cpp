#include "pchlib.h"
#include "BoundingSphereRenderer.h"

#include "ShapeRendererHelper.h"

namespace CPyburnRTXEngine
{
	BoundingSphereRenderer::BoundingSphereRenderer() : BoundingSphere()
	{

	}

	BoundingSphereRenderer::~BoundingSphereRenderer()
	{

	}

	void BoundingSphereRenderer::CreateDeviceDependentResources(DX::DeviceResources* deviceResources)
	{
		ShapeRendererHelper::MeshData meshData = ShapeRendererHelper::CreateSphere(Center, Radius, 10, 10);

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
		DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));


		DX::ThrowIfFailed(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		deviceResources->WaitForGpu();

		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
	}
}