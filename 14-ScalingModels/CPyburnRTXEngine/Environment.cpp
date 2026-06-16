#include "pchlib.h"
#include "Environment.h"

namespace CPyburnRTXEngine
{
	Environment::Environment()
	{
	}

	Environment::~Environment()
	{
	}

	void Environment::CreateDeviceDependentResources(DX::DeviceResources* deviceResources)
	{
		m_deviceResources = deviceResources;
		m_EnvironmentCb.CreateCbvOnUploadHeap(m_deviceResources->GetD3DDevice(), L"Environment Cbv");

		for (UINT i = 0; i < DX::DeviceResources::c_backBufferCount; i++)
		{
			m_EnvironmentCb.CopyToGpu(i);
		}
	}

	void Environment::Update(DX::StepTimer const& timer, CameraBase* camera)
	{
		XMFLOAT3 lightDir = XMFLOAT3(0.5f, 0.5f, -0.5f);
		m_EnvironmentCb.CpuData.lightDirection = lightDir;
		m_EnvironmentCb.CopyToGpu(m_deviceResources->GetCurrentFrameIndex());
	}
	void Environment::Release()
	{
		m_EnvironmentCb.Release();
	}
}