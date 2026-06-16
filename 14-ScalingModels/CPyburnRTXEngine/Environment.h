#pragma once

namespace CPyburnRTXEngine
{
	class Environment
	{
	private:
		struct EnvironmentData
		{
			XMFLOAT3 lightDirection = XMFLOAT3(0.5f, 0.5f, -0.5f);
		};
		EnvironmentData m_environmentData = {};
		BufferConstant<EnvironmentData> m_EnvironmentCb;

		DX::DeviceResources* m_deviceResources = nullptr;
	public:
		const BufferConstant<EnvironmentData>& GetEnvironmentConstantBuffer() { return m_EnvironmentCb; }

		Environment();
		~Environment();
		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void Update(DX::StepTimer const& timer, CameraBase* camera);
		void Release();
	};
}