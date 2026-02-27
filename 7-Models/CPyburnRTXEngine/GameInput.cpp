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

	GameInput::~GameInput()
	{

	}

	void GameInput::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	{
		m_mouse->SetWindow(deviceResources->GetWindow());
	}

	void GameInput::Update()
	{
		auto kb = m_keyboard->GetState();
		m_keys.Update(kb);

		auto mouse = m_mouse->GetState();
		m_mouseButtons.Update(mouse);

		auto pad = m_gamePad0->GetState(0);
		if (pad.IsConnected())
		{
			m_buttons.Update(pad);
		}
		else
		{
			m_buttons.Reset();
		}
	}

	void GameInput::OnSuspending()
	{
		m_gamePad0->Suspend();
	}

	void GameInput::OnResuming()
	{
		m_gamePad0->Resume();

		m_keys.Reset();
		m_mouseButtons.Reset();
		m_buttons.Reset();
	}

	void GameInput::OnActivated()
	{
		m_keys.Reset();
		m_mouseButtons.Reset();
		m_buttons.Reset();
	}
}