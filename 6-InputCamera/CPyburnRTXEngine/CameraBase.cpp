#include "pchlib.h"
#include "CameraBase.h"

namespace CPyburnRTXEngine
{
    CameraBase::CameraBase() :
        m_fieldOfView(70.0f * XM_PI / 180.0f)
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

        // view * proj maps world -> clip. We need its inverse to map clip->world.
        XMMATRIX viewProj = XMMatrixMultiply(view, proj);
        XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

        // Store as TRANSPOSED so HLSL reads correctly when using default column-major in shader
        XMStoreFloat4x4(&m_cameraCbv.CpuData.invViewProj, XMMatrixTranspose(invViewProj));

        // camera position
        XMStoreFloat3(&m_cameraCbv.CpuData.camPos, m_eye);

        // resolution must be set by you from your DeviceResources
        // dst->resolution = XMFLOAT2(float(width), float(height));
        m_cameraCbv.CpuData.resolution = XMFLOAT2(1280.0f, 720.0f);

        m_cameraCbv.CopyToGpu(m_deviceResources->GetCurrentFrameIndex());
	}
}
