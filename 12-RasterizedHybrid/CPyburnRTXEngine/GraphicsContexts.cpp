#include "pchlib.h"
#include "GraphicsContexts.h"

using namespace CPyburnRTXEngine;

UINT GraphicsContexts::c_descriptorSize;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GraphicsContexts::c_heap;

UINT GraphicsContexts::m_heapPositionCounter;
std::vector<UINT> GraphicsContexts::m_availableHeapPositions;
std::unordered_map<UINT, UINT> GraphicsContexts::m_multiUseHeapPositions;
std::mutex GraphicsContexts::m_mutexMultiUseHeapPositions;

Microsoft::WRL::ComPtr<ID3D12PipelineState> GraphicsContexts::m_pipelineStatePositionColorLine;
Microsoft::WRL::ComPtr<ID3D12PipelineState> GraphicsContexts::m_pipelineStatePositionColorTriangle;
Microsoft::WRL::ComPtr<ID3D12RootSignature> GraphicsContexts::m_rootSignaturePositionColor;

namespace CPyburnRTXEngine
{
	void GraphicsContexts::CreateRootSignatureAndPipelinePositionColor(ID3D12Device* d3dDevice)
	{
		CD3DX12_ROOT_PARAMETER paramCbvAll1;
		paramCbvAll1.InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0));

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAGS::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(1, &paramCbvAll1, 0, nullptr, rootSignatureFlags);

		Microsoft::WRL::ComPtr<ID3DBlob> pSignature;
		Microsoft::WRL::ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignaturePositionColor)));
	}

	GraphicsContexts::GraphicsContexts()
	{

	}

	GraphicsContexts::~GraphicsContexts()
	{
		Release();
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE GraphicsContexts::GetCpuHandle(const UINT& index)
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), index, GraphicsContexts::c_descriptorSize);
		//D3D12_CPU_DESCRIPTOR_HANDLE handle = c_heap->GetCPUDescriptorHandleForHeapStart();
		//handle.ptr += index * c_descriptorSize;
		//return handle;
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE GraphicsContexts::GetGpuHandle(const UINT& index)
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(GraphicsContexts::c_heap->GetGPUDescriptorHandleForHeapStart(), index, GraphicsContexts::c_descriptorSize);
		//D3D12_GPU_DESCRIPTOR_HANDLE handle = c_heap->GetGPUDescriptorHandleForHeapStart();
		//handle.ptr += index * c_descriptorSize;
		//return handle;
	}

	void GraphicsContexts::AddMultiHeapPosition(UINT heapPosition)
	{
		m_mutexMultiUseHeapPositions.lock();
		auto iter = m_multiUseHeapPositions.find(heapPosition);
		if (iter != m_multiUseHeapPositions.end())
		{
			iter->second++;
		}
		else
		{
			m_multiUseHeapPositions.insert(std::pair<UINT, UINT>(heapPosition, 1));
		}
		m_mutexMultiUseHeapPositions.unlock();
	}

	bool GraphicsContexts::RemoveHeapPosition(UINT heapPosition)
	{
		bool didErase = false;

		m_mutexMultiUseHeapPositions.lock();
		auto iter = m_multiUseHeapPositions.find(heapPosition);
		if (iter != m_multiUseHeapPositions.end())
		{
			// remove one from multiheap
			iter->second--;
			if (iter->second == 0)
			{
				// remove from multiheap if all are gone
				m_multiUseHeapPositions.erase(iter);
				DebugTrace("Heap position reclaimed: %s\n", (std::to_string(heapPosition)).c_str());
				m_availableHeapPositions.push_back(heapPosition);
				didErase = true;
			}
		}
		else
			m_availableHeapPositions.push_back(heapPosition); // if it wasn't a multiuse then reuse

		m_mutexMultiUseHeapPositions.unlock();

		return didErase;
	}

	UINT GraphicsContexts::GetAvailableHeapPosition()
	{
		m_mutexMultiUseHeapPositions.lock();
		// NOTE: was having issues tying to claim previously used heap so commented out for now, just incriments until it runs out for now but added a catch
		assert(m_heapPositionCounter != UINT32_MAX);

		UINT value = m_heapPositionCounter;
		// chad: right now not reusing heap positions, not important, game will never be that big
		//if (m_availableHeapPositions.size() > 0)
		//{
		//	value = m_availableHeapPositions[0];
		//	DebugTrace("Heap position reused: %s\n", (std::to_string(value)).c_str());
		//	m_availableHeapPositions.erase(m_availableHeapPositions.begin());
		//}
		//else
		{
			m_heapPositionCounter++;
			DebugTrace("Heap position used: %s\n", (std::to_string(value)).c_str());
		}

		m_mutexMultiUseHeapPositions.unlock();

		return value;
	}

	void GraphicsContexts::CreateDeviceDependentResources(ID3D12Device* d3dDevice)
	{
		c_descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = DX::DeviceResources::c_backBufferCount // Vertex constant buffers per frame 
			+ 100; // Arbitrary large number for now.
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&c_heap)));
		c_heap->SetName(L"Descriptor Heap from GraphicsContexts");
	}

	Microsoft::WRL::ComPtr<IDxcBlob> GraphicsContexts::CompileHlslLibrary(ID3D12Device* d3dDevice, std::wstring filename, std::wstring shaderType, std::wstring shaderVersion)
	{
		// Create the pipeline state, which includes compiling and loading shaders.
		Microsoft::WRL::ComPtr<IDxcCompiler> compiler;
		Microsoft::WRL::ComPtr<IDxcLibrary> library;
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;

		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
		DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));

		library->CreateBlobFromFile(GetAssetFullPath(filename.c_str()).c_str(), nullptr, &source);

		Microsoft::WRL::ComPtr<IDxcOperationResult> result;
		DX::ThrowIfFailed(compiler->Compile(
			source.Get(),
			filename.c_str(),
			shaderType.c_str(),
			shaderVersion.c_str(),
			nullptr, 0,
			nullptr, 0,
			nullptr,
			&result));

		HRESULT status;
		result->GetStatus(&status);

		if (FAILED(status))
		{
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> errors;
			result->GetErrorBuffer(&errors);

			if (errors)
			{
				OutputDebugStringA((char*)errors->GetBufferPointer());
			}

			DX::ThrowIfFailed(status);
		}

		Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
		DX::ThrowIfFailed(result->GetResult(&shaderBlob));

		return shaderBlob;
	}

	Microsoft::WRL::ComPtr<IDxcBlob> GraphicsContexts::CompileDXRLibrary(const wchar_t* filename)
	{
		auto sourceData = LoadBinaryFile(filename);

		Microsoft::WRL::ComPtr<IDxcUtils> utils;
		Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

		DX::ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
		DX::ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
		DX::ThrowIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));

		DxcBuffer source = {};
		source.Ptr = sourceData.data();
		source.Size = sourceData.size();
		source.Encoding = DXC_CP_UTF8;

		const wchar_t* arguments[] =
		{
			filename,               // REQUIRED (virtual filename)
			L"-T", L"lib_6_6",        // DXR shader library
			L"-HV", L"2021",
			L"-Zi",
			L"-Qembed_debug",
			L"-Od"
			//, L"-WX"                   // Treat warnings as errors (optional)
		};

		Microsoft::WRL::ComPtr<IDxcResult> result;
		DX::ThrowIfFailed(compiler->Compile(&source, arguments, _countof(arguments), includeHandler.Get(), IID_PPV_ARGS(&result)));

		// Check compile status
		HRESULT status;
		result->GetStatus(&status);
		if (FAILED(status))
		{
			Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
			result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
			if (errors && errors->GetStringLength())
				OutputDebugStringA(errors->GetStringPointer());

			throw std::runtime_error("DXR shader compilation failed");
		}

		// Get DXIL object
		Microsoft::WRL::ComPtr<IDxcBlob> dxil;
		HRESULT hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxil), nullptr);
		if (FAILED(hr) || !dxil || dxil->GetBufferSize() == 0)
		{
			throw std::runtime_error("Failed to retrieve DXIL object");
		}

		return dxil;
	}

	void GraphicsContexts::CreateRootSignatures(ID3D12Device* d3dDevice)
	{
	}

	void GraphicsContexts::Release()
	{
		c_heap.Reset();
	}
}

