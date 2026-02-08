#pragma once

#include "d3dx12.h"

namespace CPyburnRTXEngine
{
    template<typename T, UINT FrameCount>
    class ConstantBuffer
    {
    public:
        using ConstantType = T;

        // CPU-side data (what you write to)
        T CpuData{};

        // D3D12 requires 256-byte alignment for CBVs
        static constexpr UINT AlignedSize =
            (sizeof(T) + 255u) & ~255u;

        // Per-frame descriptor heap positions
        UINT HeapIndex[FrameCount]{};

        // Per-frame GPU descriptor handles
        CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle[FrameCount]{};

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
            for (UINT n = 0; n < FrameCount; n++)
            {
                GraphicsContexts::RemoveHeapPosition(HeapIndex[n]);
            }
			Resource->Unmap(0, nullptr); // reset should and probably does unmap, but we conver our tracks
            Resource.Reset();
            MappedData = nullptr;
		}
    };
}



