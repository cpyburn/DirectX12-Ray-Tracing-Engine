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

    void CameraBase::Update(DX::StepTimer const& timer, GameInput* gameInput)
    {
        float dt = static_cast<float>(timer.GetElapsedSeconds());

        // =========================
        // Mouse Look (Relative Mode)
        // =========================
        auto mouse = gameInput->GetMouse()->GetState();
        auto previousMouse = gameInput->GetMouseButtons().GetLastState();

        // Switch modes based on left button
        gameInput->GetMouse()->SetMode(mouse.rightButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);

        if (mouse.positionMode == Mouse::MODE_RELATIVE)
        {
            // In relative mode, x/y are already deltas
            float deltaX = static_cast<float>(previousMouse.x);
            float deltaY = static_cast<float>(previousMouse.y);

            m_yaw += deltaX * m_mouseSensitivity;
            m_pitch += deltaY * m_mouseSensitivity;

            // Clamp pitch so we don’t flip upside down
            m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);
        }

        // =========================
        // 2. BUILD CAMERA BASIS
        // =========================
        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);
        XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotation);
        XMVECTOR right = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), rotation);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);

        // =========================
        // 3. KEYBOARD MOVEMENT
        // =========================
        auto keyboard = gameInput->GetKeyboard()->GetState();
        XMVECTOR move = XMVectorZero();

        if (keyboard.W)
            move += forward;
        if (keyboard.S)
            move -= forward;
        if (keyboard.A)
            move -= right;
        if (keyboard.D)
            move += right;

        if (!XMVector3Equal(move, XMVectorZero()))
            move = XMVector3Normalize(move);

        m_eye += move * m_movementSpeed * dt;

        // =========================
        // 4. BUILD VIEW / PROJECTION
        // =========================
        XMVECTOR lookAt = m_eye + forward;
        XMMATRIX view = XMMatrixLookAtLH(m_eye, lookAt, up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fieldOfView, m_aspectRatio, 0.1f, 1000.0f);
        XMMATRIX invView = XMMatrixInverse(nullptr, view);
        XMMATRIX invProj = XMMatrixInverse(nullptr, proj);

        // =========================
        // 5. UPDATE CONSTANT BUFFER
        // =========================
        XMStoreFloat4x4(&m_cameraCbv.CpuData.gView, view);
        XMStoreFloat4x4(&m_cameraCbv.CpuData.gProj, proj);
        XMStoreFloat4x4(&m_cameraCbv.CpuData.gInvView, invView);
        XMStoreFloat4x4(&m_cameraCbv.CpuData.gInvProj, invProj);

        XMStoreFloat3(&m_cameraCbv.CpuData.gCameraPos, m_eye);

        m_cameraCbv.CopyToGpu(m_deviceResources->GetCurrentFrameIndex());
    }
}
