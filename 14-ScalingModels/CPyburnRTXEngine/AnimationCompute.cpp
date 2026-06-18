#include "pchlib.h"
#include "AnimationCompute.h"

#include "GraphicsContexts.h"

namespace CPyburnRTXEngine
{
    AnimationCompute::AnimationCompute()
    {
    }

    AnimationCompute::~AnimationCompute()
    {
    }

    void AnimationCompute::CreateDeviceDependentResources(DX::DeviceResources* deviceResources)
    {
        m_deviceResources = deviceResources;

        Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = GraphicsContexts::CompileHlslLibrary(m_deviceResources->GetD3DDevice(), L"skinnedCompute.hlsl", L"CS", L"cs_6_0");

        //CD3DX12_DESCRIPTOR_RANGE ranges[2];
        //ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0); // t0–t2
        //ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

        //CD3DX12_ROOT_PARAMETER params[2];
        //params[0].InitAsDescriptorTable(1, &ranges[0]);
        //params[1].InitAsDescriptorTable(1, &ranges[1]);

        //CD3DX12_ROOT_SIGNATURE_DESC desc;
        //desc.Init(2, params);

        CD3DX12_DESCRIPTOR_RANGE ranges[3];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); // t0-t1
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2 because we have to buffer 
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

        CD3DX12_ROOT_PARAMETER params[3];
        params[0].InitAsDescriptorTable(1, &ranges[0]);
        params[1].InitAsDescriptorTable(1, &ranges[1]);
        params[2].InitAsDescriptorTable(1, &ranges[2]);

        CD3DX12_ROOT_SIGNATURE_DESC desc;
        desc.Init(3, params);

        Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);

        m_deviceResources->GetD3DDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));

        D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = m_rootSig.Get();
        pso.CS = { shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize() };

        m_deviceResources->GetD3DDevice()->CreateComputePipelineState(&pso, IID_PPV_ARGS(&m_pso));

        for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            m_boneMatricesBuffer[i].CreateDeviceDependentResources(m_deviceResources->GetD3DDevice());
        }
		m_outVertexBuffer.CreateDeviceDependentResources(m_deviceResources->GetD3DDevice());
    }

    void AnimationCompute::CreateBuffers(ID3D12GraphicsCommandList4* commandList, BufferHeap<AssimpFactory::VSVertices>* baseVertices, BufferHeap<AssimpFactory::VertexBoneData>* boneData, const std::vector<XMMATRIX>& bones)
    {
        //Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList;
        //Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        //DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
        //DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

		m_baseVertexBufferPtr = baseVertices;

		m_boneBufferPtr = boneData;

        for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            m_boneMatricesBuffer[i].CpuData = bones;
            m_boneMatricesBuffer[i].CreateOnUploadHeap(L"Bone Matrices Buffer");
        }

        // Output buffer
        m_outVertexBuffer.CpuData = m_baseVertexBufferPtr->CpuData;
        m_outVertexBuffer.CreateOnDefaultHeap(commandList, L"Out Vertices Buffer", D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        //DX::ThrowIfFailed(commandList->Close());
        //ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        //m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        //m_deviceResources->WaitForGpu();

        //DX::ThrowIfFailed(commandAllocator->Reset());
        //DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
    }

    void AnimationCompute::CreateShaderResources()
    {
        //m_boneBuffer.CreateShaderResourceView(); // t1
        // since bones are small, going to use SRV upload heap like a constant buffer
        for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            m_boneMatricesBuffer[i].CreateShaderResourceView(true); // t2
        }
        m_outVertexBuffer.CreateUnorderedAccessView(L"Output Vertices Buffer"); // U0
    }

    void AnimationCompute::Update(const std::vector<XMMATRIX>& bones)
    {
        if (m_boneMatricesBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData.size() == 0 || bones.size() == 0)
        {
            return;
        }

        m_boneMatricesBuffer[m_deviceResources->GetCurrentFrameIndex()].CpuData = bones;
        m_boneMatricesBuffer[m_deviceResources->GetCurrentFrameIndex()].CopyCpuDataToUploadHeap();
    }

    void AnimationCompute::Dispatch(ID3D12GraphicsCommandList4* commandList)
    {		
        PIXBeginEvent(commandList, 0, L"Animation Compute");

        ID3D12DescriptorHeap* heaps[] = { GraphicsContexts::c_heap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);

        commandList->SetPipelineState(m_pso.Get());
        commandList->SetComputeRootSignature(m_rootSig.Get());

        // Root parameter 0: SRVs t0-t1
        commandList->SetComputeRootDescriptorTable(0, m_baseVertexBufferPtr->GpuHandle);
        // Root parameter 1: SRVs t2 because we have to buffer
        commandList->SetComputeRootDescriptorTable(1, m_boneMatricesBuffer[m_deviceResources->GetCurrentFrameIndex()].GpuHandle);
        // Root parameter 2: UAV u0
        commandList->SetComputeRootDescriptorTable(2, m_outVertexBuffer.GpuHandle);

        const UINT groups = (static_cast<UINT>(m_baseVertexBufferPtr->CpuData.size()) + 255u) / 256u;
        commandList->Dispatch(groups, 1, 1);

        auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_outVertexBuffer.DefaultHeapResource.Get());
        commandList->ResourceBarrier(1, &barrier);

        PIXEndEvent(commandList);
    }

    void AnimationCompute::ReleaseUploadResources()
    {
        m_outVertexBuffer.ReleaseUploadResource();
    }
}

