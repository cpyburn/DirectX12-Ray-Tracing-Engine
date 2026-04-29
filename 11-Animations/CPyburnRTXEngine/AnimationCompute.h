#pragma once

namespace CPyburnRTXEngine
{
	class AnimationCompute
	{
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> basePositions;
		Microsoft::WRL::ComPtr<ID3D12Resource> baseNormals;
		Microsoft::WRL::ComPtr<ID3D12Resource> boneIndices;
		Microsoft::WRL::ComPtr<ID3D12Resource> boneWeights;
		Microsoft::WRL::ComPtr<ID3D12Resource> boneMatrices;

		Microsoft::WRL::ComPtr<ID3D12Resource> skinnedPositions;
		Microsoft::WRL::ComPtr<ID3D12Resource> skinnedNormals;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

	public:
		AnimationCompute();
		~AnimationCompute();

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		//void Initialize();
	};
}
