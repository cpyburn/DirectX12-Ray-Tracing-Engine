#include "pchlib.h"
#include "BoundingSphereRenderer.h"

#include "ShapeRendererHelper.h"

namespace CPyburnRTXEngine
{
	BoundingSphereRenderer::BoundingSphereRenderer() : BoundingSphere(), BoundingRendererParent()
	{

	}

	BoundingSphereRenderer::~BoundingSphereRenderer()
	{

	}

	void BoundingSphereRenderer::CreateDeviceDependentResources(DX::DeviceResources* deviceResources, const UINT& maxInstances)
	{
		m_deviceResources = deviceResources;
		ShapeRendererHelper::MeshData meshData = ShapeRendererHelper::CreateSphere(Center, Radius, 10, 10);

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
		DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

		// vertex buffer
		{
			m_vertexBuffer.CreateDeviceDependentResources(deviceResources->GetD3DDevice());
			std::vector<VSVertices> vertices(meshData.Vertices.size());
			for (size_t i = 0; i < vertices.size(); i++)
			{
				const ShapeRendererHelper::Vertex& vertex = meshData.Vertices[i];
				vertices[i].position = vertex.Position;
			}
			m_vertexBuffer.CpuData = vertices;
			m_vertexBuffer.CreateOnDefaultHeap(commandList.Get(), L"Bounding Sphere Vertex Buffer");

			m_vertexBufferView.BufferLocation = m_vertexBuffer.DefaultHeapResource->GetGPUVirtualAddress();
			m_vertexBufferView.StrideInBytes = sizeof(VSVertices);
			m_vertexBufferView.SizeInBytes = m_vertexBuffer.BufferSize;
		}

		// index buffer
		{
			m_indexBuffer.CreateDeviceDependentResources(deviceResources->GetD3DDevice());
			m_indexBuffer.CpuData = meshData.Indices32;
			m_indexBuffer.CreateOnDefaultHeap(commandList.Get(), L"Bounding Sphere Index Buffer");

			m_indexBufferView.BufferLocation = m_indexBuffer.DefaultHeapResource->GetGPUVirtualAddress();
			m_indexBufferView.SizeInBytes = m_indexBuffer.BufferSize;
			m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		}

		// instance buffer (world0 in shader) not a Cbv
		{
			// as of right now, the instances should be relatively small < 10000, so use upload heap for updating per frame. Can always benchmark if performance is issue and decide to use default
			for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
			{
				m_instanceBuffer[i].CreateDeviceDependentResources(deviceResources->GetD3DDevice());
				m_instanceBuffer[i].CpuData.resize(maxInstances);

				XMMATRIX transform = XMMatrixIdentity();
				for (size_t x = 0; x < maxInstances; x++)
				{
					m_instanceBuffer[i].CpuData[x] = transform; // testing
				}
				
				m_instanceBuffer[i].CreateOnUploadHeap(L"Bounding Sphere Instance Buffer");
				m_instanceBuffer[i].CopyCpuDataToUploadHeap();

				m_instanceBufferView[i].BufferLocation = m_instanceBuffer[i].UploadHeapResource->GetGPUVirtualAddress();
				m_instanceBufferView[i].StrideInBytes = sizeof(XMFLOAT4X4);
				m_instanceBufferView[i].SizeInBytes = m_instanceBuffer[i].BufferSize;
			}
		}

		DX::ThrowIfFailed(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		deviceResources->WaitForGpu();

		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

		m_vertexBuffer.ReleaseUploadResource();
		m_indexBuffer.ReleaseUploadResource();
	}

	void BoundingSphereRenderer::Update(const XMMATRIX& modelTransform, CameraBase* camera, const std::vector<XMMATRIX>* instances, const UINT& begin, const UINT& end)
	{
		if (instances)
		{
			m_draw = false;
			std::vector<XMMATRIX> visibleInstances;
			for (UINT i = begin; i < end; i++)
			{
				// most of the time this will be a simple single instance
				DirectX::BoundingSphere worldSphere;
				XMMATRIX instance = instances->at(i);
				this->Transform(worldSphere, instance);
				if (worldSphere.Intersects(camera->GetBoundingFrustum()))
				{
					m_draw = true;
					visibleInstances.push_back(instance);
				}
			}

			size_t capacity = m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData.capacity();
			size_t count = visibleInstances.size();

			if (count <= capacity)
			{
				std::vector<XMMATRIX>& cpuData = m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData;
				cpuData.resize(count);
				std::copy(visibleInstances.begin() + begin, visibleInstances.end(), cpuData.begin());
				m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CopyCpuDataToUploadHeap();
			}
			else if (count > capacity)
			{
				std::vector<XMMATRIX>& cpuData = m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData;
				cpuData.resize(capacity);
				std::copy(visibleInstances.begin() + begin, visibleInstances.begin() + begin + capacity, cpuData.begin());
				m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CopyCpuDataToUploadHeap();
			}

			return;
		}
		else
		{
			// most of the time this will be a simple single instance
			DirectX::BoundingSphere worldSphere;
			this->Transform(worldSphere, modelTransform);
			m_draw = this->Intersects(camera->GetBoundingFrustum());

			m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData[0] = modelTransform;
			m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CopyCpuDataToUploadHeap();
		}
	}

	void BoundingSphereRenderer::Render(ID3D12GraphicsCommandList* commandList, CameraBase* camera)
	{
		if (!m_draw)
			return;

		commandList->SetGraphicsRootSignature(GraphicsContexts::GetRootSignaturePositionColorInstanced());
		commandList->SetPipelineState(GraphicsContexts::GetPipelinePositionColorInstancedLine()); 
		commandList->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		commandList->SetGraphicsRootConstantBufferView(0, camera->GetCbv()->GetGPUVirtualAddressBuffered(camera->GetDeviceResources()->GetCurrentFrameIndex()));

		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[2];
		vertexBufferViews[0] = m_vertexBufferView;
		vertexBufferViews[1] = m_instanceBufferView[camera->GetDeviceResources()->GetCurrentFrameIndex()];

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), &vertexBufferViews[0]);
		commandList->IASetIndexBuffer(&m_indexBufferView);
		commandList->DrawIndexedInstanced(static_cast<UINT>(m_indexBuffer.CpuData.size()), static_cast<UINT>(m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData.size()), 0, 0, 0);
	}
}