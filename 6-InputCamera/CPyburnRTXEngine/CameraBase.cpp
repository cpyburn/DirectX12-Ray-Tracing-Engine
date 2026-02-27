#include "pchlib.h"
#include "CameraBase.h"

namespace CPyburnRTXEngine
{
    CameraBase::CameraBase() :
        m_fieldOfView(70.0f * XM_PI / 180.0f),
        m_up(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
        m_eye(XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f)),
        m_lookAt(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f))
	{

	}

	void CameraBase::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	{
		m_deviceResources = deviceResources;		
		m_cameraCbv.CreateCbvOnUploadHeap(deviceResources->GetD3DDevice(), L"Camera Cbv");
	}

    void CameraBase::CreateWindowSizeDependentResources()
    {
        m_aspectRatio = m_deviceResources->GetScreenViewport().Width / m_deviceResources->GetScreenViewport().Height;
    }

	void CameraBase::Update(_In_ DX::StepTimer const& timer, _In_  const XMVECTOR& mousePosition)
	{
        // build view and projection (Left-handed example)
        XMMATRIX view = XMMatrixLookAtLH(m_eye, m_lookAt, m_up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fieldOfView, m_aspectRatio, 0.1f, 1000.0f);
        XMMATRIX invView = XMMatrixInverse(nullptr, view);
        XMMATRIX invProj = XMMatrixInverse(nullptr, proj);

        // view * proj maps world -> clip. We need its inverse to map clip->world.
        XMMATRIX viewProj = XMMatrixMultiply(view, proj);
        XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

        XMStoreFloat4x4(&m_cameraCbv.CpuData.gView, view);
        XMStoreFloat4x4(&m_cameraCbv.CpuData.gProj, proj);
        XMStoreFloat4x4(&m_cameraCbv.CpuData.gInvView, invView);
        XMStoreFloat4x4(&m_cameraCbv.CpuData.gInvProj, invProj);

        // camera position
        XMStoreFloat3(&m_cameraCbv.CpuData.gCameraPos, m_eye);

        // resolution must be set by you from your DeviceResources
        // dst->resolution = XMFLOAT2(float(width), float(height));
        //m_cameraCbv.CpuData.resolution = XMFLOAT2(1280.0f, 720.0f);

        m_cameraCbv.CopyToGpu(m_deviceResources->GetCurrentFrameIndex());
	}
}
