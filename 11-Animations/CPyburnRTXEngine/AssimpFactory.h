#pragma once

// this needs to stay
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing fla

namespace CPyburnRTXEngine
{
	class AssimpFactory
	{
	public:
		struct VSVertices
		{
			XMFLOAT3 position;
			XMFLOAT2 texture;
			XMFLOAT3 normal;
			XMFLOAT3 tangent;
			XMFLOAT3 binormal; // todo: drop for ray tracing
		};

	private:
		struct Bone
		{
			std::string nodeName;
			int nodeId = 0;
			bool hasBoneMapping = false;
			const aiNodeAnim* pNodeAnim = nullptr;
			XMFLOAT4X4 parentNodeTransformation = XMFLOAT4X4(1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1);
			XMFLOAT4X4 globalNodeTransform = XMFLOAT4X4(1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1);
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

		struct VertexBoneData
		{
			unsigned int IDs[4];
			float Weights[4];

			//unsigned int IDs1[4];
			//float Weights1[4];

			void AddBoneData(int boneID, float weight);
		};

		struct GlobalBoneTransform
		{
			std::string pBoneName;
			XMFLOAT4X4* pGlobalTransform = nullptr;

			GlobalBoneTransform(std::string pBoneName, XMFLOAT4X4* pGlobalTransform)
			{
				this->pBoneName = pBoneName;
				this->pGlobalTransform = pGlobalTransform;
			}
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

		std::vector<MeshEntry> m_meshEntries;
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
		BoundingBox m_boundingBox;
		BoundingSphere m_boundingSphere;
		XMMATRIX m_boundingSphereRadiusTranslation;

		void DoMeshTransforms(aiNode* node, XMMATRIX parentTransform);
		void CreateSingleMeshEntry(UINT i, UINT& numVertices, UINT& numIndices, MeshEntry* mesh);
		void InitializeMesh(const UINT& i, MeshEntry* meshEntry);

		// todo: decide if LoadAllMaterials is worth using
		void InitializeMaterials();
		std::vector<LoadedMaterial> LoadAllMaterials(const aiScene* scene);
		
		std::string FormatTexturePath(const std::string& path, const aiTextureType& type);

		float m_ticksPerSecond = 0;
		float m_duration = 0;
		std::map<std::string, unsigned int> m_boneMapping; // maps a bone name to its index
		unsigned int m_numBones = 0;
		std::vector<XMFLOAT4X4> m_boneInfo;
		Bone m_rootBone;
		std::vector<VertexBoneData> m_bones;

		// for now this is a string, but may be a map of <key, object holdind everything needed>
		std::vector<Bone*> m_globalBones;

		void CalcInterpolatedScaling(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedRotation(aiQuaternion& out, float animationTime, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedPosition(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim);
		int FindScaling(float animationTime, const aiNodeAnim* pNodeAnim);
		int FindRotation(float animationTime, const aiNodeAnim* pNodeAnim);
		int FindPosition(float animationTime, const aiNodeAnim* pNodeAnim);
		const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName);
		void CreateSkeletonBones(const aiNode* pNode, Bone* pBone);

		void ReadSkeletonBonesBlended(float blendFactor, float animationTimeCurrent, float animationTimeTarget, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
		void ReadSkeletonBones(float animationTime, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
		void LoadBones(int meshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& bones);
	public:
		std::vector<MeshEntry>& GetMeshEntries() { return m_meshEntries; }
		std::string GetTextureDiffuse() const { return m_textureDiffuse; }

		AssimpFactory();
		~AssimpFactory();

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

		void BoneTransformBlended(float blendFactor, float timeInSecondsCurrent, float timeInSecondsTarget, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
		void BoneTransform(float timeInSeconds, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
	};
}
