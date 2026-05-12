#pragma once
#include "AssimpFactory.h"
#include "AssimpAnimations.h"

namespace CPyburnRTXEngine
{
	class AnimationCompute
	{
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> baseVertices;
		Microsoft::WRL::ComPtr<ID3D12Resource> boneBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> boneMatrices;
		Microsoft::WRL::ComPtr<ID3D12Resource> outVertices;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

		std::shared_ptr<DX::DeviceResources> m_deviceResources;

	public:
		AnimationCompute();
		~AnimationCompute();

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateBuffers(const std::vector<AssimpFactory::VSVertices>& vertices, const std::vector<AssimpAnimations::VertexBoneData>& boneData, const std::vector<XMMATRIX>& bones, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator);
		void Dispatch(ID3D12GraphicsCommandList4* commandList);
		//void Initialize();
	};
}
