#pragma once
#include "AssimpFactory.h"
#include "AssimpAnimations.h"

namespace CPyburnRTXEngine
{
	class AnimationCompute
	{
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> baseVertices;
		UINT m_heapIndexBaseVertices = MAXUINT;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuBaseVertices;
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_gpuBaseVertices;

		Microsoft::WRL::ComPtr<ID3D12Resource> boneBuffer;
		UINT m_heapIndexBoneBuffer = MAXUINT;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuBoneBuffer;

		Microsoft::WRL::ComPtr<ID3D12Resource> boneMatrices;
		UINT m_heapIndexBoneMatrices = MAXUINT;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuBoneMatrices;

		Microsoft::WRL::ComPtr<ID3D12Resource> outVertices;
		UINT m_heapIndexOutVertices = MAXUINT;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuOutVertices;
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_gpuOutVertices;

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
