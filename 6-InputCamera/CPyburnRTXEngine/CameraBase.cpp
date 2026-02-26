#include "pchlib.h"
#include "CameraBase.h"

namespace CPyburnRTXEngine
{
	CameraBase::CameraBase()
	{

	}

	void CameraBase::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	{
		m_deviceResources = deviceResources;
		
		m_cameraCbv.CreateCbvOnUploadHeap(deviceResources->GetD3DDevice(), L"Camera Cbv");
	}

	void Update(DX::StepTimer const& timer, const XMVECTOR& mousePosition)
	{

	}
}
