#pragma once

namespace CPyburnRTXEngine
{
	class FrameResource
	{
	public:
		static const int COMMAND_LIST_COUNT = 2;

		static const int COMMAND_LIST_SCENE_0 = 0;
		static const int COMMAND_LIST_POST_1 = 1;

		ID3D12CommandList* BatchSubmit[COMMAND_LIST_COUNT];
	private:
		ComPtr<ID3D12CommandAllocator> m_commandAllocators[COMMAND_LIST_COUNT];
		ComPtr<ID3D12GraphicsCommandList> m_commandLists[COMMAND_LIST_COUNT];
	public:
		FrameResource();
		~FrameResource();

		void Init(DeviceResources* deviceResources);
		ID3D12GraphicsCommandList* ResetCommandList(const int commandList, ID3D12PipelineState* pInitialState = nullptr);
		ComPtr<ID3D12GraphicsCommandList> GetCommandList(int commandList) { return m_commandLists[commandList]; }
		void Release();
	};
}
