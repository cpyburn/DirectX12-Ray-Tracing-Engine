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
		BufferHeap<AnimationStructs::VertexBoneData>* m_boneBuffer = nullptr;
		BufferHeap<XMMATRIX> m_boneMatricesBuffer[DX::DeviceResources::c_backBufferCount];
		BufferHeap<AssimpFactory::VSVertices> m_outVertexBuffer;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

		DX::DeviceResources* m_deviceResources = nullptr;

	public:
		const BufferHeap<AssimpFactory::VSVertices>& GetVertexOutputBuffer() const { return m_outVertexBuffer; }

		AnimationCompute();
		~AnimationCompute();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void CreateBuffers(ID3D12GraphicsCommandList4* commandList, BufferHeap<AssimpFactory::VSVertices>* baseVertices, BufferHeap<AnimationStructs::VertexBoneData>* boneData, const std::vector<XMMATRIX>& bones);
		void CreateShaderResources();
		
		void Update(const std::vector<XMMATRIX>& bones);
		void Dispatch(ID3D12GraphicsCommandList4* commandList);

		void ReleaseUploadResources();
	};
}
