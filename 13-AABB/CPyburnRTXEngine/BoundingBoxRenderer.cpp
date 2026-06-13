#include "pchlib.h"
#include "BoundingBoxRenderer.h"

namespace CPyburnRTXEngine
{
	void BoundingBoxRenderer::FillVertices(const UINT& i, const DirectX::BoundingBox boundingBox)
	{
		XMVECTOR bbMin = XMVectorSubtract(XMLoadFloat3(&boundingBox.Center), XMLoadFloat3(&boundingBox.Extents));
		XMVECTOR bbMax = XMVectorAdd(XMLoadFloat3(&boundingBox.Center), XMLoadFloat3(&boundingBox.Extents));

		float minX = XMVectorGetX(bbMin);
		float minY = XMVectorGetY(bbMin);
		float minZ = XMVectorGetZ(bbMin);

		float maxX = XMVectorGetX(bbMax);
		float maxY = XMVectorGetY(bbMax);
		float maxZ = XMVectorGetZ(bbMax);

		// note that [0] is directly diagonal from [6]

		// 0
		m_vertexBuffer->CpuData[i * 8 + 0].position = XMFLOAT3(minX, maxY, maxZ);
		m_vertexBuffer->CpuData[i * 8 + 0].color = m_lineColor;
		// 1
		m_vertexBuffer->CpuData[i * 8 + 1].position = XMFLOAT3(maxX, maxY, maxZ);
		m_vertexBuffer->CpuData[i * 8 + 1].color = m_lineColor;
		// 2
		m_vertexBuffer->CpuData[i * 8 + 2].position = XMFLOAT3(maxX, minY, maxZ);
		m_vertexBuffer->CpuData[i * 8 + 2].color = m_lineColor;
		// 3
		m_vertexBuffer->CpuData[i * 8 + 3].position = XMFLOAT3(minX, minY, maxZ);
		m_vertexBuffer->CpuData[i * 8 + 3].color = m_lineColor;
		// 4
		m_vertexBuffer->CpuData[i * 8 + 4].position = XMFLOAT3(minX, maxY, minZ);
		m_vertexBuffer->CpuData[i * 8 + 4].color = m_lineColor;
		// 5
		m_vertexBuffer->CpuData[i * 8 + 5].position = XMFLOAT3(maxX, maxY, minZ);
		m_vertexBuffer->CpuData[i * 8 + 5].color = m_lineColor;
		// 6
		m_vertexBuffer->CpuData[i * 8 + 6].position = XMFLOAT3(maxX, minY, minZ);
		m_vertexBuffer->CpuData[i * 8 + 6].color = m_lineColor;
		// 7
		m_vertexBuffer->CpuData[i * 8 + 7].position = XMFLOAT3(minX, minY, minZ);
		m_vertexBuffer->CpuData[i * 8 + 7].color = m_lineColor;
	}

	void BoundingBoxRenderer::FillIndices(const UINT& i)
	{
		m_indexBuffer->CpuData[i * 24 + 0] = i * 24 + 0;
		m_indexBuffer->CpuData[i * 24 + 1] = i * 24 + 1;
		m_indexBuffer->CpuData[i * 24 + 2] = i * 24 + 1;
		m_indexBuffer->CpuData[i * 24 + 3] = i * 24 + 2;
		m_indexBuffer->CpuData[i * 24 + 4] = i * 24 + 2;
		m_indexBuffer->CpuData[i * 24 + 5] = i * 24 + 3;
		m_indexBuffer->CpuData[i * 24 + 6] = i * 24 + 3;
		m_indexBuffer->CpuData[i * 24 + 7] = i * 24 + 0;

		m_indexBuffer->CpuData[i * 24 + 8] = i * 24 + 4;
		m_indexBuffer->CpuData[i * 24 + 9] = i * 24 + 5;
		m_indexBuffer->CpuData[i * 24 + 10] = i * 24 + 5;
		m_indexBuffer->CpuData[i * 24 + 11] = i * 24 + 6;
		m_indexBuffer->CpuData[i * 24 + 12] = i * 24 + 6;
		m_indexBuffer->CpuData[i * 24 + 13] = i * 24 + 7;
		m_indexBuffer->CpuData[i * 24 + 14] = i * 24 + 7;
		m_indexBuffer->CpuData[i * 24 + 15] = i * 24 + 4;

		m_indexBuffer->CpuData[i * 24 + 16] = i * 24 + 0;
		m_indexBuffer->CpuData[i * 24 + 17] = i * 24 + 4;
		m_indexBuffer->CpuData[i * 24 + 18] = i * 24 + 1;
		m_indexBuffer->CpuData[i * 24 + 19] = i * 24 + 5;
		m_indexBuffer->CpuData[i * 24 + 20] = i * 24 + 2;
		m_indexBuffer->CpuData[i * 24 + 21] = i * 24 + 6;
		m_indexBuffer->CpuData[i * 24 + 22] = i * 24 + 3;
		m_indexBuffer->CpuData[i * 24 + 23] = i * 24 + 7;

		//std::vector<UINT> indices =
		//{
		//	0, 
		//	1,
		//	1, 
		//	2,
		//	2, 
		//	3,
		//	3, 
		//	0,

		//	4, 
		//	5,
		//	5, 
		//	6,
		//	6, 
		//	7,
		//	7, 
		//	4,

		//	0, 
		//	4,
		//	1, 
		//	5,
		//	2, 
		//	6,
		//	3, 
		//	7
		//};
	}

	BoundingBoxRenderer::BoundingBoxRenderer() : BoundingBox(), BoundingRendererParent()
	{
	
	}

