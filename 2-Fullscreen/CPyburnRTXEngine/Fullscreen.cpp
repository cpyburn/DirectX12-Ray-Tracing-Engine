#include "pchlib.h"
#include "Fullscreen.h"

#include "GraphicsContexts.h"
#include "FrameResource.h"

using namespace CPyburnRTXEngine;

const float Fullscreen::QuadWidth = 20.0f;
const float Fullscreen::QuadHeight = 720.0f;
const float Fullscreen::LetterboxColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
const float Fullscreen::ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

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

Fullscreen::Fullscreen() :
    m_sceneConstantBufferData{},
    m_sceneViewport(0.0f, 0.0f, 0.0f, 0.0f),
    m_sceneScissorRect(0, 0, 0, 0),
    m_postViewport(0.0f, 0.0f, 0.0f, 0.0f)
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
    const float translationSpeed = 0.0001f;
    const float offsetBounds = 1.0f;

    m_sceneConstantBufferData.offset.x += translationSpeed;
    if (m_sceneConstantBufferData.offset.x > offsetBounds)
    {
        m_sceneConstantBufferData.offset.x = -offsetBounds;
    }

    XMMATRIX transform = XMMatrixMultiply(
        XMMatrixOrthographicLH(static_cast<float>(m_resolutionOptions[m_resolutionIndex].Width), static_cast<float>(m_resolutionOptions[m_resolutionIndex].Height), 0.0f, 100.0f),
        XMMatrixTranslation(m_sceneConstantBufferData.offset.x, 0.0f, 0.0f));

    XMStoreFloat4x4(&m_sceneConstantBufferData.transform, XMMatrixTranspose(transform));

    UINT offset = m_deviceResource->GetCurrentFrameIndex() * c_alignedSceneConstantBuffer;
    memcpy(m_pSceneConstantBufferDataBegin + offset, &m_sceneConstantBufferData, sizeof(m_sceneConstantBufferData));
}

void Fullscreen::Render()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    //ThrowIfFailed(m_sceneCommandAllocators[m_frameIndex]->Reset());
    //ThrowIfFailed(m_postCommandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    //ThrowIfFailed(m_sceneCommandList->Reset(m_sceneCommandAllocators[m_frameIndex].Get(), m_scenePipelineState.Get()));
    //ThrowIfFailed(m_postCommandList->Reset(m_postCommandAllocators[m_frameIndex].Get(), m_postPipelineState.Get()));

    ID3D12GraphicsCommandList* m_sceneCommandList = m_deviceResource->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_SCENE_0, m_scenePipelineState.Get());
    ID3D12GraphicsCommandList* m_postCommandList = m_deviceResource->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_POST_1, m_postPipelineState.Get());

    // Populate m_sceneCommandList to render scene to intermediate render target.
    {
        // Set necessary state.
        m_sceneCommandList->SetGraphicsRootSignature(m_sceneRootSignature.Get());

        ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get()};
        m_sceneCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        //D3D12_RESOURCE_BARRIER barriers[] = {
        //CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResource->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET) };

        //m_sceneCommandList->ResourceBarrier(_countof(barriers), barriers);

        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuHandleSceneConstantBuffer[m_deviceResource->GetCurrentFrameIndex()]);
        m_sceneCommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);
        m_sceneCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_sceneCommandList->RSSetViewports(1, &m_sceneViewport);
        m_sceneCommandList->RSSetScissorRects(1, &m_sceneScissorRect);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_deviceResource->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), m_rtvHeapPositionPostSrv, m_deviceResource->GetRtvDescriptorSize());
        //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_deviceResource->GetRenderTargetView();
        m_sceneCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        // Record commands.
        m_sceneCommandList->ClearRenderTargetView(rtvHandle, ClearColor, 0, nullptr);
        m_sceneCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_sceneCommandList->IASetVertexBuffers(0, 1, &m_sceneVertexBufferView);

        PIXBeginEvent(m_sceneCommandList, 0, L"Draw a thin rectangle");
        m_sceneCommandList->DrawInstanced(4, 1, 0, 0);

        //barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResource->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        //m_sceneCommandList->ResourceBarrier(_countof(barriers), barriers);

        PIXEndEvent(m_sceneCommandList);
    }

    ThrowIfFailed(m_sceneCommandList->Close());

    // Populate m_postCommandList to scale intermediate render target to screen.
    {
        // Set necessary state.
        m_postCommandList->SetGraphicsRootSignature(m_postRootSignature.Get());

        ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get() };
        m_postCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        
        // Indicate that the back buffer will be used as a render target and the
        // intermediate render target will be used as a SRV.
        D3D12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResource->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(m_intermediateRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };

        m_postCommandList->ResourceBarrier(_countof(barriers), barriers);

        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart(), m_cbvSrvHeapPositionPost, GraphicsContexts::c_descriptorSize);
        m_postCommandList->SetGraphicsRootDescriptorTable(0, cbvSrvHandle); // srv location GPU handle
        m_postCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_postCommandList->RSSetViewports(1, &m_postViewport);
        m_postCommandList->RSSetScissorRects(1, &m_postScissorRect);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_deviceResource->GetRenderTargetView();
        m_postCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        // Record commands.
        m_postCommandList->ClearRenderTargetView(rtvHandle, LetterboxColor, 0, nullptr);
        m_postCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_postCommandList->IASetVertexBuffers(0, 1, &m_postVertexBufferView);

        PIXBeginEvent(m_postCommandList, 0, L"Draw texture to screen.");
        m_postCommandList->DrawInstanced(4, 1, 0, 0);
        PIXEndEvent(m_postCommandList);

        // Revert resource states back to original values.
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        m_postCommandList->ResourceBarrier(_countof(barriers), barriers);
    }

    ThrowIfFailed(m_postCommandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_sceneCommandList, m_postCommandList };
    m_deviceResource->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_deviceResource->Present();
}

