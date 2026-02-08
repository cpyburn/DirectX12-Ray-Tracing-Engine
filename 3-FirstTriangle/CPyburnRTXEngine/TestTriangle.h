#pragma once

namespace CPyburnRTXEngine
{
	class TestTriangle
	{
	private:
		
	public:
		TestTriangle();
		~TestTriangle();
		void Update(DX::StepTimer const& timer);
		void Render();
		void Release();
	};
}
