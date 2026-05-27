#pragma once

#include "pchlib.h"

namespace CPyburnRTXEngine
{
    class BufferBlas
    {
    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_scratch;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_result;

    public:
        Microsoft::WRL::ComPtr<ID3D12Resource> GetResult() const { return m_result; }

        BufferBlas()
        {

        }

        ~BufferBlas()
        {
            Release();
        }

        template<typename T>
        void UpdateBlas(const std::shared_ptr<DX::DeviceResources>& deviceResources, const UINT& count, const Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12Resource> indicesBuffer = nullptr, UINT indicesCount = 0)
        {
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

            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescVector;
            geomDescVector.resize(1);

            D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
            geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
            geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(T);
            geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            geomDesc.Triangles.VertexCount = count;
            geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            if (indicesBuffer)
            {
                geomDesc.Triangles.IndexBuffer = indicesBuffer->GetGPUVirtualAddress();
                geomDesc.Triangles.IndexCount = indicesCount;
                geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
            }

            geomDescVector[0] = geomDesc;

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
            inputs.NumDescs = static_cast<UINT>(geomDescVector.size());
            //inputs.pGeometryDescs = &geomDesc;
            inputs.pGeometryDescs = geomDescVector.data();
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
            deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bufDesc.Width = info.ScratchDataSizeInBytes;
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_scratch)));
            m_scratch->SetName(L"BLAS Scratch");

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&m_result)));
            m_result->SetName(L"BLAS Result");

            // Create the bottom-level AS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
            asDesc.Inputs = inputs;
            asDesc.DestAccelerationStructureData = m_result->GetGPUVirtualAddress();
            asDesc.ScratchAccelerationStructureData = m_scratch->GetGPUVirtualAddress();

            commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

            // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = m_result.Get();
            commandList->ResourceBarrier(1, &uavBarrier);
        }

        template<typename T>
        static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBlas(const std::shared_ptr<DX::DeviceResources>& deviceResources, const UINT& count, const Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator, Microsoft::WRL::ComPtr<ID3D12Resource> indicesBuffer = nullptr, UINT indicesCount = 0)
        {
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

            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescVector;
            geomDescVector.resize(1);

            D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
            geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress(); 
            geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(T);
            geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            geomDesc.Triangles.VertexCount = count; 
            geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            if (indicesBuffer)
            {
                geomDesc.Triangles.IndexBuffer = indicesBuffer->GetGPUVirtualAddress();
                geomDesc.Triangles.IndexCount = indicesCount;
                geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
            }

            geomDescVector[0] = geomDesc; 

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = static_cast<UINT>(geomDescVector.size());
            //inputs.pGeometryDescs = &geomDesc;
            inputs.pGeometryDescs = geomDescVector.data();
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
            deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bufDesc.Width = info.ScratchDataSizeInBytes;

            Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pScratch)));
            pScratch->SetName(L"BLAS Scratch");

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));
            pResult->SetName(L"BLAS Result");

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
            DX::ThrowIfFailed(commandList->Close());
            ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
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