void Fullscreen::CreateDeviceDependentResources()
{

}

void Fullscreen::CreateWindowSizeDependentResources(const std::shared_ptr<DeviceResources>& deviceResource)
{
    m_deviceResource = deviceResource;
    ComPtr<ID3D12Device> device = deviceResource->GetD3DDevice();

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
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_sceneRootSignature)));
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
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_postRootSignature)));
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
        psoDesc.RTVFormats[0] = m_deviceResource->GetBackBufferFormat();
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_scenePipelineState)));
        NAME_D3D12_OBJECT(m_scenePipelineState);

        psoDesc.InputLayout = { scaleInputElementDescs, _countof(scaleInputElementDescs) };
        psoDesc.pRootSignature = m_postRootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(postVertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(postPixelShader.Get());

        ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_postPipelineState)));
        NAME_D3D12_OBJECT(m_postPipelineState);
    }

    // Single-use command allocator and command list for creating resources.
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    // Reserve heap position for the post-process SRV.
    {
        m_rtvHeapPositionPostSrv = DeviceResources::c_backBufferCount; // RTV right after the swap chain RTVs. There shouldnt be a lot of RTVs in an engine, so we can keep track of these
        m_cbvSrvHeapPositionPost = GraphicsContexts::GetAvailableHeapPosition(); // SRV in the CbvSrv heap
    }

    LoadSizeDependentResources();
    LoadSceneResolutionDependentResources();

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

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_sceneVertexBuffer)));

        ThrowIfFailed(device->CreateCommittedResource(
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

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_postVertexBuffer)));

        ThrowIfFailed(device->CreateCommittedResource(
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
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(c_alignedSceneConstantBuffer* DeviceResources::c_backBufferCount),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_sceneConstantBuffer)));

        NAME_D3D12_OBJECT(m_sceneConstantBuffer);

        // Create constant buffer views to access the upload buffer.
        D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_sceneConstantBuffer->GetGPUVirtualAddress();

        for (UINT n = 0; n < DeviceResources::c_backBufferCount; n++)
        {
            m_heapPositionSceneConstantBuffer[n] = GraphicsContexts::GetAvailableHeapPosition();
            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), m_heapPositionSceneConstantBuffer[n], GraphicsContexts::c_descriptorSize);

            // Describe and create constant buffer views.
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = cbvGpuAddress;
            cbvDesc.SizeInBytes = c_alignedSceneConstantBuffer;
            device->CreateConstantBufferView(&cbvDesc, cpuHandle);

            cbvDesc.BufferLocation += c_alignedSceneConstantBuffer;
            m_gpuHandleSceneConstantBuffer[n] = CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart(), m_heapPositionSceneConstantBuffer[n], GraphicsContexts::c_descriptorSize);
        }

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_sceneConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pSceneConstantBufferDataBegin)));
        memcpy(m_pSceneConstantBufferDataBegin, &m_sceneConstantBufferData, sizeof(m_sceneConstantBufferData));
    }

    // Close the resource creation command list and execute it to begin the vertex buffer copy into
    // the default heap.
    ThrowIfFailed(commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    m_deviceResource->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    m_deviceResource->WaitForGpu();
}

std::wstring Fullscreen::GetAssetFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
}


