#pragma once

#include "AssimpFactory.h"
#include "Animation.h"

namespace CPyburnRTXEngine
{
	class AnimationPlayer; // forward declaration
	class AnimationCompute; // forward declaration

	class AssimpAnimations : public AssimpFactory
	{
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

		float m_ticksPerSecond = 0;
		float m_duration = 0;
		std::unordered_map<std::string, unsigned int> m_boneMapping; // maps a bone name to its index
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

		void LoadJSON();
		static std::string GetAnimationTypeNameById(const UINT& id)
		{
			return AnimationTypes[id];
		}
		static UINT GetAnimationTypeIdByName(std::string name)
		{
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);
			for (auto it = AnimationTypes.begin(); it != AnimationTypes.end(); ++it)
				if (it->second == name)
					return it->first;
		}

		// add animation player
		std::unique_ptr<AnimationPlayer> m_animationPlayer = nullptr;
		std::unique_ptr<AnimationCompute> m_animationCompute = nullptr;
	public:
		static std::unordered_map<UINT, std::string> AnimationTypes;
		static std::unordered_map<UINT, std::unordered_map<UINT, std::unordered_map<std::string, Animation>>> Animations;
		
		AssimpAnimations();
		~AssimpAnimations();

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);

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
		) override;

		void BoneTransformBlended(float blendFactor, float timeInSecondsCurrent, float timeInSecondsTarget, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
		void BoneTransform(float timeInSeconds, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);

		void Update(DX::StepTimer const& timer);
		virtual void Release() override;
	};
}

