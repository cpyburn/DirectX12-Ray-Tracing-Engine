#pragma once

#include "d3dx12.h"

namespace CPyburnRTXEngine
{
    template<typename T>
    class ConstantBuffer
    {
    private:
        void Initialize()
        {
            if (HeapIndex[0] == 0)
            {
                for (UINT n = 0; n < DX::DeviceResources::c_backBufferCount; n++)
                {
                    HeapIndex[n] = GraphicsContexts::GetAvailableHeapPosition();
                    CpuHandle[n] = CD3DX12_CPU_DESCRIPTOR_HANDLE(GraphicsContexts::GetCpuHandle(HeapIndex[n]));
                    GpuHandle[n] = CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::GetGpuHandle(HeapIndex[n]));
                }
            }
        }

    public:
        ConstantBuffer()
        {

        }

        ~ConstantBuffer()
        {
            Release();
        }

        using ConstantType = T;

        // CPU-side data (what you write to)
        T CpuData{};

        // D3D12 requires 256-byte alignment for CBVs
        static constexpr UINT AlignedSize = (sizeof(T) + 255u) & ~255u;

        // Per-frame descriptor heap positions
        UINT HeapIndex[DX::DeviceResources::c_backBufferCount]{};

        CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle[DX::DeviceResources::c_backBufferCount]{};
        CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle[DX::DeviceResources::c_backBufferCount]{};

        // Upload heap resource
        Microsoft::WRL::ComPtr<ID3D12Resource> Resource;

        // Persistently mapped pointer
        uint8_t* MappedData = nullptr;

        void CreateCbvOnUploadHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, const WCHAR* name = L"CBV not named")
        {
            Initialize(); // make sure descriptor heap is allocated

            DX::ThrowIfFailed(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(AlignedSize * DX::DeviceResources::c_backBufferCount),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&Resource)));

            Resource->SetName(name);

            // Create constant buffer views to access the upload buffer.
            D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = Resource->GetGPUVirtualAddress();

            for (UINT n = 0; n < DX::DeviceResources::c_backBufferCount; n++)
            {
                // Describe and create constant buffer views.
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation = cbvGpuAddress;
                cbvDesc.SizeInBytes = AlignedSize;
                device->CreateConstantBufferView(&cbvDesc, CpuHandle[n]);

                cbvGpuAddress += AlignedSize;
            }

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            DX::ThrowIfFailed(Resource->Map(0, &readRange, reinterpret_cast<void**>(&MappedData)));
        }

        // Convenience: write CPU data into mapped memory
        void CopyToGpu(UINT frameIndex)
        {
            memcpy(MappedData + AlignedSize * frameIndex, &CpuData, sizeof(T));
        }

        void Release()
        {
            // if heap is already been assigned
            if (HeapIndex[0] > 0)
            {
                for (UINT n = 0; n < DX::DeviceResources::c_backBufferCount; n++)
                {
                    GraphicsContexts::RemoveHeapPosition(HeapIndex[n]);
                }
            }

            if (Resource)
            {
                Resource->Unmap(0, nullptr); // reset should and probably does unmap, but we conver our tracks
                Resource.Reset();
            }
            MappedData = nullptr;
		}
    };
}



