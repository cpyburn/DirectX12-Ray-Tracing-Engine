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

#pragma region Vertex Buffer
		m_vertexBuffer.CreateDeviceDependentResources(deviceResources->GetD3DDevice());
		std::vector<VSVertices> vertices;
		vertices.resize(meshData.Vertices.size());
		for (size_t i = 0; i < vertices.size(); i++)
		{
			const ShapeRendererHelper::Vertex& vertex = meshData.Vertices[i];
			vertices[i].position = vertex.Position;
		}
		m_vertexBuffer.CpuData = vertices;
		m_vertexBuffer.CreateOnDefaultHeap(commandList.Get(), L"Bounding Sphere Vertex Buffer");
#pragma endregion

#pragma region Index Buffer
		m_indexBuffer.CreateDeviceDependentResources(deviceResources->GetD3DDevice());
		m_indexBuffer.CpuData = meshData.Indices32;
		m_indexBuffer.CreateOnDefaultHeap(commandList.Get(), L"Bounding Sphere Index Buffer");
#pragma endregion

#pragma region Instance Buffer
		// as of right now, the instances should be relatively small < 10000, so use upload heap for updating per frame. Can always benchmark if performance is issue and decide to use default
		for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
		{
			m_instanceBuffer->CreateDeviceDependentResources(deviceResources->GetD3DDevice());
			m_instanceBuffer->ReserveMemory(1000);
			m_instanceBuffer->CreateOnUploadHeap(L"Bounding Sphere Instance Buffer");
		}
#pragma endregion

		DX::ThrowIfFailed(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		deviceResources->WaitForGpu();

		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

		m_vertexBuffer.ReleaseUploadResource();
		m_indexBuffer.ReleaseUploadResource();
	}
}