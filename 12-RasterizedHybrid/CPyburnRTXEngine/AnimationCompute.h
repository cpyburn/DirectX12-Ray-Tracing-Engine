#pragma once
#include "AssimpFactory.h"
#include "AssimpAnimations.h"
#include "BufferHeap.h"

namespace CPyburnRTXEngine
{
	class AnimationCompute
	{
	private:
		BufferHeap<AssimpFactory::VSVertices>* m_baseVertexBuffer = nullptr;
		BufferHeap<AssimpAnimations::VertexBoneData> m_boneBuffer;
		BufferHeap<XMMATRIX> m_boneMatricesBuffer;
		BufferHeap<AssimpFactory::VSVertices> m_outVertexBuffer;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

		ID3D12Device5* m_d3dDevice = nullptr;

	public:
		const BufferHeap<AssimpFactory::VSVertices>& GetVertexOutputBuffer() const { return m_outVertexBuffer; }

		AnimationCompute();
		~AnimationCompute();

		void CreateDeviceDependentResources(ID3D12Device5* d3dDevice);
		void CreateBuffers(ID3D12GraphicsCommandList4* commandList, BufferHeap<AssimpFactory::VSVertices>* baseVertices, const std::vector<AssimpAnimations::VertexBoneData>& boneData, const std::vector<XMMATRIX>& bones);
		void CreateShaderResources();
		
		void Update(const std::vector<XMMATRIX>& bones);
		void Dispatch(ID3D12GraphicsCommandList4* commandList);

		void ReleaseUploadResources();
	};
}
