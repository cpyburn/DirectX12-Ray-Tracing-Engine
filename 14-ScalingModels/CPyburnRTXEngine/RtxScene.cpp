#include "pchlib.h"
#include "RtxScene.h"

#include "BufferBlas.h"
#include "AnimationCompute.h"

#include "EntitiesManager.h"
#include "Entity.h"

namespace CPyburnRTXEngine
{
    static const WCHAR* kRayGenShader = L"rayGen";
    static const WCHAR* kMissShader = L"miss";
    static const WCHAR* kClosestHitShader = L"chs";
    static const WCHAR* kHitGroup = L"HitGroup";
    // 12.1.f
    static const WCHAR* kPlaneChs = L"planeChs";
    static const WCHAR* kPlaneHitGroup = L"PlaneHitGroup";
    // 13.2.a
    static const WCHAR* kShadowChs = L"shadowChs";
    static const WCHAR* kShadowMiss = L"shadowMiss";
    static const WCHAR* kShadowHitGroup = L"ShadowHitGroup";

    void RtxScene::CreateCommandObjects()
    {
        for (size_t i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[i])));
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[i].Get(), nullptr, IID_PPV_ARGS(&m_commandList[i])));

            if (i > 0)
            {
                m_commandList[i]->Close();
            }
        }

        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));
        m_fence->SetName(L"Test Instances Fence");

        m_fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
        if (!m_fenceEvent.IsValid())
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEventEx");
        }
    }

    void RtxScene::CreateBuffers()
    {
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(0, nullptr);

        // create createPlaneVB
        std::vector<XMFLOAT3> planeVertices(6);
        planeVertices[0] = XMFLOAT3(-100, -1, -2);
        planeVertices[1] = XMFLOAT3(100, -1, 100);
        planeVertices[2] = XMFLOAT3(-100, -1, 100);

        planeVertices[3] = XMFLOAT3(-100, -1, -2);
        planeVertices[4] = XMFLOAT3(100, -1, -2);
        planeVertices[5] = XMFLOAT3(100, -1, 100);

		m_planeVertexBuffer.CpuData = planeVertices;
		m_planeVertexBuffer.CreateOnDefaultHeap(commandList.Get(), L"Plane Buffer");

        // create model data
        // after all the buffers are created, create the shader resource views in the correct order for shader table
        m_modelDataPerInstanceBuffer.CpuData.resize(64); // arbitary number to start testing
        m_modelDataPerInstanceBuffer.CreateOnUploadHeap(L"ModelDataPerInstance Buffer");
        m_modelDataPerInstanceBuffer.CreateShaderResourceView(true); // t0 for rtx shader
        EntitiesManager::m_modelDataGpuMapped = m_modelDataPerInstanceBuffer.MappedData; // point to the gpu mapped data to skip unneeded iterating and updates

        // create textures AFTER the last shader views
        for (auto& unorderedModel : AssimpFactory::Models)
        {
            AssimpFactory::Model& model = unorderedModel.second;
            for (size_t texIndex = 0; texIndex < model.textures.size(); texIndex++)
            {
                std::string textureLocation = model.textures[texIndex];
                model.texturesHeap.push_back(Texture::LoadTextureHeap("..\\..\\Assets\\Models\\" + model.contentLocation + textureLocation, commandList.Get()));
            }

            // load the normal and the ORM
            std::string outExtension;
            std::string fileName = RemoveExtension(model.textures[0], outExtension) + "_NRM." + outExtension;
            model.texturesNrm.push_back(fileName);
            model.texturesHeapNrm.push_back(Texture::LoadTextureHeap("..\\..\\Assets\\Models\\" + model.contentLocation + fileName, commandList.Get()));

            fileName = RemoveExtension(model.textures[0], outExtension) + "_ORM." + outExtension;
            model.texturesOrm.push_back(fileName);
            model.texturesHeapOrm.push_back(Texture::LoadTextureHeap("..\\..\\Assets\\Models\\" + model.contentLocation + fileName, commandList.Get()));
        }

        // upload goes out of scope if we don't execute the command list and wait for the GPU to finish before exiting the function, so execute and wait here
        DX::ThrowIfFailed(commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        m_deviceResources->WaitForGpu();

        m_deviceResources->GetCurrentFrameResource()->ResetCommandList(0);

        // release any uploads that will not be used again
        //m_elfAnimated->ReleaseUploadResources(); // todo: chad
        //m_modelDataPerInstanceBuffer.ReleaseUploadResource();
		m_planeVertexBuffer.ReleaseUploadResource();
    }

    void RtxScene::createAccelerationStructures()
    {
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList = m_deviceResources->GetCurrentFrameResource()->GetCommandList(0);
        // Bottom Level AS
        {
            // Store the AS buffers. The rest of the buffers will be released once we exit the function
            m_planeBlas.InitBlas(m_deviceResources->GetD3DDevice(), 6, m_planeVertexBuffer.DefaultHeapResource, commandList.Get());
            m_planeBlas.UpdateBlas(commandList.Get());
        }

        // Top Level AS
        for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            RefitOrRebuildTLAS(commandList.Get(), i, false);

            // map the instances to the entities manager
            
        }

        // Close the resource creation command list and execute it to begin the vertex buffer copy into
        // the default heap.
        DX::ThrowIfFailed(commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        m_deviceResources->WaitForGpu();

        //m_deviceResources->GetCurrentFrameResource()->ResetCommandList(0, nullptr);
    }

    void RtxScene::RefitOrRebuildTLAS(ID3D12GraphicsCommandList4* commandList, UINT currentFrame, bool update)
    {
        // First, get the size of the TLAS buffers and create them
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        // 14.1.b
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        inputs.NumDescs = static_cast<UINT>(m_instanceData.size());
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
        m_deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        D3D12_RESOURCE_DESC m_bufDesc = {};
        m_bufDesc.Alignment = 0;
        m_bufDesc.DepthOrArraySize = 1;
        m_bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        // Create the buffers
        m_bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        m_bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        m_bufDesc.Height = 1;
        m_bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        m_bufDesc.MipLevels = 1;
        m_bufDesc.SampleDesc.Count = 1;
        m_bufDesc.SampleDesc.Quality = 0;
        m_bufDesc.Width = info.ScratchDataSizeInBytes;

        // 14.1.c
        if (update)
        {
            // If this a request for an update, then the TLAS was already used in a DispatchRay() call. We need a UAV barrier to make sure the read operation ends before updating the buffer
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = mpTopLevelAS[currentFrame].pResult.Get();
            commandList->ResourceBarrier(1, &uavBarrier);
        }
        else
        {
            auto heapPropertiesDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mpTopLevelAS[currentFrame].pScratch)));

            m_bufDesc.Width = info.ResultDataMaxSizeInBytes;
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&mpTopLevelAS[currentFrame].pResult)));
            mTlasSize = info.ResultDataMaxSizeInBytes;

            // The instance desc should be inside a buffer, create and map the buffer
            m_bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            m_bufDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * EntitiesManager::m_maxEntities;
            auto heapPropertiesUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&heapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpTopLevelAS[currentFrame].pInstanceDescResource)));
        
            // todo: no need to map and unmap since we write every frame
            mpTopLevelAS[currentFrame].pInstanceDescResource->Map(0, nullptr, (void**)&pInstanceDesc[currentFrame]);
            ZeroMemory(pInstanceDesc[currentFrame], sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT>(EntitiesManager::m_maxEntities));
            EntitiesManager::m_instanceDescGpuMapped[currentFrame] = pInstanceDesc[currentFrame];

            // plane
            pInstanceDesc[currentFrame][0].InstanceID = 0;
            pInstanceDesc[currentFrame][0].InstanceContributionToHitGroupIndex = 2;
            pInstanceDesc[currentFrame][0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            XMMATRIX transpose = XMMatrixTranspose(m_instanceData[0].world);
            memcpy(pInstanceDesc[currentFrame][0].Transform, &transpose, sizeof(pInstanceDesc[currentFrame][0].Transform));
            pInstanceDesc[currentFrame][0].AccelerationStructure = m_planeBlas.GetResult()->GetGPUVirtualAddress(); // plane blas
            pInstanceDesc[currentFrame][0].InstanceMask = 0xFF;
        }

        // plane
        //pInstanceDesc[currentFrame][0].InstanceID = 0;
        //pInstanceDesc[currentFrame][0].InstanceContributionToHitGroupIndex = 2;
        //pInstanceDesc[currentFrame][0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        //XMMATRIX transpose = XMMatrixTranspose(m_instanceData[0].world);
        //memcpy(pInstanceDesc[currentFrame][0].Transform, &transpose, sizeof(pInstanceDesc[currentFrame][0].Transform));
        //pInstanceDesc[currentFrame][0].AccelerationStructure = m_planeBlas.GetResult()->GetGPUVirtualAddress(); // plane blas
        //pInstanceDesc[currentFrame][0].InstanceMask = 0xFF;

        // model
        for (UINT i = 1; i < m_instanceData.size(); i++)
        {
            auto it = EntitiesManager::LoadedEntities.find(i);
            if (it != EntitiesManager::LoadedEntities.end())
            {
                Entity* entity = &it->second;

                //pInstanceDesc[currentFrame][i].InstanceID = i;                            // This value will be exposed to the shader via InstanceID()
                // 13.3.a
                //pInstanceDesc[currentFrame][i].InstanceContributionToHitGroupIndex = 0;  // The indices are relative to to the start of the hit-table entries specified in Raytrace()
                //pInstanceDesc[currentFrame][i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
                //XMMATRIX transpose = XMMatrixTranspose(m_instanceData[i].world);
                //memcpy(pInstanceDesc[currentFrame][i].Transform, &transpose, sizeof(pInstanceDesc[currentFrame][i].Transform));
                //pInstanceDesc[i].AccelerationStructure = entity->GetAssimpAnimations()->GetAnimationBlas()->GetResult()->GetGPUVirtualAddress(); // triangle blas
                EntitiesManager::m_instanceDescGpuMapped[currentFrame][i].AccelerationStructure = entity->GetAssimpFactoryModel()->GetBlasPtr()->GetResult()->GetGPUVirtualAddress(); // triangle blas
                //pInstanceDesc[currentFrame][i].InstanceMask = 0xFF;
            }
        }

        //EntitiesManager::m_instanceDescGpuMapped[currentFrame] = pInstanceDesc[currentFrame];

        // Unmap
        //mpTopLevelAS[currentFrame].pInstanceDescResource->Unmap(0, nullptr);

        // Create the TLAS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
        asDesc.Inputs = inputs;
        asDesc.Inputs.InstanceDescs = mpTopLevelAS[currentFrame].pInstanceDescResource->GetGPUVirtualAddress();
        asDesc.DestAccelerationStructureData = mpTopLevelAS[currentFrame].pResult->GetGPUVirtualAddress();
        asDesc.ScratchAccelerationStructureData = mpTopLevelAS[currentFrame].pScratch->GetGPUVirtualAddress();

        // 14.1.e If this is an update operation, set the source buffer and the perform_update flag
        if (update)
        {
            asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            asDesc.SourceAccelerationStructureData = mpTopLevelAS[currentFrame].pResult->GetGPUVirtualAddress();
        }

        commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

        // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = mpTopLevelAS[currentFrame].pResult.Get();
        commandList->ResourceBarrier(1, &uavBarrier);
    }

    void RtxScene::RefitOrRebuildTLASNext()
    {
        // Build the TLAS for the next frame while the current frame is being rendered.
        UINT currentFrame = m_deviceResources->GetCurrentFrameIndex();
        UINT nextFrame = (currentFrame + 1) % DX::DeviceResources::c_backBufferCount;

        auto task = Concurrency::create_task([this, nextFrame]()
            {
                WaitForFrameSlot(nextFrame);

                DX::ThrowIfFailed(m_commandAllocator[nextFrame]->Reset());
                DX::ThrowIfFailed(m_commandList[nextFrame]->Reset(m_commandAllocator[nextFrame].Get(), nullptr));

                RefitOrRebuildTLAS(m_commandList[nextFrame].Get(), nextFrame, true);

                DX::ThrowIfFailed(m_commandList[nextFrame]->Close());
                ID3D12CommandList* lists[] = { m_commandList[nextFrame].Get() };
                m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1, lists);
                // set a fence to check before we render the next frame to make sure the TLAS update is finished
                //m_deviceResources->WaitForGpu(); // we don't want to stall the CPU/GPU here, so we will check the fence in the main loop before rendering the next frame

                const UINT64 fenceValue = m_nextFenceValue.fetch_add(1);
                DX::ThrowIfFailed(m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), fenceValue));
                m_fenceValues[nextFrame].store(fenceValue);
            });
    }

    void RtxScene::WaitForFrameSlot(UINT frameIndex)
    {
        const UINT64 fenceValue = m_fenceValues[frameIndex].load();
        if (fenceValue != 0 && m_fence->GetCompletedValue() < fenceValue)
        {
            DX::ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent.Get()));
            WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);
        }
    }

    UINT RtxScene::GetReadyFrameIndex() const
    {
        const UINT current = m_deviceResources->GetCurrentFrameIndex();

        // Prefer current frame if ready.
        if (m_fence->GetCompletedValue() >= m_fenceValues[current].load())
            return current;

        // Fall back to the previous frame slot.
        const UINT prev = (current + DX::DeviceResources::c_backBufferCount - 1) % DX::DeviceResources::c_backBufferCount;
        if (m_fence->GetCompletedValue() >= m_fenceValues[prev].load())
            return prev;

        // If neither is ready, return current or handle the failure however you want.
        return prev;
    }

    void RtxScene::createRtPipelineState()
    {
        //  1 for the DXIL library
        //  1 for hit-group triangle
        //  1 for hit-group plane
        //  1 for shadow hit group
        //  2 for RayGen root-signature (root-signature and the subobject association)
        //  9.2.a 2 for hit-program root-signature (root-signature and the subobject association)
        //  13 2 for plane hit root-signature and association
        //  9.2.b 2 for miss-shader root-signature (signature and association)
        //  2 for shader config (shared between all programs. 1 for the config, 1 for association)
        //  1 for pipeline config
        //  1 for the global root signature

        D3D12_STATE_SUBOBJECT subobjects[12] = {};
        uint32_t index = 0;

#pragma region DXIL library
        // DXIL library
        std::wstring shaderFilePath = GetAssetFullPath(L"09-Shaders.hlsl");
        Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = GraphicsContexts::CompileDXRLibrary(shaderFilePath.c_str());

        const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kPlaneChs /* 12.3.e */, kClosestHitShader, kShadowMiss /* 12.3.b */, kShadowChs /* 12.3.b */ };

        std::vector<D3D12_EXPORT_DESC> exportDesc;
        std::vector<std::wstring> exportName;
        uint32_t entryPointCount = _countof(entryPoints);

        D3D12_STATE_SUBOBJECT stateSubobject{};
        D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
        exportName.resize(entryPointCount);
        exportDesc.resize(entryPointCount);
        if (shaderBlob)
        {
            dxilLibDesc.DXILLibrary.pShaderBytecode = shaderBlob->GetBufferPointer();
            dxilLibDesc.DXILLibrary.BytecodeLength = shaderBlob->GetBufferSize();
            dxilLibDesc.NumExports = entryPointCount;
            dxilLibDesc.pExports = exportDesc.data();

            for (uint32_t i = 0; i < entryPointCount; i++)
            {
                exportName[i] = entryPoints[i];
                exportDesc[i].Name = exportName[i].c_str();
                exportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
                exportDesc[i].ExportToRename = nullptr;
            }
        }
        stateSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        stateSubobject.pDesc = &dxilLibDesc;
        subobjects[index++] = stateSubobject;
