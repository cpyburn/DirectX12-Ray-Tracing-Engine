#pragma once

// this needs to stay
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing fla

#include "BoundingBoxRenderer.h"
#include "BoundingSphereRenderer.h"
#include "BufferHeap.h"

namespace CPyburnRTXEngine
{
	class AssimpFactory
	{
	public:
		struct VertexBoneData
		{
			unsigned int IDs[4];
			float Weights[4];

			//unsigned int IDs1[4];
			//float Weights1[4];

			void AddBoneData(int boneID, float weight)
			{
				for (UINT i = 0; i < 4; i++)
				{
					if (Weights[i] == 0.0f)
					{
						IDs[i] = boneID;
						Weights[i] = weight;
						return;
					}
				}
			}
		};

		struct Model
		{
			UINT modelId = MAXUINT;
			std::string name = "not set";
			UINT meshEntryLocation = MAXUINT;
			std::string contentLocation = "not set";
			std::vector<std::string> textures;

			std::shared_ptr<AssimpFactory> assimpFactory = nullptr; // pointer to the ONE copy of the static model and resources
			std::shared_ptr<BufferHeap<AssimpFactory::VertexBoneData>> boneBuffer = nullptr; // pointer to the ONE copy of the animation bones so the resource isn't created over and over again

			AssimpFactory* GetAssimpFactoryPtr() { return assimpFactory.get(); }
		};
		static std::map<UINT, Model> Models;

		struct VSVertices
		{
			XMFLOAT3 position;
			XMFLOAT2 texture;
			XMFLOAT3 normal;
			XMFLOAT3 tangent;
		};

		struct MeshEntry
		{
			std::string name = "";
			// added missing textures as a quick fix to glass, and other things that are missing textures
			bool isMissingTextures = false;
			UINT numIndices = 0;
			UINT baseVertex = 0;
			UINT baseIndex = 0;
			bool hasBones = false;
			UINT materialIndex = 0;
			UINT numVerts = 0;
			// this transform is set by the import or skinning
			XMMATRIX meshTransform; //XMMATRIX(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

			BoundingBox boundingBox;
			BoundingSphere boundingSphere;

			std::vector<VSVertices> vertices;
			std::vector<UINT> indices;

			MeshEntry() :
				meshTransform(XMMatrixIdentity())
			{}
			~MeshEntry() {}
			//MeshEntry(MeshEntry&&) noexcept {}
			MeshEntry(const MeshEntry&) :
				meshTransform(XMMatrixIdentity())
			{}
			const MeshEntry& operator=(const MeshEntry& meshEntry)
			{
				return meshEntry;
			}
		};

		struct Bone
		{
			std::string nodeName;
			int nodeId = 0;
			bool hasBoneMapping = false;
			const aiNodeAnim* pNodeAnim = nullptr;
			XMMATRIX parentNodeTransformation = XMMatrixIdentity();
			XMMATRIX globalNodeTransform = XMMatrixIdentity();
			std::vector<std::unique_ptr<Bone>> children;

			Bone()
			{

			}

			~Bone()
			{
				for (size_t i = 0; i < children.size(); i++)
				{
					children[i] = nullptr;
				}

				children.clear();
			}
		};
	private:
		struct LoadedMaterial
		{
			std::string name;

			std::vector<std::string> albedoTextures;
			std::vector<std::string> normalTextures;
			std::vector<std::string> metallicTextures;
			std::vector<std::string> roughnessTextures;
			std::vector<std::string> aoTextures;
			std::vector<std::string> emissiveTextures;
			std::vector<std::string> opacityTextures;
		};

		DX::DeviceResources* m_deviceResources = nullptr;

		std::vector<MeshEntry> m_meshEntries;
		Model* m_ptrModel = nullptr;
		std::string m_pathFileName;
		std::string m_fileName;
		std::string m_pathDirectory;

		const aiScene* m_pScene = nullptr;
		Assimp::Importer m_importer;

