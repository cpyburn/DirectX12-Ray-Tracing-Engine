#pragma once
#include "AssimpFactory.h"
#include "AssimpAnimations.h"
#include "BufferHeap.h"

namespace CPyburnRTXEngine
{
	class AnimationCompute
	{
	private:
		BufferHeap<AssimpFactory::VSVertices>* m_baseVertices = nullptr;
		BufferHeap<AssimpAnimations::VertexBoneData> m_boneBuffer;
		BufferHeap<XMMATRIX> m_boneMatrices;
		BufferHeap<AssimpFactory::VSVertices> m_outVertices;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

		std::shared_ptr<DX::DeviceResources> m_deviceResources;

	public:
		const BufferHeap<AssimpFactory::VSVertices>& GetOutputBuffer() const { return m_outVertices; }

		AnimationCompute();
		~AnimationCompute();

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateBuffers(ID3D12GraphicsCommandList4* commandList, BufferHeap<AssimpFactory::VSVertices>* baseVertices, const std::vector<AssimpAnimations::VertexBoneData>& boneData, const std::vector<XMMATRIX>& bones);
		void CreateShaderResources();
		
		void Update(const std::vector<XMMATRIX>& bones);
		void Dispatch(ID3D12GraphicsCommandList4* commandList);
	};
}
