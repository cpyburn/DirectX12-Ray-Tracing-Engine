#pragma once


namespace CPyburnRTXEngine
{
	class CameraBase
	{
	private:
		
	public:
		CameraBase();
		virtual ~CameraBase() = default;

		virtual void Update(_In_ DX::StepTimer const& timer, _In_  const XMVECTOR& mousePosition) = 0;
	};
}