#pragma endregion

#pragma region Hit group triangle
        D3D12_HIT_GROUP_DESC hitGroupDescTriangle = {};
        hitGroupDescTriangle.ClosestHitShaderImport = kClosestHitShader;
        hitGroupDescTriangle.HitGroupExport = kHitGroup;

        D3D12_STATE_SUBOBJECT hitGroupSubobjectTriangle = {};
        hitGroupSubobjectTriangle.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroupSubobjectTriangle.pDesc = &hitGroupDescTriangle;
        subobjects[index++] = hitGroupSubobjectTriangle;
#pragma endregion

#pragma region Hit group plane
        // Hit group /*12.1.c*/
        D3D12_HIT_GROUP_DESC hitGroupDescPlane = {};
        hitGroupDescPlane.ClosestHitShaderImport = kPlaneChs;
        hitGroupDescPlane.HitGroupExport = kPlaneHitGroup;

        D3D12_STATE_SUBOBJECT hitGroupSubobjectPlane = {};
        hitGroupSubobjectPlane.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroupSubobjectPlane.pDesc = &hitGroupDescPlane;
        subobjects[index++] = hitGroupSubobjectPlane;
#pragma endregion


        // 13.2.c Create the shadow-ray hit group
#pragma region Shadow ray hit group
        // 13.2.c Create the shadow-ray hit group
        D3D12_HIT_GROUP_DESC hitGroupDescShadow = {};
        hitGroupDescShadow.ClosestHitShaderImport = kShadowChs;
        hitGroupDescShadow.HitGroupExport = kShadowHitGroup;

        D3D12_STATE_SUBOBJECT hitGroupSubobjectShadow = {};
        hitGroupSubobjectShadow.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroupSubobjectShadow.pDesc = &hitGroupDescShadow;
        subobjects[index++] = hitGroupSubobjectShadow;
