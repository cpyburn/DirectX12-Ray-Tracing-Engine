#pragma once

#include "pchlib.h"

namespace CPyburnRTXEngine
{
    template<typename T>
    class BufferBlas
    {
    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_scratch;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_result;

        D3D12_RESOURCE_DESC m_bufDesc = {};
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_geomDescVector = {};
        D3D12_RAYTRACING_GEOMETRY_DESC m_geomDesc = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_asDesc = {};
        D3D12_RESOURCE_BARRIER m_uavBarrier = {};

    public:
        Microsoft::WRL::ComPtr<ID3D12Resource> GetResult() const { return m_result; }

        BufferBlas()
        {

        }

        ~BufferBlas()
        {
            Release();
        }

        void InitDynamicBlas(ID3D12Device5* m_d3dDevice, const UINT& count, const Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer, ID3D12GraphicsCommandList4* commandList, Microsoft::WRL::ComPtr<ID3D12Resource> indicesBuffer = nullptr, UINT indicesCount = 0)
        {
            m_bufDesc.Alignment = 0;
            m_bufDesc.DepthOrArraySize = 1;
            m_bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            m_bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            m_bufDesc.Format = DXGI_FORMAT_UNKNOWN;
            m_bufDesc.Height = 1;
            m_bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            m_bufDesc.MipLevels = 1;
            m_bufDesc.SampleDesc.Count = 1;
            m_bufDesc.SampleDesc.Quality = 0;

            m_geomDescVector.resize(1);

            m_geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            m_geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(T);
            m_geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            m_geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            m_geomDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
            m_geomDesc.Triangles.VertexCount = count;

            if (indicesBuffer)
            {
                m_geomDesc.Triangles.IndexBuffer = indicesBuffer->GetGPUVirtualAddress();
                m_geomDesc.Triangles.IndexCount = indicesCount;
                m_geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
            }

            m_geomDescVector[0] = m_geomDesc;

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
            inputs.NumDescs = static_cast<UINT>(m_geomDescVector.size());
            //inputs.pGeometryDescs = &geomDesc;
            inputs.pGeometryDescs = m_geomDescVector.data();
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
            m_d3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            m_bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            m_bufDesc.Width = info.ScratchDataSizeInBytes;

            auto heapPropertiesDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            if (!m_scratch.Get())
            {
                DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_scratch)));
                m_scratch->SetName(L"BLAS Scratch");
            }

            if (!m_result.Get())
            {
                m_bufDesc.Width = info.ResultDataMaxSizeInBytes;
                DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&m_result)));
                m_result->SetName(L"BLAS Result");
            }

            // Create the bottom-level AS
            
            m_asDesc.Inputs = inputs;
            m_asDesc.DestAccelerationStructureData = m_result->GetGPUVirtualAddress();
            m_asDesc.ScratchAccelerationStructureData = m_scratch->GetGPUVirtualAddress();

            m_uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            m_uavBarrier.UAV.pResource = m_result.Get();
        }
        
        void UpdateDynamicBlas(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList)
        {
            commandList->BuildRaytracingAccelerationStructure(&m_asDesc, 0, nullptr);

            // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
            commandList->ResourceBarrier(1, &m_uavBarrier);
        }

        static Microsoft::WRL::ComPtr<ID3D12Resource> CreateStaticBlas(DX::DeviceResources* deviceResources, const UINT& count, const Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer, ID3D12GraphicsCommandList4* commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator, Microsoft::WRL::ComPtr<ID3D12Resource> indicesBuffer = nullptr, UINT indicesCount = 0)
        {
            D3D12_RESOURCE_DESC m_bufDesc = {};
            m_bufDesc.Alignment = 0;
            m_bufDesc.DepthOrArraySize = 1;
            m_bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            m_bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            m_bufDesc.Format = DXGI_FORMAT_UNKNOWN;
            m_bufDesc.Height = 1;
            m_bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            m_bufDesc.MipLevels = 1;
            m_bufDesc.SampleDesc.Count = 1;
            m_bufDesc.SampleDesc.Quality = 0;

            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_geomDescVector;
            m_geomDescVector.resize(1);

            D3D12_RAYTRACING_GEOMETRY_DESC m_geomDesc = {};
            m_geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            m_geomDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
            m_geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(T);
            m_geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            m_geomDesc.Triangles.VertexCount = count;
            m_geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            if (indicesBuffer)
            {
                m_geomDesc.Triangles.IndexBuffer = indicesBuffer->GetGPUVirtualAddress();
                m_geomDesc.Triangles.IndexCount = indicesCount;
                m_geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
            }

            m_geomDescVector[0] = m_geomDesc;

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = static_cast<UINT>(m_geomDescVector.size());
            //inputs.pGeometryDescs = &geomDesc;
            inputs.pGeometryDescs = m_geomDescVector.data();
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
            deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            m_bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            m_bufDesc.Width = info.ScratchDataSizeInBytes;

            Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
            auto heapPropertiesDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pScratch)));
            pScratch->SetName(L"BLAS Scratch");

            m_bufDesc.Width = info.ResultDataMaxSizeInBytes;
            Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));
            pResult->SetName(L"BLAS Result");

            // Create the bottom-level AS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_asDesc = {};
            m_asDesc.Inputs = inputs;
            m_asDesc.DestAccelerationStructureData = pResult->GetGPUVirtualAddress();
            m_asDesc.ScratchAccelerationStructureData = pScratch->GetGPUVirtualAddress();

            commandList->BuildRaytracingAccelerationStructure(&m_asDesc, 0, nullptr);

            // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
            D3D12_RESOURCE_BARRIER m_uavBarrier = {};
            m_uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            m_uavBarrier.UAV.pResource = pResult.Get();
            commandList->ResourceBarrier(1, &m_uavBarrier);

            // Close the resource creation command list and execute it to begin the vertex buffer copy into
            // the default heap.
            DX::ThrowIfFailed(commandList->Close());
            ID3D12CommandList* ppCommandLists[] = { commandList };
            deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            deviceResources->WaitForGpu();

            DX::ThrowIfFailed(commandAllocator->Reset());
            DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

            // Store the AS buffers. The rest of the buffers will be released once we exit the function
            return pResult;
        }

        void Release()
        {

        }
    };
}