	BoundingBoxRenderer::~BoundingBoxRenderer()
	{
	
	}

	void BoundingBoxRenderer::CreateDeviceDependentResources(DX::DeviceResources* deviceResources, const UINT& maxInstances)
	{
		m_deviceResources = deviceResources;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
		DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

		// Create the vertices.
		std::vector<VSVertices> vertices(8 * maxInstances);
		// Create the incdices
		std::vector<UINT> indices(24 * maxInstances);

		for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
		{
			// vertex buffer
			{
				m_vertexBuffer[i].CreateDeviceDependentResources(deviceResources->GetD3DDevice());
				m_vertexBuffer[i].CpuData = vertices;
				m_vertexBuffer[i].CreateOnUploadHeap(L"Bounding Box Vertex Buffer");
				m_vertexBuffer[i].CopyCpuDataToUploadHeap();

				m_vertexBufferView[i].BufferLocation = m_vertexBuffer[i].UploadHeapResource->GetGPUVirtualAddress();
				m_vertexBufferView[i].StrideInBytes = sizeof(VSVertices);
				m_vertexBufferView[i].SizeInBytes = m_vertexBuffer[i].BufferSize;
			}

			// index buffer
			{
				m_indexBuffer[i].CreateDeviceDependentResources(deviceResources->GetD3DDevice());
				m_indexBuffer[i].CpuData = indices;
				m_indexBuffer[i].CreateOnUploadHeap(L"Bounding Box Index Buffer");
				m_indexBuffer[i].CopyCpuDataToUploadHeap();

				m_indexBufferView[i].BufferLocation = m_indexBuffer[i].UploadHeapResource->GetGPUVirtualAddress();
				m_indexBufferView[i].SizeInBytes = m_indexBuffer[i].BufferSize;
				m_indexBufferView[i].Format = DXGI_FORMAT_R32_UINT;
			}
		}

		// create the instances
		std::vector<XMMATRIX> instances(1);
		{
			m_instanceBuffer.CreateDeviceDependentResources(deviceResources->GetD3DDevice());
			m_instanceBuffer.CpuData = instances;
			m_instanceBuffer.CpuData[0] = XMMatrixIdentity();

			m_instanceBuffer.CreateOnDefaultHeap(commandList.Get(), L"Bounding Box Instance Buffer");

			m_instanceBufferView.BufferLocation = m_instanceBuffer.DefaultHeapResource->GetGPUVirtualAddress();
			m_instanceBufferView.StrideInBytes = sizeof(XMFLOAT4X4);
			m_instanceBufferView.SizeInBytes = m_instanceBuffer.BufferSize;
		}

		DX::ThrowIfFailed(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		deviceResources->WaitForGpu();

		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
	}

	void BoundingBoxRenderer::Update(const XMMATRIX& modelTransform, CameraBase* camera, const std::vector<XMMATRIX>* instances, const UINT& begin, const UINT& end)
	{
		if (instances)
		{
			m_draw = false;
			std::vector<DirectX::BoundingBox> visibleBoundingBoxes;
			for (UINT i = begin; i < end; i++)
			{
				// most of the time this will be a simple single instance
				DirectX::BoundingBox worldBox;
				XMMATRIX instance = instances->at(i);
				this->Transform(worldBox, instance);
				if (worldBox.Intersects(camera->GetBoundingFrustum()))
				{
					m_draw = true;
					visibleBoundingBoxes.push_back(worldBox);
				}
			}

			size_t capacity = m_vertexBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData.capacity() / 8;
			size_t count = visibleBoundingBoxes.size();

			if (count <= capacity)
			{
				// count = size
				m_vertexBuffer->CpuData.resize(count * 8);
				m_indexBuffer->CpuData.resize(count * 24);
				for (size_t i = 0; i < count; i++)
				{
					FillVertices(i, visibleBoundingBoxes[i]);
					m_vertexBuffer->CopyCpuDataToUploadHeap();
					FillIndices(i);
					m_indexBuffer->CopyCpuDataToUploadHeap();
				}
			}
			else if (count > capacity)
			{
				// capacity = size
				m_vertexBuffer->CpuData.resize(capacity * 8);
				m_indexBuffer->CpuData.resize(capacity * 24);
				for (size_t i = 0; i < capacity; i++)
				{
					FillVertices(i, visibleBoundingBoxes[i]);
					m_vertexBuffer->CopyCpuDataToUploadHeap();
					FillIndices(i);
					m_indexBuffer->CopyCpuDataToUploadHeap();
				}
			}

			return;
		}
		else
		{
			// most of the time this will be a simple single instance
			//DirectX::BoundingBox worldBox;
			//this->Transform(worldBox, modelTransform);
			//m_draw = this->Intersects(camera->GetBoundingFrustum());

			//m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData[0] = modelTransform;
			//m_instanceBuffer[m_deviceResources->GetCurrentFrameIndex()].CopyCpuDataToUploadHeap();
		}
	}

	void BoundingBoxRenderer::Render(ID3D12GraphicsCommandList * commandList)
	{
		if (!m_draw)
			return;

		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[2];
		vertexBufferViews[0] = m_vertexBufferView[m_deviceResources->GetCurrentFrameIndex()];
		vertexBufferViews[1] = m_instanceBufferView;

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), &vertexBufferViews[0]);
		commandList->IASetIndexBuffer(&m_indexBufferView[m_deviceResources->GetCurrentFrameIndex()]);
		commandList->DrawIndexedInstanced(static_cast<UINT>(m_indexBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData.size()), 1, 0, 0, 0);
	}
}
