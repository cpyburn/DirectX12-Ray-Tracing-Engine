#pragma once

#include "pchlib.h"
#include "AssimpFactory.h"

namespace CPyburnRTXEngine
{
    template<typename T>
    class DefaultBuffer
    {
    private:

    public:
        DefaultBuffer()
        {

        }

        ~DefaultBuffer()
        {
            Release();
        }

        static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferOnDefaultHeap(const std::shared_ptr<DX::DeviceResources>& deviceResources, const UINT& count, const T* data, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator, const WCHAR* name = L"Buffer not named")
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> dafaultBuffer;

            // create createTriangleVB
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
