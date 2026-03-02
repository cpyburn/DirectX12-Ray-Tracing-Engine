#pragma once

// this needs to stay
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing fla

namespace CPyburnRTXEngine
{
	class TestAssimp
	{
	private:
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
			XMFLOAT4X4 meshTransform = XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

			BoundingBox boundingBox;
			BoundingSphere boundingSphere;

			MeshEntry() {}
			~MeshEntry() {}
			//MeshEntry(MeshEntry&&) noexcept {}
			MeshEntry(const MeshEntry&) {}
			const MeshEntry& operator=(const MeshEntry& meshEntry)
			{
				return meshEntry;
			}
		};

		struct VSVertices
		{
			XMFLOAT3 position;
			XMFLOAT2 texture;
			XMFLOAT3 normal;
			XMFLOAT3 tangent;
			XMFLOAT3 binormal; // todo: drop for ray tracing
		};

		std::string m_pathFileName;
		std::string m_fileName;
		std::string m_pathDirectory;

		const aiScene* m_pScene = nullptr;
		Assimp::Importer m_importer;

		std::vector<MeshEntry> m_meshEntries;
		std::vector<VSVertices> m_vertices;
		std::vector<UINT> m_indices;
		std::vector<XMFLOAT3> m_positions;
		std::vector<std::string> m_textureDiffuse;
		std::vector<std::string> m_textureSPEC;
		std::vector<std::string> m_textureNRM;
		std::vector<std::string> m_textureDISP;


		bool m_isSkinned = false;
		BoundingBox m_boundingBox;
		BoundingSphere m_boundingSphere;
		XMFLOAT4X4 m_boundingSphereRadiusTranslation;

		void DoMeshTransforms(aiNode* node, XMMATRIX parentTransform);
		void CreateSingleMeshEntry(UINT i, UINT& numVertices, UINT& numIndices, MeshEntry* mesh);
		void InitializeMesh(const UINT& i, MeshEntry* meshEntry, std::vector<VSVertices>& vertices, std::vector<unsigned int>& indices);
		void InitializeMaterials();
		void FormatTexturePath(const std::string& path, const std::string& appendType = "");
	public:
		TestAssimp();
		~TestAssimp();

		void Initialize(const std::string& fileName,
			unsigned int customFlags = aiProcess_Triangulate
			//| aiProcess_MakeLeftHanded
			| aiProcess_GenSmoothNormals
			//| aiProcess_FlipWindingOrder
			| aiProcess_FlipUVs
			| aiProcess_JoinIdenticalVertices
			| aiProcess_CalcTangentSpace
			// todo: remove the extra processing the shaders and then enable this
			| aiPostProcessSteps::aiProcess_LimitBoneWeights
		);
	};
}
