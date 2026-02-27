#pragma once

using namespace DirectX;

namespace CPyburnRTXEngine
{
	class CameraBase
	{
	private:
		struct CameraCbv
		{
			XMFLOAT4X4 gView;
			XMFLOAT4X4 gProj;
			XMFLOAT4X4 gInvView;
			XMFLOAT4X4 gInvProj;

			XMFLOAT3 gCameraPos;
		};

		ConstantBuffer<CameraCbv> m_cameraCbv;
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		float m_aspectRatio = 0;
		float m_fieldOfView = 0;

		XMVECTOR m_up;
		XMVECTOR m_eye;
		XMVECTOR m_lookAt;

		CameraCbv m_cbvData = {};
	public:
		CameraBase();
		virtual ~CameraBase() = default;

		ID3D12Resource* GetCbv() { return m_cameraCbv.Resource.Get(); }

		virtual void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		virtual void CreateWindowSizeDependentResources();
		virtual void Update(_In_ DX::StepTimer const& timer, _In_  const XMVECTOR& mousePosition);
		//virtual void Release();
	};
}
