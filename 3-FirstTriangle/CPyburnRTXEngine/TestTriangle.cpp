#include "pchlib.h"
#include "TestTriangle.h"

namespace CPyburnRTXEngine
{
	void TestTriangle::createAccelerationStructures()
	{
        // create createTriangleVB
        const XMFLOAT3 vertices[] =
        {
            XMFLOAT3(0,          1,  0),
            XMFLOAT3(0.866f,  -0.5f, 0),
            XMFLOAT3(-0.866f, -0.5f, 0),
        };

        static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
        {
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0,
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

        AccelerationStructureBuffers bottomLevelBuffers = createBottomLevelAS(mpDevice, mpCmdList, mpVertexBuffer);
        AccelerationStructureBuffers topLevelBuffers = createTopLevelAS(mpDevice, mpCmdList, bottomLevelBuffers.pResult, mTlasSize);

        // The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
        //mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
        //mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        //WaitForSingleObject(mFenceEvent, INFINITE);
        //uint32_t bufferIndex = mpSwapChain->GetCurrentBackBufferIndex();
        //mpCmdList->Reset(mFrameObjects[0].pCmdAllocator, nullptr);

        // Store the AS buffers. The rest of the buffers will be released once we exit the function
        mpTopLevelAS = topLevelBuffers.pResult;
        mpBottomLevelAS = bottomLevelBuffers.pResult;
	}

    void TestTriangle::CreateDeviceDependentResources(const std::shared_ptr<DeviceResources>& deviceResources)
	{
		m_deviceResources = deviceResources;
		createAccelerationStructures();
    }

    void TestTriangle::Release()
	{

	}
}