#pragma endregion

#pragma region Hit triangle root signature
        D3D12_ROOT_SIGNATURE_DESC descHit = {};
        std::vector<D3D12_DESCRIPTOR_RANGE> rangeHit;
        std::vector<D3D12_ROOT_PARAMETER> rootParamsHit;

        rangeHit.resize(1 + 1); // srv material + texture array (bindless)
        rootParamsHit.resize(1); // (srv + diffuseSRV)

        // SRV materials
        rangeHit[0].BaseShaderRegister = 0; // gOutput used the first t() register in the shader
        rangeHit[0].NumDescriptors = 1;
        rangeHit[0].RegisterSpace = 1;
        rangeHit[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeHit[0].OffsetInDescriptorsFromTableStart = 0; // this has to match the shader table entries
        // t4 - texture array (bindless)
        rangeHit[1].BaseShaderRegister = 1;
        rangeHit[1].NumDescriptors = 100; // todo: make it use cbv size
        rangeHit[1].RegisterSpace = 1;
        rangeHit[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeHit[1].OffsetInDescriptorsFromTableStart = 1; // this has to match the shader table entries

        // SRVs
        rootParamsHit[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParamsHit[0].DescriptorTable.NumDescriptorRanges = static_cast<UINT>(rangeHit.size());
        rootParamsHit[0].DescriptorTable.pDescriptorRanges = rangeHit.data();

        descHit.NumParameters = 1; // (srv vertex + srv index + diffuseSRV)
        descHit.pParameters = rootParamsHit.data();
        descHit.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // todo: move samplers
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        descHit.NumStaticSamplers = 1;
        descHit.pStaticSamplers = &sampler;

        // create ray-gen root-signature and associate it with the ray-gen shader
        Microsoft::WRL::ComPtr<ID3DBlob> pSigBlobHit;
        Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlobHit;
        HRESULT hrHit = D3D12SerializeRootSignature(&descHit, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobHit, &pErrorBlobHit);
        if (FAILED(hrHit))
        {
            if (pErrorBlobHit)
            {
                OutputDebugStringA((char*)pErrorBlobHit->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSigHit;
        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobHit->GetBufferPointer(), pSigBlobHit->GetBufferSize(), IID_PPV_ARGS(&pRootSigHit)));

        ID3D12RootSignature* pRootSigHitPtr = pRootSigHit.Get();
        subobjects[index].pDesc = &pRootSigHitPtr;
        subobjects[index++].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
#pragma endregion

#pragma region Hit triangle root associations
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationHit = {};
        associationHit.NumExports = 1;
        const WCHAR* exportsHit[] = { kClosestHitShader };
        associationHit.pExports = exportsHit;
        associationHit.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationHit;
#pragma endregion

#pragma region Empty root signature
        D3D12_ROOT_SIGNATURE_DESC emptyDescMiss = {};
        emptyDescMiss.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // create empty root-signature and associate it with the ray-gen shader
        Microsoft::WRL::ComPtr<ID3DBlob> pSigBlobEmptyMiss;
        Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlobEmptyMiss;
        HRESULT hrEmptyHitMiss = D3D12SerializeRootSignature(&emptyDescMiss, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobEmptyMiss, &pErrorBlobEmptyMiss);
        if (FAILED(hrEmptyHitMiss))
        {
            if (pErrorBlobEmptyMiss)
            {
                OutputDebugStringA((char*)pErrorBlobEmptyMiss->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSigEmptyMiss;
        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobEmptyMiss->GetBufferPointer(), pSigBlobEmptyMiss->GetBufferSize(), IID_PPV_ARGS(&pRootSigEmptyMiss)));

        ID3D12RootSignature* pRootSigEmptyPtrMiss = pRootSigEmptyMiss.Get();
        subobjects[index].pDesc = &pRootSigEmptyPtrMiss;
        subobjects[index++].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
#pragma endregion

#pragma region Empty root association
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationMiss = {};
        const WCHAR* emptyRootExport[] = { kMissShader, kShadowChs /* 13.2.d */, kShadowMiss /* 13.2.d */ };
        associationMiss.NumExports = _countof(emptyRootExport);
        associationMiss.pExports = emptyRootExport;
        associationMiss.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationMiss;
#pragma endregion

#pragma region Shader config
        // Shader config, shared between all shaders (ray-gen, miss and hit)
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
        shaderConfig.MaxPayloadSizeInBytes = sizeof(UINT) + sizeof(float) + sizeof(XMFLOAT3); // 12.0 hybrid, we now have a bool, a float, and color
        shaderConfig.MaxAttributeSizeInBytes = sizeof(XMFLOAT2); // Triangle hit attributes are barycentrics, which can be represented in 2 floats

        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        subobjects[index++].pDesc = &shaderConfig;
#pragma endregion

#pragma region Shader exports
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationMissChsRgs = {};
        const WCHAR* shaderExports[] = { kMissShader, kClosestHitShader, kPlaneChs /*12.1.e*/, kRayGenShader, kShadowMiss /* 13.2.e */, kShadowChs /* 13.2.e */ };
        associationMissChsRgs.NumExports = _countof(shaderExports);
        associationMissChsRgs.pExports = shaderExports;
        associationMissChsRgs.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationMissChsRgs;
#pragma endregion

#pragma region Pipeline config
        D3D12_RAYTRACING_PIPELINE_CONFIG config = {};
        config.MaxTraceRecursionDepth = 2;
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        subobjects[index++].pDesc = &config;
#pragma endregion

#pragma region Global root signature
        D3D12_DESCRIPTOR_RANGE globalRanges[4] = {};

        // u0 = output UAV
        globalRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        globalRanges[0].NumDescriptors = 1;
        globalRanges[0].BaseShaderRegister = 0;
        globalRanges[0].RegisterSpace = 0;
        globalRanges[0].OffsetInDescriptorsFromTableStart = 0;

        // t0 = TLAS SRV
        globalRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        globalRanges[1].NumDescriptors = 1;
        globalRanges[1].BaseShaderRegister = 0;
        globalRanges[1].RegisterSpace = 0;
        globalRanges[1].OffsetInDescriptorsFromTableStart = 0;

        // t1 = depth SRV
        globalRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        globalRanges[2].NumDescriptors = 1;
        globalRanges[2].BaseShaderRegister = 1;
        globalRanges[2].RegisterSpace = 0;
        globalRanges[2].OffsetInDescriptorsFromTableStart = 0;

        // t2 = raster SRV
        globalRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        globalRanges[3].NumDescriptors = 1;
        globalRanges[3].BaseShaderRegister = 2;
        globalRanges[3].RegisterSpace = 0;
        globalRanges[3].OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER globalParams[6] = {};

        // b0 camera
        globalParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        globalParams[0].Descriptor.ShaderRegister = 0;
        globalParams[0].Descriptor.RegisterSpace = 0;
        globalParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // b1 environment
        globalParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        globalParams[1].Descriptor.ShaderRegister = 1;
        globalParams[1].Descriptor.RegisterSpace = 0;
        globalParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // u0 table
        globalParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        globalParams[2].DescriptorTable.NumDescriptorRanges = 1;
        globalParams[2].DescriptorTable.pDescriptorRanges = &globalRanges[0];
        globalParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // t0 table
        globalParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        globalParams[3].DescriptorTable.NumDescriptorRanges = 1;
        globalParams[3].DescriptorTable.pDescriptorRanges = &globalRanges[1];
        globalParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // t1 table
        globalParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        globalParams[4].DescriptorTable.NumDescriptorRanges = 1;
        globalParams[4].DescriptorTable.pDescriptorRanges = &globalRanges[2];
        globalParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // t2 table
        globalParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        globalParams[5].DescriptorTable.NumDescriptorRanges = 1;
        globalParams[5].DescriptorTable.pDescriptorRanges = &globalRanges[3];
        globalParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC globalDesc = {};
        globalDesc.NumParameters = _countof(globalParams);
        globalDesc.pParameters = globalParams;
        globalDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

        Microsoft::WRL::ComPtr<ID3DBlob> pSigBlobGlobal;
        Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlobGlobal;
        HRESULT hrGlobal = D3D12SerializeRootSignature(
            &globalDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobGlobal, &pErrorBlobGlobal);

        if (FAILED(hrGlobal))
        {
            if (pErrorBlobGlobal) OutputDebugStringA((char*)pErrorBlobGlobal->GetBufferPointer());
            throw std::runtime_error("Failed to serialize global root signature");
        }

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateRootSignature(
                0,
                pSigBlobGlobal->GetBufferPointer(),
                pSigBlobGlobal->GetBufferSize(),
                IID_PPV_ARGS(&mpEmptyRootSig)));

        ID3D12RootSignature* pRootSigGlobalPtr = mpEmptyRootSig.Get();
        subobjects[index].pDesc = &pRootSigGlobalPtr;
        subobjects[index++].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
#pragma endregion


        // Create the state object
        D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
        stateObjectDesc.NumSubobjects = _countof(subobjects);
        stateObjectDesc.pSubobjects = subobjects;
        stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&mpPipelineState)));
    }

    uint8_t* RtxScene::shaderTableEntryHelper(UINT entry, ID3D12StateObjectProperties* pRtsoProps, uint8_t* pData, const WCHAR* exportName, const Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
    {
        uint8_t* pDataEntry = pData + (entry * mShaderTableEntrySize);
        memcpy(pDataEntry, pRtsoProps->GetShaderIdentifier(exportName), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        if (resource)
        {
            // ensure 8-byte alignment for root descriptor data per spec
            uint8_t* dataPtr = pDataEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
            assert(((uintptr_t)dataPtr % 8) == 0);
            *(D3D12_GPU_VIRTUAL_ADDRESS*)(pDataEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = resource->GetGPUVirtualAddress();
        }

        return pDataEntry;
    }

    void RtxScene::createShaderTable()
    {
        const size_t kShaderId = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        //const size_t kCbvSize = sizeof(D3D12_GPU_VIRTUAL_ADDRESS); // 8
        const size_t kSrvSize = sizeof(uint64_t); // descriptor pointer you store
        mShaderTableEntrySize = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, static_cast<uint32_t>(kShaderId + (kSrvSize * 3 /*vertexSRV indexSRV and diffuseSRV*/)));
        uint32_t shaderTableSize = mShaderTableEntrySize * 7;

        // For simplicity, we create the shader-table on the upload heap. You can also create it on the default heap
        D3D12_RESOURCE_DESC m_bufDesc = {};
        m_bufDesc.Alignment = 0;
        m_bufDesc.DepthOrArraySize = 1;
        m_bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        m_bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        m_bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        m_bufDesc.Height = 1;
        m_bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        m_bufDesc.MipLevels = 1;
        m_bufDesc.SampleDesc.Count = 1;
        m_bufDesc.SampleDesc.Quality = 0;
        m_bufDesc.Width = shaderTableSize;

        auto heapPropertiesUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&heapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &m_bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpShaderTable)));

        // Map the buffer
        uint8_t* pData;
        DX::ThrowIfFailed(mpShaderTable->Map(0, nullptr, (void**)&pData));

        Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pRtsoProps;
        DX::ThrowIfFailed(mpPipelineState->QueryInterface(IID_PPV_ARGS(&pRtsoProps)));

        // Entry 0 - ray-gen program ID and descriptor data MOVED TO GLOBAL ROOT SIGNATURE
        uint8_t* pEntry0 = shaderTableEntryHelper(0, pRtsoProps.Get(), pData, kRayGenShader);
        //*(uint64_t*)(pEntry0 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = GraphicsContexts::GetGpuHandle(mUavPosition).ptr; // heapStart + GraphicsContexts::c_descriptorSize * mUavPosition;

        // Entry 1 - primary ray miss
        shaderTableEntryHelper(1, pRtsoProps.Get(), pData, kMissShader);

        // Entry 2 - shadow-ray miss
        shaderTableEntryHelper(2, pRtsoProps.Get(), pData, kShadowMiss);

        // Entry 3 - Triangle 0, primary ray. ProgramID and constant-buffer data
        //uint8_t* pEntry3 = shaderTableEntryHelper(3, pRtsoProps.Get(), pData, kHitGroup);
        //*(uint64_t*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + kSrvSize * 0) = GraphicsContexts::GetGpuHandle(mVertexBufferSrvPosition).ptr; //heapStart + GraphicsContexts::c_descriptorSize * mVertexBufferSrvPosition; // The SRV
        //*(uint64_t*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + kSrvSize * 1) = GraphicsContexts::GetGpuHandle(mIndexBufferSrvPosition).ptr; //heapStart + GraphicsContexts::c_descriptorSize * mVertexBufferSrvPosition; // The SRV
        //*(uint64_t*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + kSrvSize * 2) = GraphicsContexts::GetGpuHandle(m_heapTextureDiffuse.heapPosition).ptr; //heapStart + GraphicsContexts::c_descriptorSize * mVertexBufferSrvPosition; // The SRV
        uint8_t* pEntry3 = shaderTableEntryHelper(3, pRtsoProps.Get(), pData, kHitGroup);
        //*(D3D12_GPU_DESCRIPTOR_HANDLE*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = m_triangleVertexBuffer.GpuHandle;
        *(D3D12_GPU_DESCRIPTOR_HANDLE*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = m_modelDataPerInstanceBuffer.GpuHandle;

        // Entry 4 - Triangle 0, shadow ray. ProgramID only
        shaderTableEntryHelper(4, pRtsoProps.Get(), pData, kShadowHitGroup);

        // Entry 5 - Plane, primary ray. ProgramID only and the TLAS SRV
        uint8_t* pEntry5 = shaderTableEntryHelper(5, pRtsoProps.Get(), pData, kPlaneHitGroup);
        //*(uint64_t*)(pEntry5 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = GraphicsContexts::GetGpuHandle(mTlasSrvPosition[0]).ptr; //heapStart + GraphicsContexts::c_descriptorSize * mSrvPosition[0];

        // Entry 6 - Plane, shadow ray
        shaderTableEntryHelper(6, pRtsoProps.Get(), pData, kShadowHitGroup);

        // Unmap
        mpShaderTable->Unmap(0, nullptr);
    }

    // todo: handle this in a more elegant way when we implement resizing
    void RtxScene::createShaderResources()
    {
        createShaderResourcesForWindowSize();

        for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            // 6.1 Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS[i].pResult->GetGPUVirtualAddress();

            m_deviceResources->GetD3DDevice()->CreateShaderResourceView(nullptr, &srvDesc, GraphicsContexts::GetCpuHandle(mTlasSrvPosition[i]));
        }
    }

    void RtxScene::createShaderResourcesForWindowSize()
    {
        // Create the output resource. The dimensions and format should match the swap-chain
        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.DepthOrArraySize = 1;
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB formats can't be used with UAVs. We will convert to sRGB ourselves in the shader
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resDesc.Width = std::max<UINT>(static_cast<UINT>(m_deviceResources->GetResolution().Width), 1u);
        resDesc.Height = std::max<UINT>(static_cast<UINT>(m_deviceResources->GetResolution().Height), 1u);
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;

        auto heapPropertiesDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&mpOutputResource))); // Starting as copy-source to simplify onFrameRender()

        // Create the UAV. Based on the root signature we created it should be the first entry
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        m_deviceResources->GetD3DDevice()->CreateUnorderedAccessView(mpOutputResource.Get(), nullptr, &uavDesc, GraphicsContexts::GetCpuHandle(mUavPosition));
    }

    RtxScene::RtxScene()
    {

    }

    RtxScene::~RtxScene()
    {
        Release();
    }

    void RtxScene::CreateDeviceDependentResources(DX::DeviceResources* deviceResources)
    {
        m_deviceResources = deviceResources;

        m_environment.CreateDeviceDependentResources(deviceResources);

        // reserve the uav position and srv position
        mUavPosition = GraphicsContexts::GetAvailableHeapPosition();
        for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            mTlasSrvPosition[i] = GraphicsContexts::GetAvailableHeapPosition();
        }

        m_modelDataPerInstanceBuffer.CreateDeviceDependentResources(deviceResources->GetD3DDevice());
		m_planeVertexBuffer.CreateDeviceDependentResources(deviceResources->GetD3DDevice());

        m_instanceData.reserve(64);
        m_instanceData.resize(3); // 1 at least for plane
        m_instanceData[0].world = XMMatrixIdentity();
        m_instanceData[1].world = XMMatrixIdentity();
        m_instanceData[2].world = XMMatrixIdentity() * XMMatrixTranslation(1, 0, 0);

        CreateCommandObjects();
        CreateBuffers();
        createAccelerationStructures(); // Tutorial 03
        createRtPipelineState(); // Tutorial 04
        createShaderResources(); // Tutorial 06. Need to do this before initializing the shader-table
        createShaderTable(); // Tutorial 05
    }

    void RtxScene::CreateWindowSizeDependentResources()
    {
        createShaderResourcesForWindowSize();
    }

    void RtxScene::Update(DX::StepTimer const& timer, CameraBase* camera)
    {
        m_environment.Update(timer, camera);
    }

    void RtxScene::Render(CameraBase* camera)
    {

#pragma region Testing Bounding Sphere
        ID3D12GraphicsCommandList4* m_sceneCommandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_SCENE_0, GraphicsContexts::GetPipelinePositionColorInstancedLine());

        ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get() };
        m_sceneCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        auto viewPort = m_deviceResources->GetScreenViewport();
        m_sceneCommandList->RSSetViewports(1, &viewPort);
        auto scissorRect = m_deviceResources->GetScissorRect();
        m_sceneCommandList->RSSetScissorRects(1, &scissorRect);

        const CD3DX12_CPU_DESCRIPTOR_HANDLE& rtvHandle = m_deviceResources->GetIntermediateRenderTargetViewCpu();
        auto depthStencilView = m_deviceResources->GetDepthStencilView();
        m_sceneCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &depthStencilView);

        // Record commands.
        m_sceneCommandList->ClearRenderTargetView(rtvHandle, m_deviceResources->GetClearColor(), 0, nullptr);
        m_sceneCommandList->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

        PIXBeginEvent(m_sceneCommandList, 0, L"Draw rasterized geom");