void Fullscreen::UpdatePostViewAndScissor()
{
    float viewWidthRatio = static_cast<float>(m_resolutionOptions[m_resolutionIndex].Width) / m_width;
    float viewHeightRatio = static_cast<float>(m_resolutionOptions[m_resolutionIndex].Height) / m_height;

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

    m_postViewport.TopLeftX = m_width * (1.0f - x) / 2.0f;
    m_postViewport.TopLeftY = m_height * (1.0f - y) / 2.0f;
    m_postViewport.Width = x * m_width;
    m_postViewport.Height = y * m_height;

    m_postScissorRect.left = static_cast<LONG>(m_postViewport.TopLeftX);
    m_postScissorRect.right = static_cast<LONG>(m_postViewport.TopLeftX + m_postViewport.Width);
    m_postScissorRect.top = static_cast<LONG>(m_postViewport.TopLeftY);
    m_postScissorRect.bottom = static_cast<LONG>(m_postViewport.TopLeftY + m_postViewport.Height);
}

void Fullscreen::LoadSizeDependentResources()
{
    UpdatePostViewAndScissor();

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_deviceResource->GetRenderTargetView();

        // Create a RTV for each frame.
        for (UINT n = 0; n < DeviceResources::c_backBufferCount; n++)
        {
            ID3D12Resource* renderTarget = m_deviceResource->GetRenderTarget();
            ThrowIfFailed(m_deviceResource->GetSwapChain()->GetBuffer(n, IID_PPV_ARGS(&renderTarget)));
            m_deviceResource->GetD3DDevice()->CreateRenderTargetView(renderTarget, nullptr, rtvHandle);
            rtvHandle.Offset(1, m_deviceResource->GetRtvDescriptorSize());

            SetName(renderTarget, L"Render Target:" + n);
        }
    }

    // Update resolutions shown in app title.
    UpdateTitle();

    // This is where you would create/resize intermediate render targets, depth stencils, or other resources
    // dependent on the window size.
}

void Fullscreen::LoadSceneResolutionDependentResources()
{
    // Update resolutions shown in app title.
    UpdateTitle();

    // Set up the scene viewport and scissor rect to match the current scene rendering resolution.
    {
        m_sceneViewport.Width = static_cast<float>(m_resolutionOptions[m_resolutionIndex].Width);
        m_sceneViewport.Height = static_cast<float>(m_resolutionOptions[m_resolutionIndex].Height);

        m_sceneScissorRect.right = static_cast<LONG>(m_resolutionOptions[m_resolutionIndex].Width);
        m_sceneScissorRect.bottom = static_cast<LONG>(m_resolutionOptions[m_resolutionIndex].Height);
    }

    // Update post-process viewport and scissor rectangle.
    UpdatePostViewAndScissor();

    // Create RTV for the intermediate render target.
    {
        D3D12_RESOURCE_DESC swapChainDesc = m_deviceResource->GetRenderTarget()->GetDesc();
        const CD3DX12_CLEAR_VALUE clearValue(swapChainDesc.Format, ClearColor);
        const CD3DX12_RESOURCE_DESC renderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            swapChainDesc.Format,
            m_resolutionOptions[m_resolutionIndex].Width,
            m_resolutionOptions[m_resolutionIndex].Height,
            1u, 1u,
            swapChainDesc.SampleDesc.Count,
            swapChainDesc.SampleDesc.Quality,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_TEXTURE_LAYOUT_UNKNOWN, 0u);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_deviceResource->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), m_rtvHeapPositionPostSrv, m_deviceResource->GetRtvDescriptorSize());
        ThrowIfFailed(m_deviceResource->GetD3DDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &renderTargetDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValue,
            IID_PPV_ARGS(&m_intermediateRenderTarget)));
        m_deviceResource->GetD3DDevice()->CreateRenderTargetView(m_intermediateRenderTarget.Get(), nullptr, rtvHandle);
        NAME_D3D12_OBJECT(m_intermediateRenderTarget);
    }

    // Create SRV for the intermediate render target.
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), m_cbvSrvHeapPositionPost, GraphicsContexts::c_descriptorSize);
    m_deviceResource->GetD3DDevice()->CreateShaderResourceView(m_intermediateRenderTarget.Get(), nullptr, cbvHandle);
}

void Fullscreen::UpdateTitle()
{
    // Update resolutions shown in app title.
    wchar_t updatedTitle[256];
    swprintf_s(updatedTitle, L"( %u x %u ) scaled to ( %u x %u )", m_resolutionOptions[m_resolutionIndex].Width, m_resolutionOptions[m_resolutionIndex].Height, m_width, m_height);
    char debug = (char)updatedTitle;
    DebugTrace(&debug);
}
