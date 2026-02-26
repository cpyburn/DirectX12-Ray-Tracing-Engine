#include "pchlib.h"
#include "CameraBase.h"

namespace CPyburnRTXEngine
{
	CameraBase::CameraBase()
	{

	}

	void CameraBase::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	{
		m_deviceResources = deviceResources;
		Microsoft::WRL::ComPtr<ID3D12Device> device = deviceResources->GetD3DDevice();

        DX::ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(m_cameraCbv.AlignedSize * DX::DeviceResources::c_backBufferCount),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_cameraCbv.Resource)));

        NAME_D3D12_OBJECT(m_cameraCbv.Resource);

        // Create constant buffer views to access the upload buffer.
        D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_cameraCbv.Resource->GetGPUVirtualAddress();

        for (UINT n = 0; n < DX::DeviceResources::c_backBufferCount; n++)
        {
            m_cameraCbv.HeapIndex[n] = GraphicsContexts::GetAvailableHeapPosition();
            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), m_cameraCbv.HeapIndex[n], GraphicsContexts::c_descriptorSize);

            // Describe and create constant buffer views.
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = cbvGpuAddress;
            cbvDesc.SizeInBytes = m_cameraCbv.AlignedSize;
            device->CreateConstantBufferView(&cbvDesc, cpuHandle);

            cbvGpuAddress += m_cameraCbv.AlignedSize;
            m_cameraCbv.GpuHandle[n] = CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart(), m_cameraCbv.HeapIndex[n], GraphicsContexts::c_descriptorSize);
        }

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(m_cameraCbv.Resource->Map(0, &readRange, reinterpret_cast<void**>(&m_cameraCbv.MappedData)));
	}

	void Update(DX::StepTimer const& timer, const XMVECTOR& mousePosition)
	{

	}
}
