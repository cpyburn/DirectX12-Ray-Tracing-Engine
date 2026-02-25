#include "pchlib.h"
#include "TestTriangle.h"

#include "GraphicsContexts.h"
#include "FrameResource.h"

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

    static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
    {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0,
    };

    static const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
    {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    // 15.5.a
    struct TriVertex
    {
        XMFLOAT3 vertex;
        XMFLOAT4 color;
    };


    void TestTriangle::createAccelerationStructures()
    {
        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Alignment = 0;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.Height = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;

        // create createTriangleVB
        {
            const TriVertex vertices[] =
            {
                XMFLOAT3(0,          1,  0), XMFLOAT4(1, 0, 0, 1),
                XMFLOAT3(0.866f,  -0.5f, 0), XMFLOAT4(1, 1, 0, 1),
                XMFLOAT3(-0.866f, -0.5f, 0), XMFLOAT4(1, 0, 1, 1),
            };
            bufDesc.Width = sizeof(vertices);

            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpVertexBuffer)));

            // For simplicity, we create the vertex buffer on the upload heap, but that's not required
            uint8_t* pData;
            mpVertexBuffer->Map(0, nullptr, (void**)&pData);
            memcpy(pData, vertices, sizeof(vertices));
            mpVertexBuffer->Unmap(0, nullptr);
        }

        // create createPlaneVB
        {
            const XMFLOAT3 vertices[] =
            {
                XMFLOAT3(-100, -1,  -2),
                XMFLOAT3(100, -1,  100),
                XMFLOAT3(-100, -1,  100),

                XMFLOAT3(-100, -1,  -2),
                XMFLOAT3(100, -1,  -2),
                XMFLOAT3(100, -1,  100),
            };
            bufDesc.Width = sizeof(vertices);

            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpVertexBuffer1)));

            // For simplicity, we create the vertex buffer on the upload heap, but that's not required
            uint8_t* pData;
            mpVertexBuffer1->Map(0, nullptr, (void**)&pData);
            memcpy(pData, vertices, sizeof(vertices));
            mpVertexBuffer1->Unmap(0, nullptr);
        }


        // Single-use command allocator and command list for creating resources.
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12GraphicsCommandList4> commandList;

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

        // Bottom Level AS
        {
            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescVector;
            geomDescVector.resize(2);

            D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
            geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDesc.Triangles.VertexBuffer.StartAddress = mpVertexBuffer->GetGPUVirtualAddress(); // triangle 
            geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(TriVertex);
            geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            geomDesc.Triangles.VertexCount = 3;
            geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            // add triangle
            geomDescVector[0] = geomDesc; // triangle

            // add plane
            geomDesc.Triangles.VertexBuffer.StartAddress = mpVertexBuffer1->GetGPUVirtualAddress(); // plane
            geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
            geomDesc.Triangles.VertexCount = 6; // plane
            geomDescVector[1] = geomDesc; // plane

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = static_cast<UINT>(geomDescVector.size());
            //inputs.pGeometryDescs = &geomDesc;
            inputs.pGeometryDescs = geomDescVector.data();
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
            m_deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bufDesc.Width = info.ScratchDataSizeInBytes;

            ComPtr<ID3D12Resource> pScratch;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pScratch)));

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            ComPtr<ID3D12Resource> pResult;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));

            // Create the bottom-level AS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
            asDesc.Inputs = inputs;
            asDesc.DestAccelerationStructureData = pResult->GetGPUVirtualAddress();
            asDesc.ScratchAccelerationStructureData = pScratch->GetGPUVirtualAddress();

            commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

            // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = pResult.Get();
            commandList->ResourceBarrier(1, &uavBarrier);

            // Close the resource creation command list and execute it to begin the vertex buffer copy into
            // the default heap.
            ThrowIfFailed(commandList->Close());
            ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
            m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            m_deviceResources->WaitForGpu();

            // Store the AS buffers. The rest of the buffers will be released once we exit the function
            mpBottomLevelAS = pResult;
        }

        ThrowIfFailed(commandAllocator->Reset());
        ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

        {
            D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
            geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDesc.Triangles.VertexBuffer.StartAddress = mpVertexBuffer->GetGPUVirtualAddress();
            geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(TriVertex);
            geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            geomDesc.Triangles.VertexCount = 3;
            geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = 1;
            inputs.pGeometryDescs = &geomDesc;
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
            m_deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bufDesc.Width = info.ScratchDataSizeInBytes;

            ComPtr<ID3D12Resource> pScratch;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pScratch)));

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            ComPtr<ID3D12Resource> pResult;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));

            // Create the bottom-level AS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
            asDesc.Inputs = inputs;
            asDesc.DestAccelerationStructureData = pResult->GetGPUVirtualAddress();
            asDesc.ScratchAccelerationStructureData = pScratch->GetGPUVirtualAddress();

            commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

            // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = pResult.Get();
            commandList->ResourceBarrier(1, &uavBarrier);

            // Close the resource creation command list and execute it to begin the vertex buffer copy into
            // the default heap.
            ThrowIfFailed(commandList->Close());
            ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
            m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            m_deviceResources->WaitForGpu();

            // Store the AS buffers. The rest of the buffers will be released once we exit the function
            mpBottomLevelAS1 = pResult;
        }

        ThrowIfFailed(commandAllocator->Reset());
        ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

        // Top Level AS
        for (UINT i = 0; i < DeviceResources::c_backBufferCount; i++)
        {
            RefitOrRebuildTLAS(commandList.Get(), i, false);
        }

        // Close the resource creation command list and execute it to begin the vertex buffer copy into
        // the default heap.
        ThrowIfFailed(commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { commandList.Get()};
        m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        m_deviceResources->WaitForGpu();
    }

    void TestTriangle::RefitOrRebuildTLAS(ID3D12GraphicsCommandList4* commandList, UINT currentFrame, bool update)
    {
        // First, get the size of the TLAS buffers and create them
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        // 14.1.b
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        inputs.NumDescs = countOfConstantBuffers; // 3 instances
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
        m_deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Alignment = 0;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        // Create the buffers
        bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.Height = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Width = info.ScratchDataSizeInBytes;

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
            //ComPtr<ID3D12Resource> pScratch;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mpTopLevelAS[currentFrame].pScratch)));

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            //ComPtr<ID3D12Resource> pResult;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&mpTopLevelAS[currentFrame].pResult)));
            mTlasSize = info.ResultDataMaxSizeInBytes;

            // The instance desc should be inside a buffer, create and map the buffer
            bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            bufDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * countOfConstantBuffers; // 3 instances

            //ComPtr<ID3D12Resource> pInstanceDescResource;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpTopLevelAS[currentFrame].pInstanceDescResource)));
        }

        D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
        mpTopLevelAS[currentFrame].pInstanceDescResource->Map(0, nullptr, (void**)&pInstanceDesc);
        ZeroMemory(pInstanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * countOfConstantBuffers); // 3 instances

        // 11.3.b Create the desc for the triangle/plane instance
        pInstanceDesc[0].InstanceID = 0;
        pInstanceDesc[0].InstanceContributionToHitGroupIndex = 0;
        pInstanceDesc[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        XMMATRIX transpose = XMMatrixTranspose(m_xmIdentity[0]);
        memcpy(pInstanceDesc[0].Transform, &transpose, sizeof(pInstanceDesc[0].Transform));
        pInstanceDesc[0].AccelerationStructure = mpBottomLevelAS->GetGPUVirtualAddress(); // plane blas
        pInstanceDesc[0].InstanceMask = 0xFF;

        // 8.0.d
        for (UINT i = 1; i < countOfConstantBuffers; i++)
        {
            pInstanceDesc[i].InstanceID = i;                            // This value will be exposed to the shader via InstanceID()
            // 13.3.a
            pInstanceDesc[i].InstanceContributionToHitGroupIndex = (i * 2) + 2;  // The indices are relative to to the start of the hit-table entries specified in Raytrace(), so we need 4 and 6
            pInstanceDesc[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            XMMATRIX transpose = XMMatrixTranspose(m_xmIdentity[i]);
            memcpy(pInstanceDesc[i].Transform, &transpose, sizeof(pInstanceDesc[i].Transform));
            pInstanceDesc[i].AccelerationStructure = mpBottomLevelAS1->GetGPUVirtualAddress(); // triangle blas
            pInstanceDesc[i].InstanceMask = 0xFF;
        }

        // Unmap
        mpTopLevelAS[currentFrame].pInstanceDescResource->Unmap(0, nullptr);

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

    void TestTriangle::createRtPipelineState()
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

        D3D12_STATE_SUBOBJECT subobjects[16] = {};
        uint32_t index = 0;

#pragma region DXIL library
        // DXIL library
        std::wstring shaderFilePath = GetAssetFullPath(L"04-Shaders.hlsl");
        ComPtr<IDxcBlob> shaderBlob = CompileDXRLibrary(shaderFilePath.c_str());

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


#pragma region Ray Root Signature
        // Create the root-signature
        D3D12_ROOT_SIGNATURE_DESC descRay{};
        std::vector<D3D12_DESCRIPTOR_RANGE> rangeRay;
        std::vector<D3D12_ROOT_PARAMETER> rootParamsRay;

        rangeRay.resize(2);
        // gOutput
        rangeRay[0].BaseShaderRegister = 0;
        rangeRay[0].NumDescriptors = 1;
        rangeRay[0].RegisterSpace = 0;
        rangeRay[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        rangeRay[0].OffsetInDescriptorsFromTableStart = 0;

        // gRtScene
        rangeRay[1].BaseShaderRegister = 0;
        rangeRay[1].NumDescriptors = 1;
        rangeRay[1].RegisterSpace = 0;
        rangeRay[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeRay[1].OffsetInDescriptorsFromTableStart = 1;

        rootParamsRay.resize(1);
        rootParamsRay[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParamsRay[0].DescriptorTable.NumDescriptorRanges = 2;
        rootParamsRay[0].DescriptorTable.pDescriptorRanges = rangeRay.data();

        // Create the desc
        descRay.NumParameters = 1;
        descRay.pParameters = rootParamsRay.data();
        descRay.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // create ray-gen root-signature and associate it with the ray-gen shader
        ComPtr<ID3DBlob> pSigBlobRay;
        ComPtr<ID3DBlob> pErrorBlobRay;
        HRESULT hrRay = D3D12SerializeRootSignature(&descRay, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobRay, &pErrorBlobRay);
        if (FAILED(hrRay))
        {
            if (pErrorBlobRay)
            {
                OutputDebugStringA((char*)pErrorBlobRay->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        ComPtr<ID3D12RootSignature> pRootSigRay;
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobRay->GetBufferPointer(), pSigBlobRay->GetBufferSize(), IID_PPV_ARGS(&pRootSigRay)));

        ID3D12RootSignature* pRootSigRayPtr = pRootSigRay.Get();
        subobjects[index].pDesc = &pRootSigRayPtr;
        subobjects[index++].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
#pragma endregion

#pragma region Ray root associations
        // Associate the ray-gen root signature with the ray-gen shader
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationRay = {};
        associationRay.NumExports = 1;
        associationRay.pExports = &kRayGenShader;
        associationRay.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index] = {};
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationRay;
#pragma endregion

#pragma region Hit triangle root signature
        D3D12_ROOT_SIGNATURE_DESC descHit = {};
        std::vector<D3D12_DESCRIPTOR_RANGE> rangeHit;
        std::vector<D3D12_ROOT_PARAMETER> rootParamsHit;

        rootParamsHit.resize(1 + 1); // cbv + srv
        // CBV
        rootParamsHit[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParamsHit[0].Descriptor.RegisterSpace = 0;
        rootParamsHit[0].Descriptor.ShaderRegister = 0;
        // SRV
        rangeHit.resize(1); // srv
        rangeHit[0].BaseShaderRegister = 1; // gOutput used the first t() register in the shader
        rangeHit[0].NumDescriptors = 1;
        rangeHit[0].RegisterSpace = 0;
        rangeHit[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeHit[0].OffsetInDescriptorsFromTableStart = 0;
        // SRV
        rootParamsHit[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParamsHit[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParamsHit[1].DescriptorTable.pDescriptorRanges = rangeHit.data();

        descHit.NumParameters = 1 + 1; // cbv + srv
        descHit.pParameters = rootParamsHit.data();
        descHit.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // create ray-gen root-signature and associate it with the ray-gen shader
        ComPtr<ID3DBlob> pSigBlobHit;
        ComPtr<ID3DBlob> pErrorBlobHit;
        HRESULT hrHit = D3D12SerializeRootSignature(&descHit, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobHit, &pErrorBlobHit);
        if (FAILED(hrHit))
        {
            if (pErrorBlobHit)
            {
                OutputDebugStringA((char*)pErrorBlobHit->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        ComPtr<ID3D12RootSignature> pRootSigHit;
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobHit->GetBufferPointer(), pSigBlobHit->GetBufferSize(), IID_PPV_ARGS(&pRootSigHit)));

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
        

        // 13.2.g Create the plane hit root-signature and association
#pragma region Plane hit root signature
        D3D12_ROOT_SIGNATURE_DESC descPlaneHit = {};
        std::vector<D3D12_DESCRIPTOR_RANGE> rangePlaneHit;
        std::vector<D3D12_ROOT_PARAMETER> rootParamsPlaneHit;

        rangePlaneHit.resize(1);
        rangePlaneHit[0].BaseShaderRegister = 0;
        rangePlaneHit[0].NumDescriptors = 1;
        rangePlaneHit[0].RegisterSpace = 0;
        rangePlaneHit[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangePlaneHit[0].OffsetInDescriptorsFromTableStart = 0;

        rootParamsPlaneHit.resize(1);
        rootParamsPlaneHit[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParamsPlaneHit[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParamsPlaneHit[0].DescriptorTable.pDescriptorRanges = rangePlaneHit.data();

        descPlaneHit.NumParameters = 1;
        descPlaneHit.pParameters = rootParamsPlaneHit.data();
        descPlaneHit.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // create ray-gen root-signature and associate it with the ray-gen shader
        ComPtr<ID3DBlob> pSigBlobPlaneHit;
        ComPtr<ID3DBlob> pErrorBlobPlaneHit;
        HRESULT hrPlaneHit = D3D12SerializeRootSignature(&descPlaneHit, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobPlaneHit, &pErrorBlobPlaneHit);
        if (FAILED(hrPlaneHit))
        {
            if (pErrorBlobPlaneHit)
            {
                OutputDebugStringA((char*)pErrorBlobPlaneHit->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        ComPtr<ID3D12RootSignature> pRootSigPlaneHit;
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobPlaneHit->GetBufferPointer(), pSigBlobPlaneHit->GetBufferSize(), IID_PPV_ARGS(&pRootSigPlaneHit)));

        ID3D12RootSignature* pRootSigPlaneHitPtr = pRootSigPlaneHit.Get();
        subobjects[index].pDesc = &pRootSigPlaneHitPtr;
        subobjects[index++].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
#pragma endregion

#pragma region plane PlaneHit root associations
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationPlaneHit = {};
            associationPlaneHit.NumExports = 1;
            const WCHAR* exportsPlaneHit[] = { kPlaneHitGroup };
            associationPlaneHit.pExports = exportsPlaneHit;
            associationPlaneHit.pSubobjectToAssociate = &subobjects[index - 1];
            subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
            subobjects[index++].pDesc = &associationPlaneHit;
#pragma endregion
        


#pragma region Empty root signature
        D3D12_ROOT_SIGNATURE_DESC emptyDescMiss = {};
        emptyDescMiss.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // create empty root-signature and associate it with the ray-gen shader
        ComPtr<ID3DBlob> pSigBlobEmptyMiss;
        ComPtr<ID3DBlob> pErrorBlobEmptyMiss;
        HRESULT hrEmptyHitMiss = D3D12SerializeRootSignature(&emptyDescMiss, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobEmptyMiss, &pErrorBlobEmptyMiss);
        if (FAILED(hrEmptyHitMiss))
        {
            if (pErrorBlobEmptyMiss)
            {
                OutputDebugStringA((char*)pErrorBlobEmptyMiss->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        ComPtr<ID3D12RootSignature> pRootSigEmptyMiss;
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobEmptyMiss->GetBufferPointer(), pSigBlobEmptyMiss->GetBufferSize(), IID_PPV_ARGS(&pRootSigEmptyMiss)));

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
        shaderConfig.MaxPayloadSizeInBytes = sizeof(XMFLOAT3); // We only need to output a color
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

#pragma region Global root signature, required always
        // Global root signature. We don't actually use it in this sample, but it's required to create the pipeline state object
        D3D12_ROOT_SIGNATURE_DESC emptyDescGlobal = {};
        emptyDescGlobal.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> pSigBlobGlobal;
        ComPtr<ID3DBlob> pErrorBlobGlobal;
        HRESULT hrGlobal = D3D12SerializeRootSignature(&emptyDescGlobal, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobGlobal, &pErrorBlobGlobal);
        if (FAILED(hrGlobal))
        {
            if (pErrorBlobGlobal)
            {
                OutputDebugStringA((char*)pErrorBlobGlobal->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobGlobal->GetBufferPointer(), pSigBlobGlobal->GetBufferSize(), IID_PPV_ARGS(&mpEmptyRootSig)));

        ID3D12RootSignature* pRootSigGlobalPtr = mpEmptyRootSig.Get();
        subobjects[index].pDesc = &pRootSigGlobalPtr;
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
#pragma endregion

        // Create the state object
        D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
        stateObjectDesc.NumSubobjects = _countof(subobjects);
        stateObjectDesc.pSubobjects = subobjects;
        stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&mpPipelineState)));
    }

    std::vector<uint8_t> TestTriangle::LoadBinaryFile(const wchar_t* path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Failed to open file");

        size_t size = (size_t)file.tellg();
        file.seekg(0);

        std::vector<uint8_t> data(size);
        file.read((char*)data.data(), size);
        return data;
    }

    ComPtr<IDxcBlob> TestTriangle::CompileDXRLibrary(const wchar_t* filename)
    {
        auto sourceData = LoadBinaryFile(filename);

        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcCompiler3> compiler;
        ComPtr<IDxcIncludeHandler> includeHandler;

        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
        ThrowIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));

        DxcBuffer source = {};
        source.Ptr = sourceData.data();
        source.Size = sourceData.size();
        source.Encoding = DXC_CP_UTF8;

        const wchar_t* arguments[] =
        {
            filename,               // REQUIRED (virtual filename)
            L"-T", L"lib_6_3",        // DXR shader library
            L"-HV", L"2021",
            L"-Zi",
            L"-Qembed_debug",
            L"-Od"
            //, L"-WX"                   // Treat warnings as errors (optional)
        };

        ComPtr<IDxcResult> result;
        ThrowIfFailed(compiler->Compile(&source, arguments, _countof(arguments), includeHandler.Get(), IID_PPV_ARGS(&result)));

        // Check compile status
        HRESULT status;
        result->GetStatus(&status);
        if (FAILED(status))
        {
            ComPtr<IDxcBlobUtf8> errors;
            result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
            if (errors && errors->GetStringLength())
                OutputDebugStringA(errors->GetStringPointer());

            throw std::runtime_error("DXR shader compilation failed");
        }

        // Get DXIL object
        ComPtr<IDxcBlob> dxil;
        HRESULT hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxil), nullptr);
        if (FAILED(hr) || !dxil || dxil->GetBufferSize() == 0)
        {
            throw std::runtime_error("Failed to retrieve DXIL object");
        }

        return dxil;
    }

    void shaderTableEntryHelper(UINT entry, ID3D12StateObjectProperties* pRtsoProps, uint8_t* pData, const WCHAR* exportName, const void* rootData = nullptr, size_t rootSize = 0)
    {

    }

    void TestTriangle::createShaderTable()
    {
        /** The shader-table layout is as follows:
            Entry 0 - Ray-gen program
            Entry 1 - Miss program
            Entry 2 - Hit program for triangle 0
            Entry 3 - Hit program for the plane
            Entry 4 - Hit program for triangle 1
            Entry 5 - Hit program for triangle 2
            All entries in the shader-table must have the same size, so we will choose it base on the largest required entry.
            The triangle hit program requires the largest entry - sizeof(program identifier) + 8 bytes for the constant-buffer root descriptor.
            The entry size must be aligned up to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
        */

        // Calculate the size and create the buffer
        mShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        mShaderTableEntrySize += 8; // The ray-gen's descriptor table
        mShaderTableEntrySize = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, mShaderTableEntrySize);
        uint32_t shaderTableSize = mShaderTableEntrySize * 11;

        // For simplicity, we create the shader-table on the upload heap. You can also create it on the default heap
        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Alignment = 0;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.Height = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Width = shaderTableSize;

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpShaderTable)));

        // Map the buffer
        uint8_t* pData;
        ThrowIfFailed(mpShaderTable->Map(0, nullptr, (void**)&pData));

        ComPtr<ID3D12StateObjectProperties> pRtsoProps;
        mpPipelineState->QueryInterface(IID_PPV_ARGS(&pRtsoProps));

        // Entry 0 - ray-gen program ID and descriptor data
        //memcpy(pData, pRtsoProps->GetShaderIdentifier(kRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        // 6.2 Binding the Resources
        memcpy(pData, pRtsoProps->GetShaderIdentifier(kRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        uint64_t heapStart = GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart().ptr;
        *(uint64_t*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart + GraphicsContexts::c_descriptorSize * mUavPosition;

        // Entry 1 - primary ray miss
        memcpy(pData + mShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 2 - shadow-ray miss
        memcpy(pData + mShaderTableEntrySize * 2, pRtsoProps->GetShaderIdentifier(kShadowMiss), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 3 - Triangle 0, primary ray. ProgramID and constant-buffer data
        uint8_t* pEntry3 = pData + mShaderTableEntrySize * 3;
        memcpy(pEntry3, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        assert(((uint64_t)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
        *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[0].Resource->GetGPUVirtualAddress();
        // 15.3.a
        *(uint64_t*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + GraphicsContexts::c_descriptorSize * mVertexBufferSrvPosition; // The SRV

        // Entry 4 - Triangle 0, shadow ray. ProgramID only
        uint8_t* pEntry4 = pData + mShaderTableEntrySize * 4;
        memcpy(pEntry4, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 5 - Plane, primary ray. ProgramID only and the TLAS SRV
        uint8_t* pEntry5 = pData + mShaderTableEntrySize * 5;
        memcpy(pEntry5, pRtsoProps->GetShaderIdentifier(kPlaneHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        *(uint64_t*)(pEntry5 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart + GraphicsContexts::c_descriptorSize * mSrvPosition[0];

        // Entry 6 - Plane, shadow ray
        uint8_t* pEntry6 = pData + mShaderTableEntrySize * 6;
        memcpy(pEntry6, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 7 - Triangle 1, primary ray. ProgramID and constant-buffer data
        uint8_t* pEntry7 = pData + mShaderTableEntrySize * 7;
        memcpy(pEntry7, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        assert(((uint64_t)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
        *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[1].Resource->GetGPUVirtualAddress();
        // 15.3.b
        *(uint64_t*)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + GraphicsContexts::c_descriptorSize * mVertexBufferSrvPosition; // The SRV

        // Entry 8 - Triangle 1, shadow ray. ProgramID only
        uint8_t* pEntry8 = pData + mShaderTableEntrySize * 8;
        memcpy(pEntry8, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 9 - Triangle 2, primary ray. ProgramID and constant-buffer data
        uint8_t* pEntry9 = pData + mShaderTableEntrySize * 9;
        memcpy(pEntry9, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        assert(((uint64_t)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
        *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[2].Resource->GetGPUVirtualAddress();
        // 15.3.c
        *(uint64_t*)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + GraphicsContexts::c_descriptorSize * mVertexBufferSrvPosition; // The SRV

        // Entry 10 - Triangle 2, shadow ray. ProgramID only
        uint8_t* pEntry10 = pData + mShaderTableEntrySize * 10;
        memcpy(pEntry10, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Unmap
        mpShaderTable->Unmap(0, nullptr);
    }

    // todo: handle this in a more elegant way when we implement resizing
    void TestTriangle::createShaderResources()
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
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&mpOutputResource))); // Starting as copy-source to simplify onFrameRender()

        // Create the UAV. Based on the root signature we created it should be the first entry
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        m_deviceResources->GetD3DDevice()->CreateUnorderedAccessView(mpOutputResource.Get(), nullptr, &uavDesc, GraphicsContexts::GetCpuHandle(mUavPosition));

        for (UINT i = 0; i < DeviceResources::c_backBufferCount; i++)
        {
            // 6.1 Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS[i].pResult->GetGPUVirtualAddress();

            m_deviceResources->GetD3DDevice()->CreateShaderResourceView(nullptr, &srvDesc, GraphicsContexts::GetCpuHandle(mSrvPosition[i]));
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.StructureByteStride = sizeof(TriVertex); // your vertex struct size goes here
        srvDesc.Buffer.NumElements = 3; // number of vertices go here
        m_deviceResources->GetD3DDevice()->CreateShaderResourceView(mpVertexBuffer.Get(), &srvDesc, GraphicsContexts::GetCpuHandle(mVertexBufferSrvPosition));
        mpVertexBuffer->SetName(L"SRV VB");
    }

    void TestTriangle::createConstantBuffer()
    {
        // The shader declares the CB with 9 float3. However, due to HLSL packing rules, we create the CB with 9 float4 (each float3 needs to start on a 16-byte boundary)
        //XMFLOAT4 bufferData[] =
        //{
        //    // A
        //    XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),
        //    XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
        //    XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),

        //    // B
        //    XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f),
        //    XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f),
        //    XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),

        //    // C
        //    XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),
        //    XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f),
        //    XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f),
        //};

        XMFLOAT4 bufferData[] =
        {
            // A
            XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),
            XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
            XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),

            // B
            XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f),
            XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),

            // C
            XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),
            XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f),
            XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f),
        };

        // Create the constant buffer.
        {
            for (UINT cbvIndex = 0; cbvIndex < countOfConstantBuffers; cbvIndex++)
            {
                ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                    D3D12_HEAP_FLAG_NONE,
                    &CD3DX12_RESOURCE_DESC::Buffer(mpConstantBuffer[cbvIndex].AlignedSize * DeviceResources::c_backBufferCount),
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&mpConstantBuffer[cbvIndex].Resource)));

                NAME_D3D12_OBJECT(mpConstantBuffer[cbvIndex].Resource);

                // Create constant buffer views to access the upload buffer.
                D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = mpConstantBuffer[cbvIndex].Resource->GetGPUVirtualAddress();

                for (UINT n = 0; n < DeviceResources::c_backBufferCount; n++)
                {
                    mpConstantBuffer[cbvIndex].HeapIndex[n] = GraphicsContexts::GetAvailableHeapPosition();

                    // Describe and create constant buffer views.
                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                    cbvDesc.BufferLocation = cbvGpuAddress;
                    cbvDesc.SizeInBytes = mpConstantBuffer[cbvIndex].AlignedSize;
                    m_deviceResources->GetD3DDevice()->CreateConstantBufferView(&cbvDesc, GraphicsContexts::GetCpuHandle(mpConstantBuffer[cbvIndex].HeapIndex[n]));

                    cbvGpuAddress += mpConstantBuffer[cbvIndex].AlignedSize;
                    mpConstantBuffer[cbvIndex].GpuHandle[n] = GraphicsContexts::GetGpuHandle(mpConstantBuffer[cbvIndex].HeapIndex[n]);
                }

                // Map and initialize the constant buffer. We don't unmap this until the
                // app closes. Keeping things mapped for the lifetime of the resource is okay.
                CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
                ThrowIfFailed(mpConstantBuffer[cbvIndex].Resource->Map(0, &readRange, reinterpret_cast<void**>(&mpConstantBuffer[cbvIndex].MappedData)));

                memcpy(mpConstantBuffer[cbvIndex].CpuData, &bufferData[cbvIndex * countOfConstantBuffers], sizeof(bufferData));

                for (UINT i = 0; i < DeviceResources::c_backBufferCount; i++)
                {
                    mpConstantBuffer[cbvIndex].CopyToGpu(i);
                }
            }
        }
    }

    TestTriangle::TestTriangle()
    {
    }

    TestTriangle::~TestTriangle()
    {
        Release();
    }

    void TestTriangle::CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources)
    {
        m_deviceResources = deviceResources;

        // reserve the uav position and srv position for reuse if window is resized
        mUavPosition = GraphicsContexts::GetAvailableHeapPosition();
        for (UINT i = 0; i < DeviceResources::c_backBufferCount; i++)
        {
            mSrvPosition[i] = GraphicsContexts::GetAvailableHeapPosition();
        }
        mVertexBufferSrvPosition = GraphicsContexts::GetAvailableHeapPosition();

        createAccelerationStructures(); // Tutorial 03
        createConstantBuffer(); // Tutorial 09
        createRtPipelineState(); // Tutorial 04
        createShaderResources(); // Tutorial 06. Need to do this before initializing the shader-table
        createShaderTable(); // Tutorial 05
    }

    void TestTriangle::CreateWindowSizeDependentResources()
    {
        createShaderResources();
    }

    void TestTriangle::Update(DX::StepTimer const& timer)
    {
		float rotation = timer.GetTotalSeconds() * 0.5f;

		XMMATRIX orthoLH = XMMatrixIdentity();
        //XMMATRIX orthoLH = XMMatrixOrthographicLH(static_cast<float>(m_deviceResources->GetResolution().Width), static_cast<float>(m_deviceResources->GetResolution().Height), 0.0f, 100.0f);
        
        XMVECTOR vec1 = XMVectorSet(-2, 0, 0, 0);
        XMVECTOR vec2 = XMVectorSet(2, 0, 0, 0);

		XMMATRIX translation1 = XMMatrixTranslationFromVector(vec1);

        // 3 instances
        m_xmIdentity[0] = XMMatrixIdentity(); // Identity matrix
        m_xmIdentity[1] = XMMatrixRotationY(rotation) * translation1;
        m_xmIdentity[2] = XMMatrixTranslation(2, 0, 0) * XMMatrixRotationY(rotation);
        //xmIdentity[3] = XMMatrixIdentity();


    }

    void TestTriangle::Render()
    {
        ID3D12GraphicsCommandList4* m_sceneCommandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_SCENE_0, nullptr);

        // Populate m_sceneCommandList to render scene to intermediate render target.
        {
            PIXBeginEvent(m_sceneCommandList, 0, L"TestTriangle.");

            // refitting
            {
                //float elapsedTime = float(timer.GetElapsedSeconds());
                RefitOrRebuildTLAS(m_sceneCommandList, m_deviceResources->GetCurrentFrameIndex(), true);

                // 6.1 Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
                //D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                //srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                //srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                //srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS[m_deviceResources->GetCurrentFrameIndex()].pResult->GetGPUVirtualAddress();

                //m_deviceResources->GetD3DDevice()->CreateShaderResourceView(nullptr, &srvDesc, GraphicsContexts::GetCpuHandle(mSrvPosition[m_deviceResources->GetCurrentFrameIndex()]));
            }

            ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get() };
            m_sceneCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            D3D12_RESOURCE_BARRIER barriers[2] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(mpOutputResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetIntermediateRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST),
            };

            // only use the first resource barrier to transition the output resource, the second one is used later
            m_sceneCommandList->ResourceBarrier(1, &barriers[0]);

            D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
            //raytraceDesc.Width = std::max<UINT>(static_cast<UINT>(m_deviceResources->GetOutputSize().right - m_deviceResources->GetOutputSize().left), 1u);
            //raytraceDesc.Height = std::max<UINT>(static_cast<UINT>(m_deviceResources->GetOutputSize().bottom - m_deviceResources->GetOutputSize().top), 1u);
            //raytraceDesc.Width = static_cast<UINT>(std::max(1.0f, m_deviceResources->GetScreenViewport().Width)); // todo: verify the window resizing works when doing refitting
            //raytraceDesc.Height = static_cast<UINT>(std::max(1.0f, m_deviceResources->GetScreenViewport().Height)); // todo: verify the window resizing works when doing refitting
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
            raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize * 8;    // 13.3.d 8 hit-entries

            // 6.4.e Bind the empty root signature
            m_sceneCommandList->SetComputeRootSignature(mpEmptyRootSig.Get());

            // 6.4.f Set Pipeline
            m_sceneCommandList->SetPipelineState1(mpPipelineState.Get());

            // 6.4.g Dispatch
            m_sceneCommandList->DispatchRays(&raytraceDesc);

            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

            m_sceneCommandList->ResourceBarrier(2, &barriers[0]);
            m_sceneCommandList->CopyResource(m_deviceResources->GetIntermediateRenderTarget(), mpOutputResource.Get());

            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            m_sceneCommandList->ResourceBarrier(1, &barriers[1]);

            PIXEndEvent(m_sceneCommandList);

            ThrowIfFailed(m_sceneCommandList->Close());
        }
    }

    void TestTriangle::Release()
    {
        mpVertexBuffer.Reset();
        //mpTopLevelAS.Release(); // todo:
        mpBottomLevelAS.Reset();
        mpPipelineState.Reset();
        mpEmptyRootSig.Reset();
        mpShaderTable.Reset();
        mpOutputResource.Reset();
        for (UINT cbvIndex = 0; cbvIndex < countOfConstantBuffers; cbvIndex++)
        {
            mpConstantBuffer[cbvIndex].Release();
        }

        GraphicsContexts::RemoveHeapPosition(mUavPosition);
        for (UINT i = 0; i < DeviceResources::c_backBufferCount; i++)
        {
            GraphicsContexts::RemoveHeapPosition(mSrvPosition[i]);
        }
    }
}
