#pragma once

#include "pchlib.h"

namespace CPyburnRTXEngine
{
    template<typename T>
    class BufferHeap
    {
    private:
        ID3D12Device5* m_d3dDevice = nullptr;
        UINT m_reserveSizeOfCpuData = 0;
    public:
        // Per-frame descriptor heap positions
        UINT HeapIndex = MAXUINT;
        UINT BufferSize = 0;

        void CreateDeviceDependentResources(ID3D12Device5* d3dDevice)
        {
            m_d3dDevice = d3dDevice;
        }

        void CreateHeapPosition()
        {
            if (HeapIndex == MAXUINT)
            {
                HeapIndex = GraphicsContexts::GetAvailableHeapPosition();
                CpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GraphicsContexts::GetCpuHandle(HeapIndex));
                GpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::GetGpuHandle(HeapIndex));
            }
        }

        void GetNewHeapPosition()
		{
            ReleaseHeapPosition();
            CreateHeapPosition();
		}

        BufferHeap()
        {
            
        }

        ~BufferHeap()
        {
            Release();
        }

        // CPU-side data (what you write to)
        std::vector<T> CpuData;

        CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle;
        CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle;

        // Upload heap resource
        Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeapResource = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> DefaultHeapResource = nullptr;

        // Persistently mapped pointer
        uint8_t* MappedData = nullptr;

        void ReserveVectorSpace(const UINT& reserveSizeOfCpuData)
        {
            m_reserveSizeOfCpuData = reserveSizeOfCpuData;
            CpuData.reserve(reserveSizeOfCpuData);
        }

        void CreateOnUploadHeap(const WCHAR* name = L"Upload buffer not named")
        {
            BufferSize = static_cast<UINT>(sizeof(T) * CpuData.size());

            if (m_reserveSizeOfCpuData == 0)
            {
                m_reserveSizeOfCpuData = static_cast<UINT>(CpuData.size());
            }

            CD3DX12_RESOURCE_DESC bufferDescModel = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);
            DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &bufferDescModel,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&UploadHeapResource)));
            UploadHeapResource->SetName(name);

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            DX::ThrowIfFailed(UploadHeapResource->Map(0, &readRange, reinterpret_cast<void**>(&MappedData)));
        }

        void CopyCpuDataToUploadHeap()
		{
            if (static_cast<UINT>(CpuData.size()) > m_reserveSizeOfCpuData)
            {
                DebugTrace("BufferHeap CpuData > reserved size");
            }

			memcpy(MappedData, CpuData.data(), BufferSize);
		}

        void CreateOnDefaultHeap(ID3D12GraphicsCommandList4* commandList, const WCHAR* name = L"Dfault buffer not named", const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_NONE)
        {
            BufferSize = static_cast<UINT>(sizeof(T) * CpuData.capacity());

            if (m_reserveSizeOfCpuData == 0)
            {
                m_reserveSizeOfCpuData = static_cast<UINT>(CpuData.size());
            }

            CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize, flags);
            DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&DefaultHeapResource)));
            DefaultHeapResource->SetName(name);

            std::wstring wname = L"" + std::wstring(name);
            CreateOnUploadHeap(wname.c_str());

            CopyUploadToDefault(commandList);

            //// upload goes out of scope if we don't execute the command list and wait for the GPU to finish before exiting the function, so execute and wait here
            //DX::ThrowIfFailed(commandList->Close());
            //ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
            //m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            //m_deviceResources->WaitForGpu();

            //DX::ThrowIfFailed(commandAllocator->Reset());
            //DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
        }

        void CreateUnorderedAccessView(const WCHAR* name = L"Upload buffer not named")
        {
            CreateHeapPosition();

            UINT stride = static_cast<UINT>(sizeof(T));
            //CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(count * stride, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            //DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(
            //    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            //    D3D12_HEAP_FLAG_NONE,
            //    &bufferDesc,
            //    D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
            //    nullptr,
            //    IID_PPV_ARGS(&DefaultHeapResource)));
            //DefaultHeapResource->SetName(name);

            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
            uav.Format = DXGI_FORMAT_UNKNOWN;
            uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav.Buffer.FirstElement = 0;
            uav.Buffer.NumElements = static_cast<UINT>(CpuData.capacity());
            uav.Buffer.StructureByteStride = stride;
            uav.Buffer.CounterOffsetInBytes = 0;
            uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

            m_d3dDevice->CreateUnorderedAccessView(DefaultHeapResource.Get(), nullptr, &uav, CpuHandle);
        }

        void CopyUploadToDefault(ID3D12GraphicsCommandList4* commandList)
        {
            if (static_cast<UINT>(CpuData.size()) > m_reserveSizeOfCpuData)
            {
                DebugTrace("BufferHeap CpuData > reserved size");
            }

            // Upload the buffer to the GPU.
            {
                D3D12_SUBRESOURCE_DATA resourceData = {};
                resourceData.pData = &CpuData[0];
                resourceData.RowPitch = 0;
                resourceData.SlicePitch = 0;

                CD3DX12_RESOURCE_BARRIER indexBufferResourceBarrier =
                    CD3DX12_RESOURCE_BARRIER::Transition(DefaultHeapResource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
                commandList->ResourceBarrier(1, &indexBufferResourceBarrier);

                UpdateSubresources(commandList, DefaultHeapResource.Get(), UploadHeapResource.Get(), 0, 0, 1, &resourceData);

                indexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(DefaultHeapResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
                commandList->ResourceBarrier(1, &indexBufferResourceBarrier);
            }
        }

        void CreateShaderResourceView(bool useUploadHeap = false)
        {
            CreateHeapPosition();

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.StructureByteStride = static_cast < UINT>(sizeof(T)); // your vertex struct size goes here
            srvDesc.Buffer.NumElements = static_cast<UINT>(CpuData.capacity()); // number of vertices go here
            
            if (useUploadHeap)
            {
                m_d3dDevice->CreateShaderResourceView(UploadHeapResource.Get(), &srvDesc, CpuHandle);
            }
            else
                m_d3dDevice->CreateShaderResourceView(DefaultHeapResource.Get(), &srvDesc, CpuHandle);
        }

        void ReleaseHeapPosition()
		{
			if (HeapIndex != MAXUINT)
			{
				GraphicsContexts::RemoveHeapPosition(HeapIndex);
				HeapIndex = MAXUINT;
			}
		}

        void ReleaseUploadResource()
		{
			if (UploadHeapResource)
			{
				UploadHeapResource.Reset();
			}
		}

        void Release()
        {
            // if heap is already been assigned
            if (HeapIndex != MAXUINT)
            {
                GraphicsContexts::RemoveHeapPosition(HeapIndex);
            }

			ReleaseUploadResource();

            if (DefaultHeapResource)
            {
                DefaultHeapResource.Reset();
            }
        }
    };
}



