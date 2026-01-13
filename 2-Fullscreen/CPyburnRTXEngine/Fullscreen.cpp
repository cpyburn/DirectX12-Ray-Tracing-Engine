#include "pchlib.h"
#include "Fullscreen.h"

#include "GraphicsContexts.h"

using namespace CPyburnRTXEngine;

const float Fullscreen::QuadWidth = 20.0f;
const float Fullscreen::QuadHeight = 720.0f;
const float Fullscreen::LetterboxColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

const Fullscreen::Resolution Fullscreen::m_resolutionOptions[] =
{
    { 800u, 600u },
    { 1200u, 900u },
    { 1280u, 720u },
    { 1920u, 1080u },
    { 1920u, 1200u },
    { 2560u, 1440u },
    { 3440u, 1440u },
    { 3840u, 2160u }
};
const UINT Fullscreen::m_resolutionOptionsCount = _countof(m_resolutionOptions);
UINT Fullscreen::m_resolutionIndex = 2;

Fullscreen::Fullscreen()
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;
}

Fullscreen::~Fullscreen()
{
}

void Fullscreen::Update(DX::StepTimer const& timer)
{
}

void Fullscreen::Render()
{
}

void Fullscreen::CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResource)
{
	m_deviceResource = deviceResource;
	m_device = deviceResource->GetD3DDevice();

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
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_sceneRootSignature)));
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
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_postRootSignature)));
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

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_scenePipelineState)));
        NAME_D3D12_OBJECT(m_scenePipelineState);

        psoDesc.InputLayout = { scaleInputElementDescs, _countof(scaleInputElementDescs) };
        psoDesc.pRootSignature = m_postRootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(postVertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(postPixelShader.Get());

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_postPipelineState)));
        NAME_D3D12_OBJECT(m_postPipelineState);
    }

    // Single-use command allocator and command list for creating resources.
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));


    // Create/update the vertex buffer.
    ComPtr<ID3D12Resource> sceneVertexBufferUpload;
    {
        // Define the geometry for a thin quad that will animate across the screen.
        const float x = QuadWidth / 2.0f;
        const float y = QuadHeight / 2.0f;
        SceneVertex quadVertices[] =
        {
            { { -x, -y, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -x, y, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { x, -y, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { x, y, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(quadVertices);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_sceneVertexBuffer)));

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&sceneVertexBufferUpload)));

        NAME_D3D12_OBJECT(m_sceneVertexBuffer);

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(sceneVertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, quadVertices, sizeof(quadVertices));
        sceneVertexBufferUpload->Unmap(0, nullptr);

        commandList->CopyBufferRegion(m_sceneVertexBuffer.Get(), 0, sceneVertexBufferUpload.Get(), 0, vertexBufferSize);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_sceneVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        // Initialize the vertex buffer views.
        m_sceneVertexBufferView.BufferLocation = m_sceneVertexBuffer->GetGPUVirtualAddress();
        m_sceneVertexBufferView.StrideInBytes = sizeof(SceneVertex);
        m_sceneVertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create/update the fullscreen quad vertex buffer.
    ComPtr<ID3D12Resource> postVertexBufferUpload;
    {
        // Define the geometry for a fullscreen quad.
        PostVertex quadVertices[] =
        {
            { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },    // Bottom left.
            { { -1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },    // Top left.
            { { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },    // Bottom right.
            { { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } }        // Top right.
        };

        const UINT vertexBufferSize = sizeof(quadVertices);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_postVertexBuffer)));

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&postVertexBufferUpload)));

        NAME_D3D12_OBJECT(m_postVertexBuffer);

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(postVertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, quadVertices, sizeof(quadVertices));
        postVertexBufferUpload->Unmap(0, nullptr);

        commandList->CopyBufferRegion(m_postVertexBuffer.Get(), 0, postVertexBufferUpload.Get(), 0, vertexBufferSize);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_postVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        // Initialize the vertex buffer views.
        m_postVertexBufferView.BufferLocation = m_postVertexBuffer->GetGPUVirtualAddress();
        m_postVertexBufferView.StrideInBytes = sizeof(PostVertex);
        m_postVertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create the constant buffer.
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneConstantBuffer) * DeviceResources::c_backBufferCount),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_sceneConstantBuffer)));

        NAME_D3D12_OBJECT(m_sceneConstantBuffer);

        // Describe and create constant buffer views.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_sceneConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = sizeof(SceneConstantBuffer);

        m_cbcbvSrv = GraphicsContexts::GetAvailableHeapPosition();
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(GraphicsContexts::GetHeap()->GetCPUDescriptorHandleForHeapStart(), m_cbcbvSrv, GraphicsContexts::GetDescriptorSize());

        for (UINT n = 0; n < DeviceResources::c_backBufferCount; n++)
        {
            m_device->CreateConstantBufferView(&cbvDesc, cpuHandle);

            cbvDesc.BufferLocation += sizeof(SceneConstantBuffer);
            cpuHandle.Offset(GraphicsContexts::GetDescriptorSize());
        }

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_sceneConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
        memcpy(m_pCbvDataBegin, &m_sceneConstantBufferData, sizeof(m_sceneConstantBufferData));
    }

    // Close the resource creation command list and execute it to begin the vertex buffer copy into
    // the default heap.
    ThrowIfFailed(commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    m_deviceResource->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    //{
    //    ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    //    m_fenceValues[m_frameIndex]++;

    //    // Create an event handle to use for frame synchronization.
    //    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    //    if (m_fenceEvent == nullptr)
    //    {
    //        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    //    }

    //    // Wait for the command list to execute before continuing.
    //    WaitForGpu();
    //}
}

void Fullscreen::CreateWindowSizeDependentResources()
{

}

void CPyburnRTXEngine::Fullscreen::UpdatePostViewAndScissor()
{
    float viewWidthRatio = static_cast<float>(m_resolutionOptions[m_resolutionIndex].Width) / m_deviceResource->GetScreenViewport().Width;
    float viewHeightRatio = static_cast<float>(m_resolutionOptions[m_resolutionIndex].Height) / m_deviceResource->GetScreenViewport().Height;

    float x = 1.0f;
    float y = 1.0f;

    if (viewWidthRatio < viewHeightRatio)
    {
        // The scaled image's height will fit to the viewport's height and 
        // its width will be smaller than the viewport's width.
        x = viewWidthRatio / viewHeightRatio;
    }
    else
    {
        // The scaled image's width will fit to the viewport's width and 
        // its height may be smaller than the viewport's height.
        y = viewHeightRatio / viewWidthRatio;
    }

    m_postViewport.TopLeftX = m_deviceResource->GetScreenViewport().Width * (1.0f - x) / 2.0f;
    m_postViewport.TopLeftY = m_deviceResource->GetScreenViewport().Height * (1.0f - y) / 2.0f;
    m_postViewport.Width = x * m_deviceResource->GetScreenViewport().Width;
    m_postViewport.Height = y * m_deviceResource->GetScreenViewport().Height;

    m_postScissorRect.left = static_cast<LONG>(m_postViewport.TopLeftX);
    m_postScissorRect.right = static_cast<LONG>(m_postViewport.TopLeftX + m_postViewport.Width);
    m_postScissorRect.top = static_cast<LONG>(m_postViewport.TopLeftY);
    m_postScissorRect.bottom = static_cast<LONG>(m_postViewport.TopLeftY + m_postViewport.Height);
}

std::wstring Fullscreen::GetAssetFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
}
