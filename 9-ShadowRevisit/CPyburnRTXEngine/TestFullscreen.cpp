#include "pchlib.h"
#include "TestFullscreen.h"

namespace CPyburnRTXEngine
{
    const float TestFullscreen::QuadWidth = 20.0f;
    const float TestFullscreen::QuadHeight = 720.0f;
    const float TestFullscreen::ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

    TestFullscreen::TestFullscreen()
    {

    }

    TestFullscreen::~TestFullscreen()
    {
        Release();
    }

    void TestFullscreen::Update(DX::StepTimer const& timer)
    {
        const float translationSpeed = 0.0001f;
        const float offsetBounds = 1.0f;

        m_sceneConstantBuffer.CpuData.offset.x += translationSpeed;
        if (m_sceneConstantBuffer.CpuData.offset.x > offsetBounds)
        {
            m_sceneConstantBuffer.CpuData.offset.x = -offsetBounds;
        }

        XMMATRIX transform = XMMatrixMultiply(
            XMMatrixOrthographicLH(static_cast<float>(m_deviceResources->GetResolution().Width), static_cast<float>(m_deviceResources->GetResolution().Height), 0.0f, 100.0f),
            XMMatrixTranslation(m_sceneConstantBuffer.CpuData.offset.x, 0.0f, 0.0f));

        XMStoreFloat4x4(&m_sceneConstantBuffer.CpuData.transform, XMMatrixTranspose(transform));

        m_sceneConstantBuffer.CopyToGpu(m_deviceResources->GetCurrentFrameIndex());
    }

    void TestFullscreen::Render()
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

        ID3D12GraphicsCommandList* m_sceneCommandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_SCENE_0, m_scenePipelineState.Get());

        // Populate m_sceneCommandList to render scene to intermediate render target.
        {
            // Set necessary state.
            m_sceneCommandList->SetGraphicsRootSignature(m_sceneRootSignature.Get());

            ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get() };
            m_sceneCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            //D3D12_RESOURCE_BARRIER barriers[] = {
            //CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResource->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET) };

            //m_sceneCommandList->ResourceBarrier(_countof(barriers), barriers);

            CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_sceneConstantBuffer.GpuHandle[m_deviceResources->GetCurrentFrameIndex()]);
            m_sceneCommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);
            m_sceneCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_sceneCommandList->RSSetViewports(1, &m_deviceResources->GetScreenViewport());
            m_sceneCommandList->RSSetScissorRects(1, &m_deviceResources->GetScissorRect());

            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_deviceResources->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), DX::DeviceResources::c_backBufferCount, m_deviceResources->GetRtvDescriptorSize());
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

        DX::ThrowIfFailed(m_sceneCommandList->Close());
    }

    void TestFullscreen::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    {
        m_deviceResources = deviceResources;
        Microsoft::WRL::ComPtr<ID3D12Device> device = deviceResources->GetD3DDevice();

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

            Microsoft::WRL::ComPtr<ID3DBlob> signature;
            Microsoft::WRL::ComPtr<ID3DBlob> error;
            DX::ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
            DX::ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_sceneRootSignature)));
            NAME_D3D12_OBJECT(m_sceneRootSignature);
        }

        // Create the pipeline state, which includes compiling and loading shaders.
        {
            Microsoft::WRL::ComPtr<ID3DBlob> sceneVertexShader;
            Microsoft::WRL::ComPtr<ID3DBlob> scenePixelShader;
            Microsoft::WRL::ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            UINT compileFlags = 0;
#endif
            std::wstring shaderFilePath = GetAssetFullPath(L"sceneShaders.hlsl");
            DX::ThrowIfFailed(D3DCompileFromFile(shaderFilePath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &sceneVertexShader, &error));
            DX::ThrowIfFailed(D3DCompileFromFile(shaderFilePath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &scenePixelShader, &error));

            // Des
            //         // Define the vertex input layouts.
            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            // cribe and create the graphics pipeline state objects (PSOs).
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
            psoDesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
            psoDesc.SampleDesc.Count = 1;

            DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_scenePipelineState)));
            NAME_D3D12_OBJECT(m_scenePipelineState);
        }

        // Single-use command allocator and command list for creating resources.
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

        DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
        DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

        // Create/update the vertex buffer.
        Microsoft::WRL::ComPtr<ID3D12Resource> sceneVertexBufferUpload;
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

            DX::ThrowIfFailed(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&m_sceneVertexBuffer)));

            DX::ThrowIfFailed(device->CreateCommittedResource(
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
            DX::ThrowIfFailed(sceneVertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
            memcpy(pVertexDataBegin, quadVertices, sizeof(quadVertices));
            sceneVertexBufferUpload->Unmap(0, nullptr);

            commandList->CopyBufferRegion(m_sceneVertexBuffer.Get(), 0, sceneVertexBufferUpload.Get(), 0, vertexBufferSize);
            commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_sceneVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

            // Initialize the vertex buffer views.
            m_sceneVertexBufferView.BufferLocation = m_sceneVertexBuffer->GetGPUVirtualAddress();
            m_sceneVertexBufferView.StrideInBytes = sizeof(SceneVertex);
            m_sceneVertexBufferView.SizeInBytes = vertexBufferSize;
        }

        // Create the constant buffer.
        {
            m_sceneConstantBuffer.CreateCbvOnUploadHeap(m_deviceResources->GetD3DDevice(), L"TestFullscreen Cbv");
            for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
            {
                m_sceneConstantBuffer.CopyToGpu(i);
            }
        }

        // Close the resource creation command list and execute it to begin the vertex buffer copy into
        // the default heap.
        DX::ThrowIfFailed(commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        m_deviceResources->WaitForGpu();
    }

    void TestFullscreen::Release()
    {
        m_scenePipelineState.Reset();
        m_sceneRootSignature.Reset();
        m_sceneVertexBuffer.Reset();
        m_sceneConstantBuffer.Release();
    }
}