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
            inputs.NumDescs = 1;
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
            bufDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

			ComPtr<ID3D12Resource> pInstanceDescResource;
            ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pInstanceDescResource)));
            D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
            pInstanceDescResource->Map(0, nullptr, (void**)&pInstanceDesc);

            // Initialize the instance desc. We only have a single instance
            pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
            pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
            pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            XMMATRIX xmIdentity = XMMatrixIdentity(); // Identity matrix
            memcpy(pInstanceDesc->Transform, &xmIdentity, sizeof(pInstanceDesc->Transform));
            pInstanceDesc->AccelerationStructure = mpBottomLevelAS->GetGPUVirtualAddress();
            pInstanceDesc->InstanceMask = 0xFF;

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
        //  2 for the root-signature shared between miss and hit shaders (signature and association)
        //  2 for shader config (shared between all programs. 1 for the config, 1 for association)
        //  1 for pipeline config
        //  1 for the global root signature

        D3D12_STATE_SUBOBJECT subobjects[10] = {};
        uint32_t index = 0;

        //0

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
        D3D12_ROOT_SIGNATURE_DESC desc{};
        std::vector<D3D12_DESCRIPTOR_RANGE> range;
        std::vector<D3D12_ROOT_PARAMETER> rootParams;

        range.resize(2);
        // gOutput
        range[0].BaseShaderRegister = 0;
        range[0].NumDescriptors = 1;
        range[0].RegisterSpace = 0;
        range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        range[0].OffsetInDescriptorsFromTableStart = 0;

        // gRtScene
        range[1].BaseShaderRegister = 0;
        range[1].NumDescriptors = 1;
        range[1].RegisterSpace = 0;
        range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range[1].OffsetInDescriptorsFromTableStart = 1;

        rootParams.resize(1);
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[0].DescriptorTable.NumDescriptorRanges = 2;
        rootParams[0].DescriptorTable.pDescriptorRanges = range.data();

        // Create the desc
        desc.NumParameters = 1;
        desc.pParameters = rootParams.data();
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // create ray-gen root-signature and associate it with the ray-gen shader
        ComPtr<ID3DBlob> pSigBlobRay;
        ComPtr<ID3DBlob> pErrorBlobRay;
        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobRay, &pErrorBlobRay);
        if (FAILED(hr))
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
        D3D12_ROOT_SIGNATURE_DESC emptyDescMissHit = {};
        emptyDescMissHit.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // create empty root-signature and associate it with the ray-gen shader
        ComPtr<ID3DBlob> pSigBlobEmptyMissHit;
        ComPtr<ID3DBlob> pErrorBlobEmptyMissHit;
        HRESULT hrEmptyHitMiss = D3D12SerializeRootSignature(&emptyDescMissHit, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlobEmptyMissHit, &pErrorBlobEmptyMissHit);
        if (FAILED(hrEmptyHitMiss))
        {
            if (pErrorBlobEmptyMissHit)
            {
                OutputDebugStringA((char*)pErrorBlobEmptyMissHit->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        ComPtr<ID3D12RootSignature> pRootSigEmptyMissHit;
        ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRootSignature(0, pSigBlobEmptyMissHit->GetBufferPointer(), pSigBlobEmptyMissHit->GetBufferSize(), IID_PPV_ARGS(&pRootSigEmptyMissHit)));

        ID3D12RootSignature* pRootSigEmptyPtrMissHit = pRootSigEmptyMissHit.Get();
        subobjects[index].pDesc = &pRootSigEmptyPtrMissHit;
        subobjects[index++].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;

        //5
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationMissHit = {};
        associationMissHit.NumExports = 2;
        const WCHAR* exportsMissHit[] = { kMissShader, kClosestHitShader };
        associationMissHit.pExports = exportsMissHit;
        associationMissHit.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationMissHit;

        //6
            // Shader config, shared between all shaders (ray-gen, miss and hit)
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
        shaderConfig.MaxPayloadSizeInBytes = sizeof(XMFLOAT3); // We only need to output a color
        shaderConfig.MaxAttributeSizeInBytes = sizeof(XMFLOAT2); // Triangle hit attributes are barycentrics, which can be represented in 2 floats

        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        subobjects[index++].pDesc = &shaderConfig;

        //7

        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associationMissChsRgs = {};
        associationMissChsRgs.NumExports = 3;
        const WCHAR* exportsMissChsRgs[] = { kMissShader, kClosestHitShader, kRayGenShader };
        associationMissChsRgs.pExports = exportsMissChsRgs;
        associationMissChsRgs.pSubobjectToAssociate = &subobjects[index - 1];
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobjects[index++].pDesc = &associationMissChsRgs;

        //8
        D3D12_RAYTRACING_PIPELINE_CONFIG config = {};
        config.MaxTraceRecursionDepth = 1;
        subobjects[index].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        subobjects[index++].pDesc = &config;

        //9
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

        //10

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
        uint8_t* pHitEntry = pData + mShaderTableEntrySize * 2; // +2 skips the ray-gen and miss entries
        memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

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
        CD3DX12_CPU_DESCRIPTOR_HANDLE uavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), mUavPosition, GraphicsContexts::c_descriptorSize);
        m_deviceResources->GetD3DDevice()->CreateUnorderedAccessView(mpOutputResource.Get(), nullptr, &uavDesc, uavCpuHandle);

        // 6.1 Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS->GetGPUVirtualAddress();

        mSrvPosition = GraphicsContexts::GetAvailableHeapPosition();
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), mSrvPosition, GraphicsContexts::c_descriptorSize);
        m_deviceResources->GetD3DDevice()->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
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
        createRtPipelineState(); // Tutorial 04
        createShaderResources(); // Tutorial 06. Need to do this before initializing the shader-table
        createShaderTable(); // Tutorial 05
    }

    void TestTriangle::Render()
    {
        ID3D12GraphicsCommandList4* m_sceneCommandList = m_deviceResources->GetCurrentFrameResource()->ResetCommandList(FrameResource::COMMAND_LIST_SCENE_0, nullptr);

        // Populate m_sceneCommandList to render scene to intermediate render target.
        {
            PIXBeginEvent(m_sceneCommandList, 0, L"TestTriangle.");

            ID3D12DescriptorHeap* ppHeaps[] = { GraphicsContexts::c_heap.Get() };
            m_sceneCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
            //m_sceneCommandList->RSSetViewports(1, &m_deviceResources->GetScreenViewport());
            //m_sceneCommandList->RSSetScissorRects(1, &m_deviceResources->GetScissorRect());

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

            //resourceBarrier(mpCmdList, mpOutputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
            //resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            //mpCmdList->CopyResource(mFrameObjects[rtvIndex].pSwapChainBuffer, mpOutputResource);
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
