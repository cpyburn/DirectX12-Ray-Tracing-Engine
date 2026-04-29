#include "pchlib.h"
#include "AnimationCompute.h"

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

        // 2 descriptor tables:
        //   table 0 -> SRVs t0..t4
        //   table 1 -> UAVs u0..u1
        CD3DX12_DESCRIPTOR_RANGE ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0); // t0..t4
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0); // u0..u1

        CD3DX12_ROOT_PARAMETER params[2];
        params[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        params[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(2, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
        DX::ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob));

        DX::ThrowIfFailed(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));

        D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = m_rootSig.Get();
        pso.CS = { shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize() };
        DX::ThrowIfFailed(device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&m_pso)));


    }
}

