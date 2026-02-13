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

    void TestTriangle::createAccelerationStructures()
    {
        // create createTriangleVB
        const XMFLOAT3 vertices[] =
        {
            XMFLOAT3(0,          1,  0),
            XMFLOAT3(0.866f,  -0.5f, 0),
            XMFLOAT3(-0.866f, -0.5f, 0),
        };

        // create buffer
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
        bufDesc.Width = sizeof(vertices);

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mpVertexBuffer)));

        // For simplicity, we create the vertex buffer on the upload heap, but that's not required
        uint8_t* pData;
        mpVertexBuffer->Map(0, nullptr, (void**)&pData);
        memcpy(pData, vertices, sizeof(vertices));
        mpVertexBuffer->Unmap(0, nullptr);

        // Single-use command allocator and command list for creating resources.
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12GraphicsCommandList4> commandList;

        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

        // Bottom Level AS
        {
            D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
            geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDesc.Triangles.VertexBuffer.StartAddress = mpVertexBuffer->GetGPUVirtualAddress();
            geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
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
            mpBottomLevelAS = pResult;
        }

        ThrowIfFailed(commandAllocator->Reset());
        ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

        // Top Level AS
        {
            // First, get the size of the TLAS buffers and create them
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = 3; // 3 instances
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
            m_deviceResources->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers
            bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bufDesc.Width = info.ScratchDataSizeInBytes;

            ComPtr<ID3D12Resource> pScratch;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pScratch)));

            bufDesc.Width = info.ResultDataMaxSizeInBytes;
            ComPtr<ID3D12Resource> pResult;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&pResult)));
            mTlasSize = info.ResultDataMaxSizeInBytes;

            // The instance desc should be inside a buffer, create and map the buffer
            bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            bufDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 3; // 3 instances

            ComPtr<ID3D12Resource> pInstanceDescResource;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pInstanceDescResource)));
            D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
            pInstanceDescResource->Map(0, nullptr, (void**)&pInstanceDesc);
            ZeroMemory(pInstanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 3); // 3 instances

            // 3 instances
            XMMATRIX xmIdentity[3] = {};
            xmIdentity[0] = XMMatrixIdentity(); // Identity matrix
            xmIdentity[1] = XMMatrixTranslation(-2, 0, 0) * XMMatrixIdentity();
            xmIdentity[2] = XMMatrixTranslation(2, 0, 0) * XMMatrixIdentity();

            for (size_t i = 0; i < _countof(xmIdentity); i++)
            {
                pInstanceDesc[i].InstanceID = i;                            // This value will be exposed to the shader via InstanceID()
                pInstanceDesc[i].InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
                pInstanceDesc[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
                XMMATRIX transpose = XMMatrixTranspose(xmIdentity[i]);
                memcpy(pInstanceDesc[i].Transform, &transpose, sizeof(pInstanceDesc[i].Transform));
                pInstanceDesc[i].AccelerationStructure = mpBottomLevelAS->GetGPUVirtualAddress();
                pInstanceDesc[i].InstanceMask = 0xFF;
            }

            // Unmap
            pInstanceDescResource->Unmap(0, nullptr);

            // Create the TLAS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
            asDesc.Inputs = inputs;
            asDesc.Inputs.InstanceDescs = pInstanceDescResource->GetGPUVirtualAddress();
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
            mpTopLevelAS = pResult;
        }
    }

    void TestTriangle::createRtPipelineState()
    {
        // Need 10 subobjects:
        //  1 for the DXIL library
        //  1 for hit-group
        //  2 for RayGen root-signature (root-signature and the subobject association)
        //  9.2.a 2 for hit-program root-signature (root-signature and the subobject association)
        //  9.2.b 2 for miss-shader root-signature (signature and association)
        //  2 for shader config (shared between all programs. 1 for the config, 1 for association)
        //  1 for pipeline config
        //  1 for the global root signature

        D3D12_STATE_SUBOBJECT subobjects[12] = {};
        uint32_t index = 0;

        //0
        // DXIL library
        std::wstring shaderFilePath = GetAssetFullPath(L"04-Shaders.hlsl");
        ComPtr<IDxcBlob> shaderBlob = CompileDXRLibrary(shaderFilePath.c_str());

        const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kClosestHitShader };

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

        //1
        // Hit group
        D3D12_HIT_GROUP_DESC hitGroupDesc = {};
        hitGroupDesc.ClosestHitShaderImport = kClosestHitShader;
        hitGroupDesc.HitGroupExport = kHitGroup;
        //hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

        D3D12_STATE_SUBOBJECT hitGroupSubobject = {};
        hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroupSubobject.pDesc = &hitGroupDesc;
        subobjects[index++] = hitGroupSubobject;

        //2
        
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
        

        //3
        // Associate the ray-gen root signature with the ray-gen shader
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationRay = {};
        associationRay.NumExports = 1;
        associationRay.pExports = &kRayGenShader;
        associationRay.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index] = {};
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationRay;

       
        
            //4
            D3D12_ROOT_SIGNATURE_DESC descHit = {};
            std::vector<D3D12_DESCRIPTOR_RANGE> rangeHit;
            std::vector<D3D12_ROOT_PARAMETER> rootParamsHit;

            rootParamsHit.resize(1);
            rootParamsHit[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParamsHit[0].Descriptor.RegisterSpace = 0;
            rootParamsHit[0].Descriptor.ShaderRegister = 0;

            descHit.NumParameters = 1;
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

            //5
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationHit = {};
            associationHit.NumExports = 1;
            const WCHAR* exportsHit[] = { kClosestHitShader };
            associationHit.pExports = exportsHit;
            associationHit.pSubobjectToAssociate = &subobjects[index - 1];
            subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
            subobjects[index++].pDesc = &associationHit;
        


        
            //6
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

            //7
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationMiss = {};
            associationMiss.NumExports = 1;
            const WCHAR* exportsMiss[] = { kMissShader };
            associationMiss.pExports = exportsMiss;
            associationMiss.pSubobjectToAssociate = &subobjects[index - 1];
            subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
            subobjects[index++].pDesc = &associationMiss;
        

        //8
        // Shader config, shared between all shaders (ray-gen, miss and hit)
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
        shaderConfig.MaxPayloadSizeInBytes = sizeof(XMFLOAT3); // We only need to output a color
        shaderConfig.MaxAttributeSizeInBytes = sizeof(XMFLOAT2); // Triangle hit attributes are barycentrics, which can be represented in 2 floats

        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        subobjects[index++].pDesc = &shaderConfig;

        //9
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationMissChsRgs = {};
        associationMissChsRgs.NumExports = 3;
        const WCHAR* exportsMissChsRgs[] = { kMissShader, kClosestHitShader, kRayGenShader };
        associationMissChsRgs.pExports = exportsMissChsRgs;
        associationMissChsRgs.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationMissChsRgs;

        //10
        D3D12_RAYTRACING_PIPELINE_CONFIG config = {};
        config.MaxTraceRecursionDepth = 1;
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        subobjects[index++].pDesc = &config;

        //11
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

        // 12
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
            L"-Od",
            L"-WX"                   // Treat warnings as errors (optional)
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

    D3D12_STATE_SUBOBJECT TestTriangle::CreateDxilSubobject()
    {
        std::wstring shaderFilePath = GetAssetFullPath(L"04-Shaders.hlsl");
        ComPtr<IDxcBlob> shaderBlob = CompileDXRLibrary(shaderFilePath.c_str());

        const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kClosestHitShader };

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
        return stateSubobject;
    }

    void TestTriangle::createShaderTable()
    {
        /** The shader-table layout is as follows:
        Entry 0 - Ray-gen program
        Entry 1 - Miss program
        Entry 2 - Hit program
        All entries in the shader-table must have the same size, so we will choose it base on the largest required entry.
        The ray-gen program requires the largest entry - sizeof(program identifier) + 8 bytes for a descriptor-table.
        The entry size must be aligned up to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
        */

        // Calculate the size and create the buffer
        mShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        mShaderTableEntrySize += 8; // The ray-gen's descriptor table
        mShaderTableEntrySize = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, mShaderTableEntrySize);
        uint32_t shaderTableSize = mShaderTableEntrySize * 3; // We have 3 programs and a single geometry, so we need 3 entries (we’ll get to why the number of entries depends on the geometry count in later tutorials).

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

        //If your descriptor table contains multiple descriptors(UAV at slot 3 and SRV at slot 4), you only need to write the GPU handle that points to the first descriptor in the table; the shader’s descriptor table layout then indexes into subsequent consecutive descriptors.

        //uint64_t heapStart = GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart().ptr;
        //*(uint64_t*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart;
        // compute GPU handle for the first descriptor in the table
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart());;
        gpuHandle.Offset(mUavPosition, GraphicsContexts::c_descriptorSize);

        // write the GPU handle ptr (8 bytes) right after the identifier
        *(UINT64*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = gpuHandle.ptr;

        // This is where we need to set the descriptor data for the ray-gen shader. We'll get to it in the next tutorial

        // Entry 1 - miss program
        memcpy(pData + mShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 2 - hit program
        //uint8_t* pHitEntry = pData + mShaderTableEntrySize * 2; // +2 skips the ray-gen and miss entries
        //memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        // 9.5 The Shader Table
        // Entry 2 - hit program. Program ID and one constant-buffer as root descriptor    
        uint8_t* pHitEntry = pData + mShaderTableEntrySize * 2; // +2 skips the ray-gen and miss entries
        memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        uint8_t* pCbDesc = pHitEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;  // Adding `progIdSize` gets us to the location of the constant-buffer entry
        assert(((uint64_t)pCbDesc % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
        *(D3D12_GPU_VIRTUAL_ADDRESS*)pCbDesc = mpConstantBuffer.Resource->GetGPUVirtualAddress();

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
        resDesc.Width = 1280; // todo: window change
        resDesc.Height = 720; // todo: window change
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&mpOutputResource))); // Starting as copy-source to simplify onFrameRender()

        // Create the UAV. Based on the root signature we created it should be the first entry
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        mUavPosition = GraphicsContexts::GetAvailableHeapPosition();
        m_deviceResources->GetD3DDevice()->CreateUnorderedAccessView(mpOutputResource.Get(), nullptr, &uavDesc, GraphicsContexts::GetCpuHandle(mUavPosition));

        // 6.1 Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS->GetGPUVirtualAddress();

        mSrvPosition = GraphicsContexts::GetAvailableHeapPosition();
        m_deviceResources->GetD3DDevice()->CreateShaderResourceView(nullptr, &srvDesc, GraphicsContexts::GetCpuHandle(mSrvPosition));
    }

    void TestTriangle::createConstantBuffer()
    {
        // The shader declares the CB with 9 float3. However, due to HLSL packing rules, we create the CB with 9 float4 (each float3 needs to start on a 16-byte boundary)
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
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(mpConstantBuffer.AlignedSize * DeviceResources::c_backBufferCount),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&mpConstantBuffer.Resource)));

            NAME_D3D12_OBJECT(mpConstantBuffer.Resource);

            // Create constant buffer views to access the upload buffer.
            D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = mpConstantBuffer.Resource->GetGPUVirtualAddress();

            for (UINT n = 0; n < DeviceResources::c_backBufferCount; n++)
            {
                mpConstantBuffer.HeapIndex[n] = GraphicsContexts::GetAvailableHeapPosition();
                CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), mpConstantBuffer.HeapIndex[n], GraphicsContexts::c_descriptorSize);

                // Describe and create constant buffer views.
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation = cbvGpuAddress;
                cbvDesc.SizeInBytes = mpConstantBuffer.AlignedSize;
                m_deviceResources->GetD3DDevice()->CreateConstantBufferView(&cbvDesc, cpuHandle);

                cbvGpuAddress += mpConstantBuffer.AlignedSize;
                mpConstantBuffer.GpuHandle[n] = CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart(), mpConstantBuffer.HeapIndex[n], GraphicsContexts::c_descriptorSize);
            }

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            ThrowIfFailed(mpConstantBuffer.Resource->Map(0, &readRange, reinterpret_cast<void**>(&mpConstantBuffer.MappedData)));
            
			memcpy(mpConstantBuffer.CpuData, bufferData, sizeof(bufferData));
            for (size_t i = 0; i < DeviceResources::c_backBufferCount; i++)
            {
                mpConstantBuffer.CopyToGpu(i);
            }
        }

        //mpConstantBuffer = createBuffer(mpDevice, sizeof(bufferData), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        //uint8_t* pData;
        //d3d_call(mpConstantBuffer->Map(0, nullptr, (void**)&pData));
        //memcpy(pData, bufferData, sizeof(bufferData));
        //mpConstantBuffer->Unmap(0, nullptr);
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
        createAccelerationStructures(); // Tutorial 03
        createConstantBuffer(); // Tutorial 09
        createRtPipelineState(); // Tutorial 04
        createShaderResources(); // Tutorial 06. Need to do this before initializing the shader-table
        createShaderTable(); // Tutorial 05
    }

    void TestTriangle::CreateWindowSizeDependentResources()
    {

    }

    void TestTriangle::Render()
    {
        ID3D12GraphicsCommandList4* m_sceneCommandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_SCENE_0, nullptr);

        // Populate m_sceneCommandList to render scene to intermediate render target.
        {
            PIXBeginEvent(m_sceneCommandList, 0, L"TestTriangle.");

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
            raytraceDesc.Width = static_cast<UINT>(std::max(1.0f, m_deviceResources->GetScreenViewport().Width));
            raytraceDesc.Height = static_cast<UINT>(std::max(1.0f, m_deviceResources->GetScreenViewport().Height));
            raytraceDesc.Depth = 1;

            // 6.4.b RayGen is the first entry in the shader-table
            raytraceDesc.RayGenerationShaderRecord.StartAddress = mpShaderTable->GetGPUVirtualAddress() + 0 * mShaderTableEntrySize;
            raytraceDesc.RayGenerationShaderRecord.SizeInBytes = mShaderTableEntrySize;

            // 6.4.c Miss is the second entry in the shader-table
            size_t missOffset = 1 * mShaderTableEntrySize;
            raytraceDesc.MissShaderTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + missOffset;
            raytraceDesc.MissShaderTable.StrideInBytes = mShaderTableEntrySize;
            raytraceDesc.MissShaderTable.SizeInBytes = mShaderTableEntrySize;   // Only a s single miss-entry

            // 6.4.d Hit is the third entry in the shader-table
            size_t hitOffset = 2 * mShaderTableEntrySize;
            raytraceDesc.HitGroupTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + hitOffset;
            raytraceDesc.HitGroupTable.StrideInBytes = mShaderTableEntrySize;
            raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize;

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
        mpTopLevelAS.Reset();
        mpBottomLevelAS.Reset();
        mpPipelineState.Reset();
        mpEmptyRootSig.Reset();
        mpShaderTable.Reset();
        mpOutputResource.Reset();
    }
}
