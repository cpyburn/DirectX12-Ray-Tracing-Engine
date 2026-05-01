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
    }

    void AnimationCompute::CreateBuffers(const std::vector<AssimpFactory::VSVertices>& vertices, const std::vector<AssimpAnimations::VertexBoneData>& boneData, const std::vector<XMMATRIX>& bones, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator)
    {
        baseVertices = BufferHelpers::CreateBufferOnDefaultHeap<AssimpFactory::VSVertices>(m_deviceResources, vertices, commandList, commandAllocator, L"Base Vertices Buffer");
        boneBuffer = BufferHelpers::CreateBufferOnDefaultHeap<AssimpAnimations::VertexBoneData>(m_deviceResources, boneData, commandList, commandAllocator, L"Bone Data Buffer");
        boneMatrices = BufferHelpers::CreateBufferOnDefaultHeap<XMMATRIX>(m_deviceResources, bones, commandList, commandAllocator, L"Bone Matrices Buffer");
        
        outVertices = BufferHelpers::CreateBuffer(m_deviceResources->GetD3DDevice(), vertices.size(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_NONE);
    }

    void AnimationCompute::Dispatch(ID3D12GraphicsCommandList4* commandList)
    {		
        ID3D12DescriptorHeap* heaps[] = { GraphicsContexts::c_heap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);

        commandList->SetPipelineState(m_pso.Get());
        commandList->SetComputeRootSignature(m_rootSig.Get());

        //commandList->SetComputeRootDescriptorTable(0, srv);
        //commandList->SetComputeRootDescriptorTable(1, uav);

        //UINT groups = (sys.vertexCount + 255) / 256;
        //commandList->Dispatch(groups, 1, 1);

        // UAV barrier
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(outVertices.Get()));
    }
}

