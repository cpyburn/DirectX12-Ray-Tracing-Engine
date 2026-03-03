#include "pchlib.h"
#include "Texture.h"

//#include <>
//
//namespace CPyburnRTXEngine
//{
//    std::map<UINT, Microsoft::WRL::ComPtr<ID3D12Resource>> Texture::m_texturesUpload;
//    std::map<UINT, Microsoft::WRL::ComPtr<ID3D12Resource>> Texture::m_textures;
//    std::map<std::string, Texture::HeapTexture> Texture::m_loadedTextures;
//    std::mutex Texture::m_mutex;
//
//    bool Texture::TryGetTextureHeap(const std::string& spath, Texture::HeapTexture& textureHeap)
//    {
//        bool didLock = m_mutex.try_lock();
//
//        std::wstring wpath = std::wstring(spath.begin(), spath.end());
//
//        wchar_t drive[_MAX_DRIVE];
//        wchar_t dir[_MAX_DIR];
//        wchar_t fname[_MAX_FNAME];
//        wchar_t ext[_MAX_EXT];
//        _wsplitpath_s(wpath.c_str(), drive, dir, fname, ext);
//
//        // put extension to lower to check for type
//        std::wstring extension = (std::wstring)ext;
//		std::wstring wFileName = (std::wstring)fname + extension;
//        wFileName.erase(std::remove(wFileName.begin(), wFileName.end(), '\r'), wFileName.end());
//        transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
//
//        auto textureIter = m_loadedTextures.find(wstringToString(wFileName));
//        if (textureIter != m_loadedTextures.end())
//        {
//            GraphicsContexts::AddMultiHeapPosition(textureIter->second.heapPosition);
//            textureHeap = textureIter->second;
//
//            if (didLock)
//            {
//                m_mutex.unlock();
//            }
//
//            return true;
//        }
//
//        if (didLock)
//        {
//            m_mutex.unlock();
//        }
//
//        return false;
//    }
//
//    Texture::HeapTexture Texture::LoadTextureHeap(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::string& spath, ID3D12GraphicsCommandList* commandList)
//    {
//		m_mutex.lock();
//		// check to see if texture exists
//		Texture::HeapTexture heapTexture;
//		if (Texture::TryGetTextureHeap(spath, heapTexture))
//		{
//			m_mutex.unlock();
//			// it does, so return it
//			return heapTexture;
//		}
//
//		// texture doesn't exist so load it
//		std::wstring wpath = std::wstring(spath.begin(), spath.end());
//
//		wchar_t drive[_MAX_DRIVE];
//		wchar_t dir[_MAX_DIR];
//		wchar_t fname[_MAX_FNAME];
//		wchar_t ext[_MAX_EXT];
//		_wsplitpath_s(wpath.c_str(), drive, dir, fname, ext);
//
//		// put extension to lower to check for type
//		std::wstring extension = (std::wstring)ext;
//		std::wstring wFileName = (std::wstring)fname + extension;
//		wFileName.erase(std::remove(wFileName.begin(), wFileName.end(), '\r'), wFileName.end());
//		transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
//
//		//auto textureIter = LoadedTextures.find(wFileName);
//		//if (textureIter != LoadedTextures.end())
//		//{
//		//	GraphicsContexts::AddMultiHeapPosition(textureIter->second.heapPosition);
//		//	mutex.unlock();
//		//	return textureIter->second;
//		//}
//
//		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
//		UINT width = 1;
//		UINT height = 1;
//		uint8_t* data = nullptr;
//		size_t rowPitch;
//		
//		std::wstring szFile = (std::wstring)dir + wFileName;
//
//		if (wFileName.compare(L"white") == 0)
//		{
//			data = new uint8_t[4];
//			data[0] = 0xff;
//			data[1] = 0xff;
//			data[2] = 0xff;
//			data[3] = 0xff;
//			rowPitch = 4;
//
//			heapTexture = LoadCustomTexture(deviceResources, commandList, format, width, height, data, rowPitch, wFileName);
//			delete[] data;
//			data = nullptr;
//		}
//		//else if (wFileName.compare(L"transparent") == 0)
//		//{
//		//	data = new uint8_t[4];
//		//	data[0] = 0x00;
//		//	data[1] = 0x00;
//		//	data[2] = 0x00;
//		//	data[3] = 0x00;
//		//	rowPitch = 4;
//		//}
//		else
//		{
//			struct _stat buffer;
//			if (_wstat(szFile.c_str(), &buffer) != 0)
//			{
//				szFile.append(L" not found, using test image instead.\n");
//				OutputDebugStringA(wstringToString(szFile).c_str());
//				extension = L".tga";
//				wFileName = L"test" + extension;
//				szFile = L"Assets\\" + wFileName;
//
//				// also change the extension because that is used to call the correct file
//				HeapTexture testTextureHeap;
//				if (TryGetTextureHeap(wstringToString(szFile), testTextureHeap))
//				{
//					return testTextureHeap;
//					m_mutex.unlock();
//				}
//			}
//
//#pragma region Get Image
//			ScratchImage image;
//			if (extension.compare(L".dds") == 0)
//			{
//				DirectX::LoadFromDDSFile(szFile.c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, nullptr, image);
//			}
//			else if (extension.compare(L".tga") == 0)
//			{
//				DirectX::LoadFromTGAFile(szFile.c_str(), nullptr, image);
//			}
//			else
//			{
//				DirectX::LoadFromWICFile(szFile.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_NONE, nullptr, image);
//			}
//
//			format = image.GetImages()->format;
//			width = image.GetImages()->width;
//			height = image.GetImages()->height;
//			data = image.GetImages()->pixels;
//			rowPitch = image.GetImages()->rowPitch;
//
//			heapTexture = LoadCustomTexture(deviceResources, commandList, format, width, height, data, rowPitch, wFileName);
//			image.Release();
//#pragma endregion
//		}
//
//		m_mutex.unlock();
//
//		return heapTexture;
//    }
//
//    void Texture::Release()
//    {
//		m_mutex.lock();
//		// todo: revisit this
//		Texture::m_textures.clear();
//		Texture::m_loadedTextures.clear();
//		Texture::m_texturesUpload.clear();
//		m_mutex.unlock();
//    }
//
//    void Texture::ReleaseUploadByHeapPosition(UINT heapPosition)
//    {
//		m_mutex.lock();
//		m_texturesUpload.erase(heapPosition);
//		m_mutex.unlock();
//    }
//
//    Texture::HeapTexture Texture::LoadCustomTexture(const std::shared_ptr<DX::DeviceResources>& deviceResources, ID3D12GraphicsCommandList* commandList, const DXGI_FORMAT& format, const UINT& width, const UINT& height, const uint8_t* data, const size_t& rowPitch, const std::wstring& wFileName)
//    {
//		bool didLock = m_mutex.try_lock();
//
//		// check to see if texture exists
//		Texture::HeapTexture heapTexture;
//		if (Texture::TryGetTextureHeap(wstringToString(wFileName), heapTexture))
//		{
//			if (didLock)
//			{
//				m_mutex.unlock();
//			}
//
//			// it does, so return it
//			return heapTexture;
//		}
//
//		Microsoft::WRL::ComPtr<ID3D12Resource> tex;
//		Microsoft::WRL::ComPtr<ID3D12Resource> uploadRes;
//
//		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
//
//		ID3D12Device* d3dDevice = deviceResources->GetD3DDevice();
//
//		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
//			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//			D3D12_HEAP_FLAG_NONE,
//			&desc,
//			D3D12_RESOURCE_STATE_COMMON,
//			nullptr,
//			IID_PPV_ARGS(&tex)));
//
//		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1); // +D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
//
//		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
//			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//			D3D12_HEAP_FLAG_NONE,
//			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
//			D3D12_RESOURCE_STATE_GENERIC_READ,
//			nullptr,
//			IID_PPV_ARGS(&uploadRes)));
//
//		tex->SetName(wFileName.c_str());
//		uploadRes->SetName(wFileName.c_str());
//
//		// Upload the Shader Resource to the GPU.
//		{
//			// Copy data to the intermediate upload heap and then schedule a copy
//			// from the upload heap to the texture.
//			D3D12_SUBRESOURCE_DATA textureData = {};
//			textureData.pData = data;
//			textureData.RowPitch = rowPitch;
//			textureData.SlicePitch = textureData.RowPitch * height;
//
//			UpdateSubresources(commandList, tex.Get(), uploadRes.Get(), 0, 0, 1, &textureData);
//
//			CD3DX12_RESOURCE_BARRIER srvBufferResourceBarrier =
//				CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
//			commandList->ResourceBarrier(1, &srvBufferResourceBarrier);
//
//			// Describe and create a SRV for the texture.
//			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
//			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//			srvDesc.Format = desc.Format;
//			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
//			srvDesc.Texture2D.MipLevels = desc.MipLevels;
//
//			UINT heapPostion = GraphicsContexts::GetAvailableHeapPosition();
//			CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(GraphicsContexts::c_heap->GetCPUDescriptorHandleForHeapStart(), heapPostion, GraphicsContexts::c_descriptorSize);
//			d3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, cbvCpuHandle);
//
//			HeapTexture heapTexture;
//			heapTexture.heapPosition = heapPostion;
//			heapTexture.textureSize = XMINT2(width, height);
//
//			Texture::m_loadedTextures.insert(std::pair<std::string, HeapTexture>(wstringToString(wFileName), heapTexture));
//			Texture::m_textures.insert(std::pair<UINT, Microsoft::WRL::ComPtr<ID3D12Resource> >(heapPostion, tex));
//			Texture::m_texturesUpload.insert(std::pair<UINT, Microsoft::WRL::ComPtr<ID3D12Resource> >(heapPostion, uploadRes));
//			if (didLock)
//			{
//				m_mutex.unlock();
//			}
//
//			GraphicsContexts::AddMultiHeapPosition(heapPostion);
//
//			return m_loadedTextures[wstringToString(wFileName)];
//		}
//    }
//
//    void Texture::RemoveHeapPosition(UINT position)
//    {
//        bool didErase = GraphicsContexts::RemoveHeapPosition(position);
//        if (!didErase)
//        {
//            return;
//        }
//
//        m_mutex.lock();
//        for (auto textureIter = m_loadedTextures.begin(); textureIter != m_loadedTextures.end(); textureIter++)
//        {
//            if (textureIter->second.heapPosition == position)
//            {
//                m_loadedTextures.erase(textureIter);
//                break;
//            }
//        }
//
//        auto textureIter1 = m_textures.find(position);
//        if (textureIter1 != m_textures.end())
//        {
//            m_textures.erase(textureIter1);
//        }
//
//        auto textureIter2 = m_texturesUpload.find(position);
//        if (textureIter2 != m_texturesUpload.end())
//        {
//            m_texturesUpload.erase(textureIter2);
//        }
//        m_mutex.unlock();
//    }
//
//    Texture::~Texture()
//    {
//
//    }
//}
//
//
