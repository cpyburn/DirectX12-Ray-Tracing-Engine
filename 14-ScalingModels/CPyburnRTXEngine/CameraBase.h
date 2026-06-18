#pragma once

#include "BufferConstant.h"

using namespace DirectX;

namespace CPyburnRTXEngine
{
	class CameraBase
	{
	public:
		struct CameraCbv
		{
			XMFLOAT4X4 gView;
			XMFLOAT4X4 gProj;
			XMFLOAT4X4 gInvView;
			XMFLOAT4X4 gInvProj;

			XMFLOAT3 gCameraPos;
		};
	private:
		BufferConstant<CameraCbv> m_cameraCbv;
		DX::DeviceResources* m_deviceResources = nullptr;

		float m_aspectRatio = 0;
		float m_fieldOfView = 0;

		XMFLOAT3 m_up;
		XMFLOAT3 m_eye;
		XMFLOAT3 m_lookAt;

		const float m_movementSpeed = 10.0f;
		float m_yaw = 0;
		float m_pitch = 0;
		float m_mouseSensitivity = 0.001f;

		CameraCbv m_cbvData = {};

		DirectX::BoundingFrustum m_boundingFrustum;
	public:
		const DX::DeviceResources* GetDeviceResources() { return m_deviceResources; }
		const DirectX::BoundingFrustum& GetBoundingFrustum() { return m_boundingFrustum; }

		CameraBase();
		virtual ~CameraBase() = default;

		BufferConstant<CameraCbv>* GetCbv() { return &m_cameraCbv; }

		virtual void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		virtual void CreateWindowSizeDependentResources();
		virtual void Update(DX::StepTimer const& timer, GameInput* gameInput);
		//virtual void Release(); // todo:
	};
}
