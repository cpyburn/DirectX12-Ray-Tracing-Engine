#pragma once
#include "AssimpFactory.h"
#include "AssimpAnimations.h"
#include "BufferHeap.h"

namespace CPyburnRTXEngine
{
	class AnimationCompute
	{
	private:
		BufferHeap<AssimpFactory::VSVertices> baseVertices;
		BufferHeap<AssimpAnimations::VertexBoneData> boneBuffer;
		BufferHeap<XMFLOAT4X4> boneMatrices;
		BufferHeap<AssimpFactory::VSVertices> outVertices;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		UINT m_vertexCount = 0;

	public:
		AnimationCompute();
		~AnimationCompute();

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateBuffers(const std::vector<AssimpFactory::VSVertices>& vertices, const std::vector<AssimpAnimations::VertexBoneData>& boneData, const std::vector<XMFLOAT4X4>& bones);
		
		void Update(const std::vector<XMMATRIX>& bones);
		void Dispatch(ID3D12GraphicsCommandList4* commandList);
		
		//void Initialize();
	};
}
