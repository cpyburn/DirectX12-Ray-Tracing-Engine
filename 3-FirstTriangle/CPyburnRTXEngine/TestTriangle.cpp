#include "pchlib.h"
#include "TestTriangle.h"

namespace CPyburnRTXEngine
{
	void TestTriangle::createAccelerationStructures()
	{
        // create createTriangleVB
        const XMFLOAT3 vertices[] =
        {
            XMFLOAT3(0,          1,  0),
            XMFLOAT3(0.866f,  -0.5f, 0),
            XMFLOAT3(-0.866f, -0.5f, 0),
        };

        static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
        {
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0,
        };

        static const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
        {
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0
        };

        // create buffer
        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Alignment = 0;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.Height = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Width = sizeof(vertices);

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpVertexBuffer)));

        // For simplicity, we create the vertex buffer on the upload heap, but that's not required
        uint8_t* pData;
        mpVertexBuffer->Map(0, nullptr, (void**)&pData);
        memcpy(pData, vertices, sizeof(vertices));
        mpVertexBuffer->Unmap(0, nullptr);

        // Single-use command allocator and command list for creating resources.
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12GraphicsCommandList4> commandList;

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

        // Bottom Level AS
        {
            D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
            geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDesc.Triangles.VertexBuffer.StartAddress = mpVertexBuffer->GetGPUVirtualAddress();
            geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
            geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            geomDesc.Triangles.VertexCount = 3;
            geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = 1;
            inputs.pGeometryDescs = &geomDesc;
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
            m_deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bufDesc.Width = info.ScratchDataSizeInBytes;

            ComPtr<ID3D12Resource> pScratch;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pScratch)));

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            ComPtr<ID3D12Resource> pResult;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));

            // Create the bottom-level AS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
            asDesc.Inputs = inputs;
            asDesc.DestAccelerationStructureData = pResult->GetGPUVirtualAddress();
            asDesc.ScratchAccelerationStructureData = pScratch->GetGPUVirtualAddress();

            commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

            // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = pResult.Get();
            commandList->ResourceBarrier(1, &uavBarrier);

            // Close the resource creation command list and execute it to begin the vertex buffer copy into
            // the default heap.
            ThrowIfFailed(commandList->Close());
            ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
            m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            m_deviceResources->WaitForGpu();

            // Store the AS buffers. The rest of the buffers will be released once we exit the function
            mpBottomLevelAS = pResult;
        }

        ThrowIfFailed(commandAllocator->Reset());
        ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

        // Top Level AS
        {
            // First, get the size of the TLAS buffers and create them
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = 1;
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
            m_deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers
            bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bufDesc.Width = info.ScratchDataSizeInBytes;

            ComPtr<ID3D12Resource> pScratch;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pScratch)));

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            ComPtr<ID3D12Resource> pResult;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));
            mTlasSize = info.ResultDataMaxSizeInBytes;

            // The instance desc should be inside a buffer, create and map the buffer
            bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            bufDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

			ComPtr<ID3D12Resource> pInstanceDescResource;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pInstanceDescResource)));
            D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
            pInstanceDescResource->Map(0, nullptr, (void**)&pInstanceDesc);

            // Initialize the instance desc. We only have a single instance
            pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
            pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
            pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            XMMATRIX xmIdentity = XMMatrixIdentity(); // Identity matrix
            memcpy(pInstanceDesc->Transform, &xmIdentity, sizeof(pInstanceDesc->Transform));
            pInstanceDesc->AccelerationStructure = mpBottomLevelAS->GetGPUVirtualAddress();
            pInstanceDesc->InstanceMask = 0xFF;

            // Unmap
            pInstanceDescResource->Unmap(0, nullptr);

            // Create the TLAS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
            asDesc.Inputs = inputs;
            asDesc.Inputs.InstanceDescs = pInstanceDescResource->GetGPUVirtualAddress();
            asDesc.DestAccelerationStructureData = pResult->GetGPUVirtualAddress();
            asDesc.ScratchAccelerationStructureData = pScratch->GetGPUVirtualAddress();

            commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

            // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = pResult.Get();
            commandList->ResourceBarrier(1, &uavBarrier);

            // Close the resource creation command list and execute it to begin the vertex buffer copy into
            // the default heap.
            ThrowIfFailed(commandList->Close());
            ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
            m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            m_deviceResources->WaitForGpu();

            // Store the AS buffers. The rest of the buffers will be released once we exit the function
            mpTopLevelAS = pResult;
        }
	}

    TestTriangle::TestTriangle()
    {
    }

    TestTriangle::~TestTriangle()
    {
    }

    void TestTriangle::CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources)
	{
		m_deviceResources = deviceResources;
		createAccelerationStructures();
    }

    void TestTriangle::Release()
	{

	}
}
