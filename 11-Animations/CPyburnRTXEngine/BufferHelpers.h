#pragma once

#include "pchlib.h"

namespace CPyburnRTXEngine
{
    template<typename T>
    class BufferHelpers
    {
    private:

    public:
        BufferHelpers()
        {

        }

        ~BufferHelpers()
        {
            Release();
        }

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

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));

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

        static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferOnDefaultHeap(const std::shared_ptr<DX::DeviceResources>& deviceResources, const UINT& count, const T* data, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator, const WCHAR* name = L"Buffer not named")
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> dafaultBuffer;

            const UINT bufferSizeModel = static_cast<UINT>(sizeof(T) * count);
            CD3DX12_RESOURCE_DESC bufferDescModel = CD3DX12_RESOURCE_DESC::Buffer(bufferSizeModel);
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &bufferDescModel,
                D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&dafaultBuffer)));

            Microsoft::WRL::ComPtr<ID3D12Resource> dafaultBufferUpload; // dont need upload buffer after uploading the data to the default heap, so we can keep it as a local variable
            DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &bufferDescModel,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&dafaultBufferUpload)));

            dafaultBuffer->SetName(name);

            // Upload the index buffer to the GPU.
            {
                D3D12_SUBRESOURCE_DATA verticeData = {};
                verticeData.pData = data;
                verticeData.RowPitch = 0;
                verticeData.SlicePitch = 0;

                CD3DX12_RESOURCE_BARRIER indexBufferResourceBarrier =
                    CD3DX12_RESOURCE_BARRIER::Transition(dafaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                commandList->ResourceBarrier(1, &indexBufferResourceBarrier);

                UpdateSubresources(commandList.Get(), dafaultBuffer.Get(), dafaultBufferUpload.Get(), 0, 0, 1, &verticeData);

                indexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(dafaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
                commandList->ResourceBarrier(1, &indexBufferResourceBarrier);
            }

            // upload goes out of scope if we don't execute the command list and wait for the GPU to finish before exiting the function, so execute and wait here
            DX::ThrowIfFailed(commandList->Close());
            ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
            deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            deviceResources->WaitForGpu();

            DX::ThrowIfFailed(commandAllocator->Reset());
            DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

            return dafaultBuffer;
        }

        void Release()
        {

        }
    };
}
