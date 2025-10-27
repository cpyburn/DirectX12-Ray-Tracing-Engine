#include "pchlib.h"
#include "Fullscreen.h"

using namespace CPyburnRTXEngine;

CPyburnRTXEngine::Fullscreen::Fullscreen()
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;
}

CPyburnRTXEngine::Fullscreen::~Fullscreen()
{
}

void CPyburnRTXEngine::Fullscreen::Update(DX::StepTimer const& timer)
{
}

void CPyburnRTXEngine::Fullscreen::Render()
{
}

void Fullscreen::CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResource)
{
	m_deviceResource = deviceResource;

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    // Create a root signature consisting of a descriptor table with a single CBV.
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(deviceResource->GetD3DDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_sceneRootSignature)));
        NAME_D3D12_OBJECT(m_sceneRootSignature);
    }

    // Create a root signature consisting of a descriptor table with a SRV and a sampler.
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];

        // We don't modify the SRV in the post-processing command list after
        // SetGraphicsRootDescriptorTable is executed on the GPU so we can use the default
        // range behavior: D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

        // Allow input layout and pixel shader access and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        // Create a sampler.
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(deviceResource->GetD3DDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_postRootSignature)));
        NAME_D3D12_OBJECT(m_postRootSignature);
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> sceneVertexShader;
        ComPtr<ID3DBlob> scenePixelShader;
        ComPtr<ID3DBlob> postVertexShader;
        ComPtr<ID3DBlob> postPixelShader;
        ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"sceneShaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &sceneVertexShader, &error));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"sceneShaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &scenePixelShader, &error));

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"postShaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &postVertexShader, &error));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"postShaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &postPixelShader, &error));

        // Define the vertex input layouts.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        D3D12_INPUT_ELEMENT_DESC scaleInputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state objects (PSOs).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_sceneRootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(sceneVertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(scenePixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(deviceResource->GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_scenePipelineState)));
        NAME_D3D12_OBJECT(m_scenePipelineState);

        psoDesc.InputLayout = { scaleInputElementDescs, _countof(scaleInputElementDescs) };
        psoDesc.pRootSignature = m_postRootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(postVertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(postPixelShader.Get());

        ThrowIfFailed(deviceResource->GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_postPipelineState)));
        NAME_D3D12_OBJECT(m_postPipelineState);
    }

    // Single-use command allocator and command list for creating resources.
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ThrowIfFailed(deviceResource->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ThrowIfFailed(deviceResource->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    // Create the command lists.
    {
        ThrowIfFailed(deviceResource->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_sceneCommandAllocators[0].Get(), m_scenePipelineState.Get(), IID_PPV_ARGS(&m_sceneCommandList)));
        NAME_D3D12_OBJECT(m_sceneCommandList);

        ThrowIfFailed(deviceResource->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_postCommandAllocators[0].Get(), m_postPipelineState.Get(), IID_PPV_ARGS(&m_postCommandList)));
        NAME_D3D12_OBJECT(m_postCommandList);

        // Close the command lists.
        ThrowIfFailed(m_sceneCommandList->Close());
        ThrowIfFailed(m_postCommandList->Close());
    }
}

void CPyburnRTXEngine::Fullscreen::CreateWindowSizeDependentResources()
{
}

std::wstring CPyburnRTXEngine::Fullscreen::GetAssetFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
}
