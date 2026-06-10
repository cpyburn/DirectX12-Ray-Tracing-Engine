#pragma once

#include "AssimpFactory.h"
#include "Animation.h"

namespace CPyburnRTXEngine
{
	class AnimationPlayer; // forward declaration
	class AnimationCompute; // forward declaration

	class AssimpAnimations
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
	private:
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

		AssimpFactory* m_assimpFactory = nullptr;

		float m_ticksPerSecond = 0;
		float m_duration = 0;
		std::unordered_map<std::string, unsigned int> m_boneMapping; // maps a bone name to its index
		UINT m_numBones = 0;
		std::vector<XMMATRIX> m_boneInfo;
		Bone m_rootBone;
		std::vector<VertexBoneData> m_bones;

		// for now this is a string, but may be a map of <key, object holdind everything needed>
		std::vector<Bone*> m_globalBones; // this is just a place for storing bones that are useful, have names, etc.

		bool played = false;

		XMVECTOR CalcInterpolatedScalingXM(float animationTime, const aiNodeAnim* pNodeAnim);
		XMVECTOR CalcInterpolatedRotationXM(float animationTime, const aiNodeAnim* pNodeAnim);
		XMVECTOR CalcInterpolatedPositionXM(float animationTime, const aiNodeAnim* pNodeAnim);

		int FindScaling(float animationTime, const aiNodeAnim* pNodeAnim);
		int FindRotation(float animationTime, const aiNodeAnim* pNodeAnim);
		int FindPosition(float animationTime, const aiNodeAnim* pNodeAnim);

		const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName);
		void CreateSkeletonBones(const aiNode* pNode, Bone* pBone);
		void ReadSkeletonBonesBlended(float blendFactor, float animationTimeCurrent, float animationTimeTarget, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
		void ReadSkeletonBones(float animationTime, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
		void LoadBones(int meshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& bones);

		void LoadJson();

		static std::string GetAnimationTypeNameById(const UINT& modelId) { return AnimationTypes[modelId]; }
		static UINT GetAnimationTypeIdByName(std::string name)
		{
			stringToLower(name);

			for (auto it = AnimationTypes.begin(); it != AnimationTypes.end(); ++it)
				if (it->second == name)
					return it->first;
		}

		// add animation player
		std::unique_ptr<AnimationPlayer> m_animationPlayer = nullptr;
		std::unique_ptr<AnimationCompute> m_animationCompute = nullptr;
	public:
		AssimpFactory* GetAssimpFactory() { return m_assimpFactory; }
		const UINT& GetNumBones() const { return m_numBones; }
		AnimationCompute* GetAnimationCompute() { return m_animationCompute.get(); }

		static std::unordered_map<UINT, std::string> AnimationTypes;
		static std::unordered_map<UINT, std::unordered_map<UINT, std::unordered_map<std::string, Animation>>> Animations;
		
		AssimpAnimations(AssimpFactory* assimpFactory);
		~AssimpAnimations();

		void CreateDeviceDependentResources(DX::DeviceResources* deviceResources);
		void CreateBuffers(ID3D12GraphicsCommandList4* commandList);
		void CreateShaderResources();

		void BoneTransformBlended(float blendFactor, float timeInSecondsCurrent, float timeInSecondsTarget, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);
		void BoneTransform(float timeInSeconds, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global);

		void Update(DX::StepTimer const& timer);

		void ReleaseUploadResources();
		void Release();
	};
}

