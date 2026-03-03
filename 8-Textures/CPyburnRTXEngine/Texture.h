#pragma once

namespace CPyburnRTXEngine
{
	class Texture
	{
	public:
		struct HeapTexture
		{
			UINT heapPosition;
			XMINT2 textureSize;
		};

	private:
		static std::mutex m_mutex;

		static std::map<UINT, Microsoft::WRL::ComPtr<ID3D12Resource>> m_texturesUpload;
		static std::map<UINT, Microsoft::WRL::ComPtr<ID3D12Resource>> m_textures;
		static std::map<std::string, HeapTexture> m_loadedTextures;
		static bool TryGetTextureHeap(const std::string& path, Texture::HeapTexture& textureHeap);



	public:
		static HeapTexture LoadTextureHeap(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::string& path, ID3D12GraphicsCommandList* commandList);
		static void Release();
		static void ReleaseUploadByHeapPosition(UINT heapPosition);
		static Texture::HeapTexture LoadCustomTexture(const std::shared_ptr<DX::DeviceResources>& deviceResources, ID3D12GraphicsCommandList* commandList, const std::wstring& wFileName);
		static Texture::HeapTexture LoadCustomTexture(const std::shared_ptr<DX::DeviceResources>& deviceResources, ID3D12GraphicsCommandList* commandList, const DXGI_FORMAT& format, const UINT& width, const UINT& height, const uint8_t* data, const size_t& rowPitch, const std::wstring& wFileName);
		static void RemoveHeapPosition(UINT position);
		~Texture();
	};
}