#ifdef _DEBUG
        BoundingRendererParent::RenderBegin(m_sceneCommandList, camera);

        /*m_elfAnimated->GetAssimpFactory()->GetBoundingBoxRenderer().Render(m_sceneCommandList);
        m_elfAnimated->GetAssimpFactory()->GetBoundingSphereRenderer().Render(m_sceneCommandList);*/
        m_entitiesManagerPtr->RenderBounding(m_sceneCommandList);
#else

#endif

        PIXEndEvent(m_sceneCommandList);

        //DX::ThrowIfFailed(m_sceneCommandList->Close());
#pragma endregion

#pragma region Ray tracing

        D3D12_RESOURCE_BARRIER depthToSrv = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_sceneCommandList->ResourceBarrier(1, &depthToSrv);

        //ID3D12GraphicsCommandList4* m_sceneCommandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_SCENE_0, nullptr);

        //m_elfAnimated->GetAnimationCompute()->Dispatch(m_sceneCommandList);
        //{
        //    D3D12_RESOURCE_BARRIER uavBarrier = {};
        //    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        //    uavBarrier.UAV.pResource =
        //        m_elfAnimated->GetAnimationCompute()->GetVertexOutputBuffer().DefaultHeapResource.Get();

        //    m_sceneCommandList->ResourceBarrier(1, &uavBarrier);
        //}

        //m_elfAnimated->GetAnimationBlas()->UpdateDynamicBlas(m_sceneCommandList);

        m_entitiesManagerPtr->DispatchAndUpdateBlas(m_sceneCommandList);

        // Populate m_sceneCommandList to render scene to intermediate render target.
        {
            PIXBeginEvent(m_sceneCommandList, 0, L"TestModel.");

            // refitting
            {
                //if (!m_TlasUpdated[m_deviceResources->GetCurrentFrameIndex()])
                //{
                //    RefitOrRebuildTLAS(m_sceneCommandList, m_deviceResources->GetCurrentFrameIndex(), true); // if the next one is not ready, fall back here, but if this happens, you should consider a different architecture as it means you are not able to keep up with the updates
                //}

                //m_deviceResources->WaitForGpu();
                RefitOrRebuildTLAS(m_sceneCommandList, m_deviceResources->GetCurrentFrameIndex(), true);

                //ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get() };
                //m_sceneCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get() };
                m_sceneCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                D3D12_RESOURCE_BARRIER barriers[2] =
                {
                    CD3DX12_RESOURCE_BARRIER::Transition(mpOutputResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetIntermediateRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                };

                // only use the first resource barrier to transition the output resource, the second one is used later
                m_sceneCommandList->ResourceBarrier(2, &barriers[0]);

                D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
                raytraceDesc.Width = std::max<UINT>(m_deviceResources->GetResolution().Width, 1u);
                raytraceDesc.Height = std::max<UINT>(m_deviceResources->GetResolution().Height, 1u);
                raytraceDesc.Depth = 1;

                // 6.4.b RayGen is the first entry in the shader-table
                raytraceDesc.RayGenerationShaderRecord.StartAddress = mpShaderTable->GetGPUVirtualAddress() + 0 * mShaderTableEntrySize;
                raytraceDesc.RayGenerationShaderRecord.SizeInBytes = mShaderTableEntrySize;

                // 6.4.c Miss is the second entry in the shader-table
                size_t missOffset = 1 * mShaderTableEntrySize;
                raytraceDesc.MissShaderTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + missOffset;
                raytraceDesc.MissShaderTable.StrideInBytes = mShaderTableEntrySize;
                raytraceDesc.MissShaderTable.SizeInBytes = mShaderTableEntrySize * 2;   // 13.3.b 2 miss-entries

                // 6.4.d Hit is the third entry in the shader-table
                size_t hitOffset = 3 * mShaderTableEntrySize; // 13.3.c
                raytraceDesc.HitGroupTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + hitOffset;
                raytraceDesc.HitGroupTable.StrideInBytes = mShaderTableEntrySize;
                // 12.3.d
                raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize * 4;    // 13.3.d 8 hit-entries

                // 6.4.e Bind the empty root signature
                m_sceneCommandList->SetComputeRootSignature(mpEmptyRootSig.Get());
                m_sceneCommandList->SetComputeRootConstantBufferView(0, camera->GetCbv()->GetGPUVirtualAddressBuffered(camera->GetDeviceResources()->GetCurrentFrameIndex()));
                m_sceneCommandList->SetComputeRootConstantBufferView(1, m_environment.GetEnvironmentConstantBuffer().Resource->GetGPUVirtualAddress());
                m_sceneCommandList->SetComputeRootDescriptorTable(2, GraphicsContexts::GetGpuHandle(mUavPosition));
                UINT tlasFrame = GetReadyFrameIndex();
                m_sceneCommandList->SetComputeRootDescriptorTable(3, GraphicsContexts::GetGpuHandle(mTlasSrvPosition[tlasFrame]));
                m_sceneCommandList->SetComputeRootDescriptorTable(4, m_deviceResources->GetSrvDepthStencilGpu());
                m_sceneCommandList->SetComputeRootDescriptorTable(5, m_deviceResources->GetSrvIntermediateGpu());

                // 6.4.f Set Pipeline
                m_sceneCommandList->SetPipelineState1(mpPipelineState.Get());

                // 6.4.g Dispatch
                m_sceneCommandList->DispatchRays(&raytraceDesc);

                D3D12_RESOURCE_BARRIER depthToWrite = CD3DX12_RESOURCE_BARRIER::Transition(
                    m_deviceResources->GetDepthStencil(),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_DEPTH_WRITE);

                m_sceneCommandList->ResourceBarrier(1, &depthToWrite);

                barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

                m_sceneCommandList->ResourceBarrier(2, &barriers[0]);
                m_sceneCommandList->CopyResource(m_deviceResources->GetIntermediateRenderTarget(), mpOutputResource.Get());

                barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

                // transition the intermediate back to render target
                m_sceneCommandList->ResourceBarrier(1, &barriers[1]);

                PIXEndEvent(m_sceneCommandList);

                DX::ThrowIfFailed(m_sceneCommandList->Close());
            }
        }
#pragma endregion
    }
    

    void RtxScene::Release()
    {
        m_planeBlas.Release();
        mpPipelineState.Reset();
        mpEmptyRootSig.Reset();
        mpShaderTable.Reset();
        mpOutputResource.Reset();

        GraphicsContexts::RemoveHeapPosition(mUavPosition);
        for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
        {
            GraphicsContexts::RemoveHeapPosition(mTlasSrvPosition[i]);
        }
    }
}
