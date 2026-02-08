#include "pchlib.h"
#include "TestTriangle.h"

namespace CPyburnRTXEngine
{
	void TestTriangle::createAccelerationStructures()
	{
        //mpVertexBuffer = createTriangleVB(mpDevice);
        //AccelerationStructureBuffers bottomLevelBuffers = createBottomLevelAS(mpDevice, mpCmdList, mpVertexBuffer);
        //AccelerationStructureBuffers topLevelBuffers = createTopLevelAS(mpDevice, mpCmdList, bottomLevelBuffers.pResult, mTlasSize);

        //// The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
        //mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
        //mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        //WaitForSingleObject(mFenceEvent, INFINITE);
        //uint32_t bufferIndex = mpSwapChain->GetCurrentBackBufferIndex();
        //mpCmdList->Reset(mFrameObjects[0].pCmdAllocator, nullptr);

        //// Store the AS buffers. The rest of the buffers will be released once we exit the function
        //mpTopLevelAS = topLevelBuffers.pResult;
        //mpBottomLevelAS = bottomLevelBuffers.pResult;
	}

	void TestTriangle::Release()
	{

	}
}
