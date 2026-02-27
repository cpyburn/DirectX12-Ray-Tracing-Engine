#pragma once

namespace CPyburnRTXEngine
{
	class GameInput
	{
	private:
		std::unique_ptr<DirectX::Keyboard> m_keyboard;
		std::unique_ptr<DirectX::Mouse> m_mouse;
		std::unique_ptr<DirectX::GamePad> m_gamePad0;
	public:
		GameInput();
		//~GameInput();

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);

		//void Update();
		void OnSuspending();
		void OnResuming();

		Keyboard* GetKeyboard() { return m_keyboard.get(); }
		Mouse* GetMouse() { return m_mouse.get(); }
		GamePad* GetGamePad0() { return m_gamePad0.get(); }
	};
}

