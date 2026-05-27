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

    void AnimationCompute::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    {
        m_deviceResources = deviceResources;
        auto device = deviceResources->GetD3DDevice();

        // Create the pipeline state, which includes compiling and loading shaders.
        Microsoft::WRL::ComPtr<IDxcCompiler> compiler;
        Microsoft::WRL::ComPtr<IDxcLibrary> library;
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;

        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));

        library->CreateBlobFromFile(GetAssetFullPath(L"skinnedCompute.hlsl").c_str(), nullptr, &source);

        Microsoft::WRL::ComPtr<IDxcOperationResult> result;
        DX::ThrowIfFailed(compiler->Compile(
            source.Get(),
            L"skinnedCompute.hlsl",
            L"CS",
            L"cs_6_0",
            nullptr, 0,
            nullptr, 0,
            nullptr,
            &result));

        HRESULT status;
        result->GetStatus(&status);

        if (FAILED(status))
        {
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> errors;
            result->GetErrorBuffer(&errors);

            if (errors)
            {
                OutputDebugStringA((char*)errors->GetBufferPointer());
            }

            DX::ThrowIfFailed(status);
        }

        Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
        DX::ThrowIfFailed(result->GetResult(&shaderBlob));

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

        device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));

        D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = m_rootSig.Get();
        pso.CS = { shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize() };

        device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&m_pso));

		m_baseVertices.CreateDeviceDependentResources(deviceResources);
		m_boneBuffer.CreateDeviceDependentResources(deviceResources);
		m_boneMatrices.CreateDeviceDependentResources(deviceResources);
		m_outVertices.CreateDeviceDependentResources(deviceResources);
    }

    void AnimationCompute::CreateBuffers(const std::vector<AssimpFactory::VSVertices>& vertices, const std::vector<AssimpAnimations::VertexBoneData>& boneData, const std::vector<XMMATRIX>& bones)
    {
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

		m_baseVertices.CpuData = vertices;
		m_baseVertices.CreateOnDefaultHeap(commandList, commandAllocator, L"Base Vertices Buffer");

		m_boneBuffer.CpuData = boneData;
		m_boneBuffer.CreateOnDefaultHeap(commandList, commandAllocator, L"Bone Data Buffer");

		m_boneMatrices.CpuData = bones;
        m_boneMatrices.CreateOnUploadHeap(L"Bone Matrices Buffer");

        // Output buffer
        m_outVertices.CpuData = vertices;
        m_outVertices.CreateOnDefaultHeap(commandList, commandAllocator, L"Out Vertices Buffer", D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        DX::ThrowIfFailed(commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        m_deviceResources->WaitForGpu();

        DX::ThrowIfFailed(commandAllocator->Reset());
        DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

        const UINT count = (UINT)(vertices.size());
        m_baseVertices.CreateShaderResourceView();
        m_boneBuffer.CreateShaderResourceView();
        // since bones are usually small, going to use upload heap
        m_boneMatrices.CreateShaderResourceView(true);
		m_outVertices.CreateUnorderedAccessView(L"Output Vertices Buffer");
    }

    void AnimationCompute::Update(const std::vector<XMMATRIX>& bones)
    {
        if (m_boneMatrices.CpuData.size() == 0 || bones.size() == 0)
        {
            return;
        }

        //std::vector<XMFLOAT4X4> transposed(bones.size());
        //for (size_t i = 0; i < bones.size(); ++i)
        //{
        //    XMStoreFloat4x4(&transposed[i], XMMatrixTranspose(bones[i]));
        //}
        //BufferHelpers::UploadData(boneMatrices.Get(), transposed.data(), transposed.size() * sizeof(XMFLOAT4X4));

        m_boneMatrices.CpuData = bones;
        m_boneMatrices.UpdateUploadHeap();
    }

    void AnimationCompute::Dispatch(ID3D12GraphicsCommandList4* commandList)
    {		
        PIXBeginEvent(commandList, 0, L"Animation Compute");

        ID3D12DescriptorHeap* heaps[] = { GraphicsContexts::c_heap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);

        commandList->SetPipelineState(m_pso.Get());
        commandList->SetComputeRootSignature(m_rootSig.Get());

        // Root parameter 0: SRVs t0-t2
        commandList->SetComputeRootDescriptorTable(0, m_baseVertices.GpuHandle);

        // Root parameter 1: UAV u0
        commandList->SetComputeRootDescriptorTable(1, m_outVertices.GpuHandle);

        const UINT groups = (static_cast<UINT>(m_baseVertices.CpuData.size()) + 255u) / 256u;
        commandList->Dispatch(groups, 1, 1);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_outVertices.DefaultHeapResource.Get()));

        PIXEndEvent(commandList);
    }
}

