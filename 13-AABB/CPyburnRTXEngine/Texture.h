#pragma once

namespace CPyburnRTXEngine
{
	class Texture
	{
	public:
		struct HeapTexture
		{
			UINT heapPosition = MAXUINT;
			UINT indexInMaterialBuffer = 0;
			XMINT2 textureSize = XMINT2(0,0);
		};

	private:
		static UINT m_textureStartOnHeap;
		static std::mutex m_mutex;

		static ID3D12Device* m_d3dDevice;

		static std::unordered_map<UINT, Microsoft::WRL::ComPtr<ID3D12Resource>> m_texturesUpload;
		static std::unordered_map<UINT, Microsoft::WRL::ComPtr<ID3D12Resource>> m_textures;
		static std::unordered_map<std::string, HeapTexture> m_loadedTextures;
		static bool TryGetTextureHeap(const std::string& path, Texture::HeapTexture& textureHeap);
		static Texture::HeapTexture LoadCustomDDSTexture(ID3D12GraphicsCommandList* commandList, const std::wstring& wFileName);
		static Texture::HeapTexture LoadCustomWICTexture(ID3D12GraphicsCommandList* commandList, const std::wstring& wFileName);

	public:
		static void CreateDeviceDependentResources(ID3D12Device* m_d3dDevice);
		static HeapTexture LoadTextureHeap(const std::string& path, ID3D12GraphicsCommandList* commandList);
		static void Release();
		static void ReleaseUploadByHeapPosition(UINT heapPosition);

		static Texture::HeapTexture LoadCustomTexture(ID3D12GraphicsCommandList* commandList, const DXGI_FORMAT& format, const UINT& width, const UINT& height, const uint8_t* data, const size_t& rowPitch, const std::wstring& wFileName);
		static void RemoveHeapPosition(UINT position);
		~Texture();
	};
}


