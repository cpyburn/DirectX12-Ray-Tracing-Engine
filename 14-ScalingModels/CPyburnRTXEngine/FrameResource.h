#pragma once

namespace CPyburnRTXEngine
{
	class FrameResource
	{
	public:
		static const int COMMAND_LIST_COUNT = 2;

		static const int COMMAND_LIST_SCENE_0 = 0;
		static const int COMMAND_LIST_POST_1 = 1;

		ID3D12CommandList* BatchSubmit[COMMAND_LIST_COUNT] = {};
	private:
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[COMMAND_LIST_COUNT];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandLists[COMMAND_LIST_COUNT];
	public:
		FrameResource();
		~FrameResource();

		void Init(ID3D12Device* d3dDevice);
		ID3D12GraphicsCommandList4* ResetCommandList(const int commandListIndex, ID3D12PipelineState* pInitialState = nullptr);
		ID3D12GraphicsCommandList4* GetCommandList(int commandListIndex) 
		{ 
			return m_commandLists[commandListIndex].Get(); 
		}
		void Release();
	};
}
