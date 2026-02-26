#pragma once

#include "d3dx12.h"

namespace CPyburnRTXEngine
{
    template<typename T>
    class ConstantBuffer
    {
    public:
        ConstantBuffer()
        {
            for (UINT n = 0; n < DX::DeviceResources::c_backBufferCount; n++)
            {
                HeapIndex[n] = GraphicsContexts::GetAvailableHeapPosition();
            }
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

        // Per-frame GPU descriptor handles
        CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle[DX::DeviceResources::c_backBufferCount]{};

        // Upload heap resource
        Microsoft::WRL::ComPtr<ID3D12Resource> Resource;

        // Persistently mapped pointer
        uint8_t* MappedData = nullptr;

        // Convenience: write CPU data into mapped memory
        void CopyToGpu(UINT frameIndex)
        {
            memcpy(MappedData + AlignedSize * frameIndex, &CpuData, sizeof(T));
        }

        void Release()
        {
            for (UINT n = 0; n < DX::DeviceResources::c_backBufferCount; n++)
            {
                GraphicsContexts::RemoveHeapPosition(HeapIndex[n]);
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



