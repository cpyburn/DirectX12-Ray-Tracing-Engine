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

		//XMVECTOR m_pitchYallRoll = {};
		const float m_movementSpeed = 10.0f;
		float m_yaw = 0;
		float m_pitch = 0;
		float m_mouseSensitivity = 0.001f;

		CameraCbv m_cbvData = {};
	public:
		CameraBase();
		virtual ~CameraBase() = default;

		ID3D12Resource* GetCbv() { return m_cameraCbv.Resource.Get(); }

		virtual void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		virtual void CreateWindowSizeDependentResources();
		virtual void Update(DX::StepTimer const& timer, GameInput* gameInput);
		//virtual void Release(); // todo:
	};
}
