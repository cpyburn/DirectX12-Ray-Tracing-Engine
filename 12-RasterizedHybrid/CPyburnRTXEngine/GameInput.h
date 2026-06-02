#pragma once

namespace DX
{
	class DeviceResources;
}

namespace CPyburnRTXEngine
{
	class GameInput
	{
	private:
		std::unique_ptr<DirectX::Keyboard> m_keyboard;
		std::unique_ptr<DirectX::Mouse> m_mouse;
		std::unique_ptr<DirectX::GamePad> m_gamePad0;

		DirectX::Keyboard::KeyboardStateTracker m_keys;
		DirectX::Mouse::ButtonStateTracker m_mouseButtons;
		DirectX::GamePad::ButtonStateTracker m_buttons;
	public:
		GameInput();
		~GameInput();

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);

		void Update();
		void OnSuspending();
		void OnResuming();
		void OnActivated();

		DirectX::Keyboard* GetKeyboard() { return m_keyboard.get(); }
		DirectX::Mouse* GetMouse() { return m_mouse.get(); }
		DirectX::GamePad* GetGamePad0() { return m_gamePad0.get(); }

		DirectX::Keyboard::KeyboardStateTracker& GetKeyboardKeys() { return m_keys; }
		DirectX::Mouse::ButtonStateTracker& GetMouseButtons() { return m_mouseButtons; }
		DirectX::GamePad::ButtonStateTracker& GetGamePadButtons() { return m_buttons; }
	};
}

