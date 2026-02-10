#include "pchlib.h"
#include "FrameResource.h"

namespace CPyburnRTXEngine
{
    FrameResource::FrameResource()
    {

    }

    FrameResource::~FrameResource()
    {
        Release();
    }

    void FrameResource::Init(DeviceResources* deviceResources)
    {
        Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = deviceResources->GetD3DDevice();

        for (UINT i = 0; i < COMMAND_LIST_COUNT; i++)
        {
            ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
            ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[i].Get(), nullptr, IID_PPV_ARGS(&m_commandLists[i])));

            NAME_D3D12_OBJECT_INDEXED(m_commandLists, i);

            // Close these command lists; don't record into them for now.
            ThrowIfFailed(m_commandLists[i]->Close());
        }

        // Batch up command lists for execution later.
        BatchSubmit[0] = m_commandLists[COMMAND_LIST_SCENE_0].Get();
        BatchSubmit[1] = m_commandLists[COMMAND_LIST_POST_1].Get();
    }

    ID3D12GraphicsCommandList4* FrameResource::ResetCommandList(const int commandList, ID3D12PipelineState* pInitialState)
    {
        ThrowIfFailed(m_commandAllocators[commandList]->Reset());
        ThrowIfFailed(m_commandLists[commandList]->Reset(m_commandAllocators[commandList].Get(), pInitialState));
        return m_commandLists[commandList].Get();
    }

    void FrameResource::Release()
    {
        for (int i = 0; i < COMMAND_LIST_COUNT; i++)
        {
            m_commandAllocators[i].Reset();
            m_commandLists[i].Reset();
        }
    }
}
