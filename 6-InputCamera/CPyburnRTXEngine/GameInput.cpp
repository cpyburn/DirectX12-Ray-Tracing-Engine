#include "pchlib.h"
#include "GameInput.h"


namespace CPyburnRTXEngine
{
	GameInput::GameInput() :
		m_keyboard(std::make_unique<Keyboard>()),
		m_mouse(std::make_unique<Mouse>()),
		m_gamePad0(std::make_unique<GamePad>())
	{

	}

	void GameInput::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	{
		m_mouse->SetWindow(deviceResources->GetWindow());
	}

	void GameInput::OnSuspending()
	{
		m_gamePad0->Suspend();
	}

	void GameInput::OnResuming()
	{
		m_gamePad0->Resume();
	}
}