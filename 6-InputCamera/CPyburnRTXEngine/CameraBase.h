#pragma once

using namespace DirectX;

namespace CPyburnRTXEngine
{
	class CameraBase
	{
	private:
		struct CameraCbv
		{
			XMFLOAT4X4 invViewProj; // 64 bytes
			XMFLOAT3   camPos;      // 12 bytes
			//float      _pad0;       // 4 bytes (pad to 16)
			XMFLOAT2   resolution;  // 8 bytes
			//float      _pad1[2];    // 8 bytes (pad to 16)
		};

		ConstantBuffer<CameraCbv> m_cameraCbv;
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
	public:
		CameraBase();
		virtual ~CameraBase() = default;

		ID3D12Resource* GetCbv() { return m_cameraCbv.Resource.Get(); }

		virtual void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		//virtual void Update(_In_ DX::StepTimer const& timer, _In_  const XMVECTOR& mousePosition);
		//virtual void Release();
	};
}
