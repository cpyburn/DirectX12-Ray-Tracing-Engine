#pragma once

#include "pchlib.h"
#include "d3dx12.h"
#include "GraphicsContexts.h"
#include "DeviceResources.h"

namespace CPyburnRTXEngine
{
    template<typename T>
    class BufferHeapHelper
    {
    private:
        std::shared_ptr<DX::DeviceResources> m_deviceResources;
    public:
        // Per-frame descriptor heap positions
        UINT HeapIndex = MAXUINT;

        void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
        {
            m_deviceResources = deviceResources;

            // make sure descriptor heap is allocated
            if (HeapIndex == MAXUINT)
            {
                HeapIndex = GraphicsContexts::GetAvailableHeapPosition();
                CpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GraphicsContexts::GetCpuHandle(HeapIndex));
                GpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::GetGpuHandle(HeapIndex));
            }
        }

        BufferHeapHelper()
        {
            
        }

        ~BufferHeapHelper()
        {
            Release();
        }

        // CPU-side data (what you write to)
        std::vector<T> CpuData{};

        // D3D12 requires 256-byte alignment for CBVs
        static constexpr UINT AlignedSize = (sizeof(T) + 255u) & ~255u;

        CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle;
        CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle;

        // Upload heap resource
        Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeapResource = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> DefaultHeapResource = nullptr;

        // Persistently mapped pointer
        uint8_t* MappedData = nullptr;

        void CreateOnUploadHeap(const WCHAR* name = L"Upload buffer not named")
        {
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(AlignedSize * DX::DeviceResources::c_backBufferCount),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&UploadHeapResource)));

            UploadHeapResource->SetName(name);
        }

        void CreateOnDefaultHeap(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator, const WCHAR* name = L"Dfault buffer not named")
        {
            const UINT bufferSizeModel = static_cast<UINT>(sizeof(T) * CpuData.size());
            CD3DX12_RESOURCE_DESC bufferDescModel = CD3DX12_RESOURCE_DESC::Buffer(bufferSizeModel);
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &bufferDescModel,
                D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&DefaultHeapResource)));
            DefaultHeapResource->SetName(name);

            CreateOnUploadHeap(L"Upload " + name);

            CopyUploadToDefault(commandList, commandAllocator);
        }

        void CopyToUpload()
        {
            memcpy(MappedData + AlignedSize, &CpuData[0], sizeof(T));
        }

        void CopyUploadToDefault(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator)
        {
            // Upload the buffer to the GPU.
            {
                D3D12_SUBRESOURCE_DATA resourceData = {};
                resourceData.pData = &CpuData[0];
                resourceData.RowPitch = sizeof(T);
                resourceData.SlicePitch = CpuData.size();

                CD3DX12_RESOURCE_BARRIER indexBufferResourceBarrier =
                    CD3DX12_RESOURCE_BARRIER::Transition(DefaultHeapResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                commandList->ResourceBarrier(1, &indexBufferResourceBarrier);

                UpdateSubresources(commandList.Get(), DefaultHeapResource.Get(), UploadHeapResource.Get(), 0, 0, 1, &resourceData);

                indexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(DefaultHeapResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
                commandList->ResourceBarrier(1, &indexBufferResourceBarrier);
            }
        }

        void CreateConstantBufferView()
        {
            // Create constant buffer views to access the upload buffer.
            D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = UploadHeapResource->GetGPUVirtualAddress();

            // Describe and create constant buffer views.
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = cbvGpuAddress;
            cbvDesc.SizeInBytes = AlignedSize;
            m_deviceResources->GetD3DDevice()->CreateConstantBufferView(&cbvDesc, CpuHandle);

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            DX::ThrowIfFailed(UploadHeapResource->Map(0, &readRange, reinterpret_cast<void**>(&MappedData)));
        }

        void CreateShaderResourceView()
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.StructureByteStride = sizeof(T); // your vertex struct size goes here
            srvDesc.Buffer.NumElements = static_cast<UINT>(CpuData.size()); // number of vertices go here

            m_deviceResources->GetD3DDevice()->CreateShaderResourceView(DefaultHeapResource.Get(), &srvDesc, CpuHandle);
        }

        void Release()
        {
            // if heap is already been assigned
            if (HeapIndex != MAXUINT)
            {
                GraphicsContexts::RemoveHeapPosition(HeapIndex);
            }

            if (UploadHeapResource)
            {
                UploadHeapResource->Unmap(0, nullptr); // reset should and probably does unmap, but we conver our tracks
                UploadHeapResource.Reset();
            }

            if (DefaultHeapResource)
            {
                DefaultHeapResource->Unmap(0, nullptr); // reset should and probably does unmap, but we conver our tracks
                DefaultHeapResource.Reset();
            }

            MappedData = nullptr;
        }
    };
}



