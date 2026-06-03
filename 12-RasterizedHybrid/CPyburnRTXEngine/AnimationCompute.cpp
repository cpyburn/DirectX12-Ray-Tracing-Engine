#include "pchlib.h"
#include "AnimationCompute.h"

#include "BufferHelpers.h"
#include "GraphicsContexts.h"

namespace CPyburnRTXEngine
{
    AnimationCompute::AnimationCompute()
    {
    }

    AnimationCompute::~AnimationCompute()
    {
    }

    void AnimationCompute::CreateDeviceDependentResources(ID3D12Device5* d3dDevice)
    {
        m_d3dDevice = d3dDevice;

        Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = GraphicsContexts::CompileHlslLibrary(d3dDevice, L"skinnedCompute.hlsl", L"CS", L"cs_6_0");

        CD3DX12_DESCRIPTOR_RANGE ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0); // t0–t2
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

        CD3DX12_ROOT_PARAMETER params[2];
        params[0].InitAsDescriptorTable(1, &ranges[0]);
        params[1].InitAsDescriptorTable(1, &ranges[1]);

        CD3DX12_ROOT_SIGNATURE_DESC desc;
        desc.Init(2, params);

        Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);

        m_d3dDevice->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));

        D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = m_rootSig.Get();
        pso.CS = { shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize() };

        m_d3dDevice->CreateComputePipelineState(&pso, IID_PPV_ARGS(&m_pso));

		m_boneBuffer.CreateDeviceDependentResources(m_d3dDevice);
		m_boneMatricesBuffer.CreateDeviceDependentResources(m_d3dDevice);
		m_outVertexBuffer.CreateDeviceDependentResources(m_d3dDevice);
    }

    void AnimationCompute::CreateBuffers(ID3D12GraphicsCommandList4* commandList, BufferHeap<AssimpFactory::VSVertices>* baseVertices, const std::vector<AssimpAnimations::VertexBoneData>& boneData, const std::vector<XMMATRIX>& bones)
    {
        //Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList;
        //Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        //DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
        //DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

		m_baseVertexBuffer = baseVertices;

		m_boneBuffer.CpuData = boneData;
		m_boneBuffer.CreateOnDefaultHeap(commandList, L"Bone Data Buffer");

		m_boneMatricesBuffer.CpuData = bones;
        m_boneMatricesBuffer.CreateOnUploadHeap(L"Bone Matrices Buffer");

        // Output buffer
        m_outVertexBuffer.CpuData = m_baseVertexBuffer->CpuData;
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
        m_boneBuffer.CreateShaderResourceView(); // t1
        // since bones are usually small, going to use upload heap
        m_boneMatricesBuffer.CreateShaderResourceView(true); // t2
        m_outVertexBuffer.CreateUnorderedAccessView(L"Output Vertices Buffer"); // U0
    }

    void AnimationCompute::Update(const std::vector<XMMATRIX>& bones)
    {
        if (m_boneMatricesBuffer.CpuData.size() == 0 || bones.size() == 0)
        {
            return;
        }

        m_boneMatricesBuffer.CpuData = bones;
        m_boneMatricesBuffer.UpdateUploadHeap();
    }

    void AnimationCompute::Dispatch(ID3D12GraphicsCommandList4* commandList)
    {		
        PIXBeginEvent(commandList, 0, L"Animation Compute");

        ID3D12DescriptorHeap* heaps[] = { GraphicsContexts::c_heap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);

        commandList->SetPipelineState(m_pso.Get());
        commandList->SetComputeRootSignature(m_rootSig.Get());

        // Root parameter 0: SRVs t0-t2
        commandList->SetComputeRootDescriptorTable(0, m_baseVertexBuffer->GpuHandle);

        // Root parameter 1: UAV u0
        commandList->SetComputeRootDescriptorTable(1, m_outVertexBuffer.GpuHandle);

        const UINT groups = (static_cast<UINT>(m_baseVertexBuffer->CpuData.size()) + 255u) / 256u;
        commandList->Dispatch(groups, 1, 1);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_outVertexBuffer.DefaultHeapResource.Get()));

        PIXEndEvent(commandList);
    }

    void AnimationCompute::ReleaseUploadResources()
    {
        m_boneBuffer.ReleaseUploadResource();
        m_outVertexBuffer.ReleaseUploadResource();
    }
}