		//std::vector<XMFLOAT3> m_positions; // will be used for phsyx later
		std::string m_textureDiffuse;
		std::string m_textureSPEC;
		std::string m_textureNRM;
		std::string m_textureDISP;

		bool m_isSkinned = false;

#ifdef _DEBUG
		BoundingBoxRenderer m_boundingBox;
		BoundingSphereRenderer m_boundingSphere;
#else
		BoundingBox m_boundingBox;
		BoundingSphere m_boundingSphere;
#endif
		
		XMMATRIX m_boundingSphereRadiusTranslation;

		void DoMeshTransforms(aiNode* node, XMMATRIX parentTransform);
		void CreateSingleMeshEntry(UINT i, UINT& numVertices, UINT& numIndices, MeshEntry* mesh);
		void InitializeMesh(const UINT& i, MeshEntry* meshEntry);

		// todo: decide if LoadAllMaterials is worth using
		void InitializeMaterials();
		std::vector<LoadedMaterial> LoadAllMaterials(const aiScene* scene);
		
		std::string FormatTexturePath(const std::string& path, const aiTextureType& type);

		// directX 12 buffers
		BufferHeap<AssimpFactory::VSVertices> m_vertexBuffer;
		BufferHeap<UINT> m_indexBuffer;
		std::shared_ptr<BufferHeap<AssimpFactory::VertexBoneData>> m_boneBuffer = nullptr;

		UINT m_numBones = 0;
		std::vector<XMMATRIX> m_boneInfo;
		std::unordered_map<std::string, unsigned int> m_boneMapping; // maps a bone name to its index
		std::vector<AssimpFactory::VertexBoneData> m_bones;
		void LoadBones(int meshIndex, const aiMesh* pMesh, std::vector<AssimpFactory::VertexBoneData>& bones);
	public:
#ifdef _DEBUG
		BoundingBoxRenderer& GetBoundingBoxRenderer() { return m_boundingBox; }
		BoundingSphereRenderer& GetBoundingSphereRenderer() { return m_boundingSphere; }
#else

#endif
		const std::vector<AssimpFactory::VertexBoneData>& GetBones() { return m_bones; }
		const std::unordered_map<std::string, unsigned int>& GetBoneMapping() { return m_boneMapping; }
		const std::vector<XMMATRIX>& GetBoneInfo() { return m_boneInfo; }
		const UINT& GetNumBones() const { return m_numBones; }
		const Model* GetModel() { return m_ptrModel; }
		std::vector<MeshEntry>& GetMeshEntries() { return m_meshEntries; }
		std::string GetTextureDiffuse() const { return m_textureDiffuse; }
		BufferHeap<AssimpFactory::VSVertices>* GetVertexBuffer() { return &m_vertexBuffer; }
		BufferHeap<UINT>& GetIndexBuffer() { return m_indexBuffer; }
		BufferHeap<AssimpFactory::VertexBoneData>* GetBoneBuffer() { return m_boneBuffer.get(); }
		const aiScene* GetAiScene() { return m_pScene; }

		AssimpFactory(Model* model, const std::string& fileName,
			unsigned int customFlags = aiProcess_Triangulate
			//| aiProcess_MakeLeftHanded
			| aiProcess_GenSmoothNormals
			//| aiProcess_FlipWindingOrder
			| aiProcess_FlipUVs
			| aiProcess_JoinIdenticalVertices
			| aiProcess_CalcTangentSpace
			// todo: remove the extra processing the shaders and then enable this
			| aiPostProcessSteps::aiProcess_LimitBoneWeights);
		~AssimpFactory();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void CreateBuffers(ID3D12GraphicsCommandList4* commandList);
		void CreateShaderResources();

		static void LoadJsonForAllModels();
		static AssimpFactory::Model* LoadJsonByModelId(const UINT& id);

		void ReleaseUploadResources();
		void Release();
	};
}
