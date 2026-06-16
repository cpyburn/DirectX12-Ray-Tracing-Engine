#pragma once

namespace CPyburnRTXEngine
{
	class CameraBase;
	class Entity;

	class EntitiesManager
	{
	private:
		DX::DeviceResources* m_deviceResources = nullptr;
	public:
		static std::unordered_map<UINT, Entity> LoadedEntities;

		EntitiesManager();
		~EntitiesManager();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void Update(DX::StepTimer const& timer, CameraBase* camera);

		static void LoadJson();
	};
}

