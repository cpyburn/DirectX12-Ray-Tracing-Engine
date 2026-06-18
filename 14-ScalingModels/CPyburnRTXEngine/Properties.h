#pragma once

#include "pchlib.h"

namespace CPyburnRTXEngine
{
	class Properties 
	{
	private:
		UINT m_id = MAXUINT;
		std::string m_name = "not set";
		XMFLOAT3 m_position = XMFLOAT3(0, 0, 0);
		XMFLOAT3 m_scale = XMFLOAT3(1, 1, 1);
		XMFLOAT3 m_rotation = XMFLOAT3(0, 0, 0);
		UINT m_modelId = MAXUINT;

		XMMATRIX m_worldTransform;

	public:
		const UINT& GetId() { return m_id; }
		void SetId(UINT id) { m_id = id; }

		const std::string& GetName() { return m_name; }
		void SetName(std::string name) { m_name = name; }

		XMVECTOR GetXMPosition() { return XMLoadFloat3(&m_position); }
		void SetPosition(XMFLOAT3 position) { m_position = position; }

		XMVECTOR GetXMScale() { return XMLoadFloat3(&m_scale); }
		void SetScale(XMFLOAT3 scale) { m_scale = scale; }

		XMVECTOR GetXMRotation() { return XMLoadFloat3(&m_rotation); }
		void SetRotation(XMFLOAT3 rotation) { m_rotation = rotation; }

		const XMMATRIX& GetXMTransform() { return m_worldTransform; }

		const UINT& GetModelId() { return m_modelId; }
		void SetModelId(UINT modelId) { m_modelId = modelId; }

		Properties() = default;
		Properties(const Properties&) = default;
		Properties& operator=(const Properties&) = default;
		~Properties() = default;

		void Update() 
		{
			m_worldTransform = XMMatrixScalingFromVector(GetXMScale()) * XMMatrixRotationRollPitchYawFromVector(GetXMRotation()) * XMMatrixTranslationFromVector(GetXMPosition());
		}
	};
}