#include "pchlib.h"
#include "AssimpAnimations.h"

#include "AnimationPlayer.h"
#include "AnimationCompute.h"

namespace CPyburnRTXEngine
{
	std::unordered_map<UINT, std::string> AssimpAnimations::AnimationTypes;
	std::unordered_map<UINT, std::unordered_map<UINT, std::unordered_map<std::string, Animation>>> AssimpAnimations::Animations;
	
	void AssimpAnimations::CalcInterpolatedScaling(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim)
	{
		if (pNodeAnim->mNumScalingKeys == 1)
		{
			out = pNodeAnim->mScalingKeys[0].mValue;
			return;
		}

		int scalingIndex = FindScaling(animationTime, pNodeAnim);
		unsigned int nextScalingIndex = (scalingIndex + 1);
		float deltaTime = (float)(pNodeAnim->mScalingKeys[nextScalingIndex].mTime - pNodeAnim->mScalingKeys[scalingIndex].mTime);
		float factor = (animationTime - (float)pNodeAnim->mScalingKeys[scalingIndex].mTime) / deltaTime;
		const aiVector3D& start = pNodeAnim->mScalingKeys[scalingIndex].mValue;
		const aiVector3D& end = pNodeAnim->mScalingKeys[nextScalingIndex].mValue;
		aiVector3D delta = end - start;
		out = start + factor * delta;
	}

	void AssimpAnimations::CalcInterpolatedRotation(aiQuaternion& out, float animationTime, const aiNodeAnim* pNodeAnim)
	{
		if (pNodeAnim->mNumRotationKeys == 1)
		{
			out = pNodeAnim->mRotationKeys[0].mValue;
			return;
		}

		int rotationIndex = FindRotation(animationTime, pNodeAnim);
		unsigned int nextRotationIndex = (rotationIndex + 1);
		float deltaTime = (float)(pNodeAnim->mRotationKeys[nextRotationIndex].mTime - pNodeAnim->mRotationKeys[rotationIndex].mTime);
		float factor = (animationTime - (float)pNodeAnim->mRotationKeys[rotationIndex].mTime) / deltaTime;
		const aiQuaternion& startRotationQ = pNodeAnim->mRotationKeys[rotationIndex].mValue;
		const aiQuaternion& endRotationQ = pNodeAnim->mRotationKeys[nextRotationIndex].mValue;
		aiQuaternion::Interpolate(out, startRotationQ, endRotationQ, factor);
		out = out.Normalize();
	}

	void AssimpAnimations::CalcInterpolatedPosition(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim)
	{
		if (pNodeAnim->mNumPositionKeys == 1)
		{
			out = pNodeAnim->mPositionKeys[0].mValue;
			return;
		}

		int positionIndex = FindPosition(animationTime, pNodeAnim);
		unsigned int nextPositionIndex = (positionIndex + 1);
		float deltaTime = (float)(pNodeAnim->mPositionKeys[nextPositionIndex].mTime - pNodeAnim->mPositionKeys[positionIndex].mTime);
		float factor = (animationTime - (float)pNodeAnim->mPositionKeys[positionIndex].mTime) / deltaTime;
		const aiVector3D& start = pNodeAnim->mPositionKeys[positionIndex].mValue;
		const aiVector3D& end = pNodeAnim->mPositionKeys[nextPositionIndex].mValue;
		aiVector3D delta = end - start;
		out = start + factor * delta;
	}

	int AssimpAnimations::FindScaling(float animationTime, const aiNodeAnim* pNodeAnim)
	{
		for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
		{
			if (animationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime)
			{
				return i;
			}
		}
		return 0;
	}

	int AssimpAnimations::FindRotation(float animationTime, const aiNodeAnim* pNodeAnim)
	{
		for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
		{
			if (animationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime)
			{
				return i;
			}
		}
		return 0;
	}

	int AssimpAnimations::FindPosition(float animationTime, const aiNodeAnim* pNodeAnim)
	{
		for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
		{
			if (animationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime)
			{
				return i;
			}
		}
		return 0;
	}

	const aiNodeAnim* AssimpAnimations::FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName)
	{
		for (unsigned int i = 0; i < pAnimation->mNumChannels; i++)
		{
			const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];

			if (std::string(pNodeAnim->mNodeName.data) == nodeName)
			{
				return pNodeAnim;
			}
		}

		return NULL;
	}

	void AssimpAnimations::CreateSkeletonBones(const aiNode* pNode, Bone* pBone)
	{
		pBone->nodeName = pNode->mName.data;
		pBone->pNodeAnim = FindNodeAnim(m_pScene->mAnimations[0], pBone->nodeName);
		pBone->parentNodeTransformation = XMFLOAT4X4(pNode->mTransformation.a1, pNode->mTransformation.a2, pNode->mTransformation.a3, pNode->mTransformation.a4,
			pNode->mTransformation.b1, pNode->mTransformation.b2, pNode->mTransformation.b3, pNode->mTransformation.b4,
			pNode->mTransformation.c1, pNode->mTransformation.c2, pNode->mTransformation.c3, pNode->mTransformation.c4,
			pNode->mTransformation.d1, pNode->mTransformation.d2, pNode->mTransformation.d3, pNode->mTransformation.d4);

		//pBone->parentNodeTransformation = XMFLOAT4X4(pNode->mTransformation.a1, pNode->mTransformation.b1, pNode->mTransformation.c1, pNode->mTransformation.d1,
		//											pNode->mTransformation.a2, pNode->mTransformation.b2, pNode->mTransformation.c2, pNode->mTransformation.d2,
		//											pNode->mTransformation.a3, pNode->mTransformation.b3, pNode->mTransformation.c3, pNode->mTransformation.d3,
		//											pNode->mTransformation.a4, pNode->mTransformation.b4, pNode->mTransformation.c4, pNode->mTransformation.d4);

		// this is a good place to point the global transform to the ModelNodeInfo tranform, needed for giving skinned models weapons, emitting particles, and emitting sounds in the right position
		//for (auto& emitter : m_emitters)
		//{
		//	std::string nodeNameToLower = stringToLowerReturned(pBone->nodeName);
		//	std::string emittersNodeNameToLower = stringToLowerReturned(emitter.emitterInfo->nodeName);
		//	if (nodeNameToLower.compare(emittersNodeNameToLower) == 0)
		//	{
		//		// store the address of the node to the emitter
		//		emitter.pGlobalTransform = &pBone->globalNodeTransform;
		//	}
		//}

		if (m_boneMapping.find(pBone->nodeName) != m_boneMapping.end())
		{
			pBone->hasBoneMapping = true;
			pBone->nodeId = m_boneMapping[pBone->nodeName];

			// store useful bones
			m_globalBones.push_back(pBone);
		}

		for (unsigned int i = 0; i < pNode->mNumChildren; i++)
		{
			pBone->children.push_back(std::make_unique<Bone>());
			CreateSkeletonBones(pNode->mChildren[i], pBone->children.back().get());
		}
	}

	void AssimpAnimations::ReadSkeletonBonesBlended(float blendFactor, float animationTimeCurrent, float animationTimeTarget, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		XMMATRIX nodeTransformation = XMLoadFloat4x4(&pBone->parentNodeTransformation);

		if (pBone->pNodeAnim)
		{
			// scaling
			aiVector3D scalingCurrent;
			CalcInterpolatedScaling(scalingCurrent, animationTimeCurrent, pBone->pNodeAnim);
			XMVECTOR xmScalingCurrent = { scalingCurrent.x, scalingCurrent.y, scalingCurrent.z };

			aiVector3D scalingTarget;
			CalcInterpolatedScaling(scalingTarget, animationTimeTarget, pBone->pNodeAnim);
			XMVECTOR xmScalingTarget = { scalingTarget.x, scalingTarget.y, scalingTarget.z };

			XMVECTOR xmScaling = XMVectorLerp(xmScalingCurrent, xmScalingTarget, blendFactor);
			XMMATRIX scalingBlendedM = XMMatrixScalingFromVector(xmScaling);


			// rotation
			aiQuaternion rotationCurrentQ;
			CalcInterpolatedRotation(rotationCurrentQ, animationTimeCurrent, pBone->pNodeAnim);

			aiQuaternion rotationTargetQ;
			CalcInterpolatedRotation(rotationTargetQ, animationTimeTarget, pBone->pNodeAnim);

			aiQuaternion rotationBlendedQ;
			aiQuaternion::Interpolate(rotationBlendedQ, rotationCurrentQ, rotationTargetQ, blendFactor);

			XMFLOAT3X3 rotationBlended(rotationBlendedQ.GetMatrix().a1, rotationBlendedQ.GetMatrix().b1, rotationBlendedQ.GetMatrix().c1,
				rotationBlendedQ.GetMatrix().a2, rotationBlendedQ.GetMatrix().b2, rotationBlendedQ.GetMatrix().c2,
				rotationBlendedQ.GetMatrix().a3, rotationBlendedQ.GetMatrix().b3, rotationBlendedQ.GetMatrix().c3);
			XMMATRIX rotationBlendedM = XMLoadFloat3x3(&rotationBlended);


			// translation
			aiVector3D translationCurrent;
			CalcInterpolatedPosition(translationCurrent, animationTimeCurrent, pBone->pNodeAnim);
			XMVECTOR xmTranslationCurrent = { translationCurrent.x, translationCurrent.y, translationCurrent.z };

			aiVector3D translationTarget;
			CalcInterpolatedPosition(translationTarget, animationTimeTarget, pBone->pNodeAnim);
			XMVECTOR xmTranslationTarget = { translationTarget.x, translationTarget.y, translationTarget.z };

			XMVECTOR xmTranslation = XMVectorLerp(xmTranslationCurrent, xmTranslationTarget, blendFactor);
			XMMATRIX translationBlendedM = XMMatrixTranslationFromVector(xmTranslation);

			nodeTransformation = scalingBlendedM * rotationBlendedM * translationBlendedM;
		}

		XMMATRIX xmTransposedNodeTransformation = XMMatrixTranspose(nodeTransformation);
		XMMATRIX globalTransformation = parent * xmTransposedNodeTransformation;

		if (pBone->hasBoneMapping)
		{
			noGlobalBones[pBone->nodeId] = xmTransposedNodeTransformation * XMLoadFloat4x4(&m_boneInfo[pBone->nodeId]);
			global[pBone->nodeId] = parent;

			XMMATRIX finalTransformationConversion = parent * noGlobalBones[pBone->nodeId];
			bones[pBone->nodeId] = finalTransformationConversion;

			// transpose this for easy math for modelNodes and LOD's
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation)); //XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f) * XMMatrixScaling(0.036f, 0.036f, 0.036f) * XMMatrixTranslation(0.09f, 0.04f, 0.0f)  // * XMMatrixTranspose(XMMatrixRotationRollPitchYaw(0.0f, 1.5708f, 1.5708f))
		}
		else if (pBone->pNodeAnim) // give the areas that don't have bones but do have animation the global transform
		{
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation));
		}

		for (size_t i = 0; i < pBone->children.size(); i++)
		{
			ReadSkeletonBonesBlended(blendFactor, animationTimeCurrent, animationTimeTarget, pBone->children[i].get(), globalTransformation, bones, noGlobalBones, global);
		}
	}

	void AssimpAnimations::ReadSkeletonBones(float animationTime, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		XMMATRIX nodeTransformation = XMLoadFloat4x4(&pBone->parentNodeTransformation);

		if (pBone->pNodeAnim)
		{
			aiVector3D scaling;
			CalcInterpolatedScaling(scaling, animationTime, pBone->pNodeAnim);
			XMMATRIX scalingM = XMMatrixScaling(scaling.x, scaling.y, scaling.z);

			aiQuaternion rotationQ;
			CalcInterpolatedRotation(rotationQ, animationTime, pBone->pNodeAnim);

			XMFLOAT3X3 rotation(rotationQ.GetMatrix().a1, rotationQ.GetMatrix().b1, rotationQ.GetMatrix().c1,
				rotationQ.GetMatrix().a2, rotationQ.GetMatrix().b2, rotationQ.GetMatrix().c2,
				rotationQ.GetMatrix().a3, rotationQ.GetMatrix().b3, rotationQ.GetMatrix().c3);
			XMMATRIX rotationM = XMLoadFloat3x3(&rotation);

			aiVector3D translation;
			CalcInterpolatedPosition(translation, animationTime, pBone->pNodeAnim);
			XMMATRIX translationM = XMMatrixTranslation(translation.x, translation.y, translation.z);

			nodeTransformation = scalingM * rotationM * translationM;
		}

		XMMATRIX xmTransposedNodeTransformation = XMMatrixTranspose(nodeTransformation);
		XMMATRIX globalTransformation = parent * xmTransposedNodeTransformation;

		if (pBone->hasBoneMapping)
		{
			noGlobalBones[pBone->nodeId] = xmTransposedNodeTransformation * XMLoadFloat4x4(&m_boneInfo[pBone->nodeId]);
			global[pBone->nodeId] = parent;

			XMMATRIX finalTransformationConversion = parent * noGlobalBones[pBone->nodeId];
			bones[pBone->nodeId] = finalTransformationConversion;

			// transpose this for easy math for modelNodes and LOD's
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation)); //XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f) * XMMatrixScaling(0.036f, 0.036f, 0.036f) * XMMatrixTranslation(0.09f, 0.04f, 0.0f)  // * XMMatrixTranspose(XMMatrixRotationRollPitchYaw(0.0f, 1.5708f, 1.5708f))
		}
		else if (pBone->pNodeAnim) // give the areas that don't have bones but do have animation the global transform
		{
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation));
		}

		for (size_t i = 0; i < pBone->children.size(); i++)
		{
			ReadSkeletonBones(animationTime, pBone->children[i].get(), globalTransformation, bones, noGlobalBones, global);
		}
	}

	void AssimpAnimations::LoadBones(int meshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& bones)
	{
		for (unsigned int i = 0; i < pMesh->mNumBones; i++)
		{
			int boneIndex = 0;
			std::string BoneName(pMesh->mBones[i]->mName.data);

			if (m_boneMapping.find(BoneName) == m_boneMapping.end())
			{
				// Allocate an index for bone
				boneIndex = m_numBones;
				m_numBones++;
				XMFLOAT4X4 boneOffsetMatrix = XMFLOAT4X4(pMesh->mBones[i]->mOffsetMatrix.a1, pMesh->mBones[i]->mOffsetMatrix.a2, pMesh->mBones[i]->mOffsetMatrix.a3, pMesh->mBones[i]->mOffsetMatrix.a4,
					pMesh->mBones[i]->mOffsetMatrix.b1, pMesh->mBones[i]->mOffsetMatrix.b2, pMesh->mBones[i]->mOffsetMatrix.b3, pMesh->mBones[i]->mOffsetMatrix.b4,
					pMesh->mBones[i]->mOffsetMatrix.c1, pMesh->mBones[i]->mOffsetMatrix.c2, pMesh->mBones[i]->mOffsetMatrix.c3, pMesh->mBones[i]->mOffsetMatrix.c4,
					pMesh->mBones[i]->mOffsetMatrix.d1, pMesh->mBones[i]->mOffsetMatrix.d2, pMesh->mBones[i]->mOffsetMatrix.d3, pMesh->mBones[i]->mOffsetMatrix.d4);

				//XMFLOAT4X4 boneOffsetMatrix = XMFLOAT4X4(pMesh->mBones[i]->mOffsetMatrix.a1, pMesh->mBones[i]->mOffsetMatrix.b1, pMesh->mBones[i]->mOffsetMatrix.c1, pMesh->mBones[i]->mOffsetMatrix.d1,
				//										pMesh->mBones[i]->mOffsetMatrix.a2, pMesh->mBones[i]->mOffsetMatrix.b2, pMesh->mBones[i]->mOffsetMatrix.c2, pMesh->mBones[i]->mOffsetMatrix.d2,
				//										pMesh->mBones[i]->mOffsetMatrix.a3, pMesh->mBones[i]->mOffsetMatrix.b3, pMesh->mBones[i]->mOffsetMatrix.c3, pMesh->mBones[i]->mOffsetMatrix.d3,
				//										pMesh->mBones[i]->mOffsetMatrix.a4, pMesh->mBones[i]->mOffsetMatrix.b4, pMesh->mBones[i]->mOffsetMatrix.c4, pMesh->mBones[i]->mOffsetMatrix.d4);

				m_boneInfo.push_back(boneOffsetMatrix);
				m_boneMapping[BoneName] = boneIndex;
			}
			else
			{
				boneIndex = m_boneMapping[BoneName];
			}

			for (unsigned int j = 0; j < pMesh->mBones[i]->mNumWeights; j++)
			{
				int vertexID = m_meshEntries[meshIndex].baseVertex + pMesh->mBones[i]->mWeights[j].mVertexId;
				float weight = pMesh->mBones[i]->mWeights[j].mWeight;
				bones[vertexID].AddBoneData(boneIndex, weight);
			}
		}
	}

	void AssimpAnimations::LoadJSON()
	{
		// see if animations have already been loaded
		if (Animations.size() > 0)
		{
			return;
		} 

		{
			std::string filePath = "../../Assets/JSON/AnimationTypes.json";
			rapidjson::Document doc = LoadJsonDocument(filePath);

			const auto& arr = doc["animationTypes"];

			for (auto& v : arr.GetArray()) {
				UINT id = v["id"].GetInt();
				std::string name = v["name"].GetString();

				std::transform(name.begin(), name.end(), name.begin(), ::tolower);
				AnimationTypes[id] = name;
			}
		}

		{
			std::string filePath = "../../Assets/JSON/Animations.json";
			rapidjson::Document doc = LoadJsonDocument(filePath);

			const auto& arr = doc["animations"];

			for (auto& v : arr.GetArray()) 
			{
				Animation animation;
				animation.animationTypeId = v["animationTypeId"].GetInt();
				// if animationTypeId is 0 then it has no use in the game, go ahead and go to the next animation
				if (animation.animationTypeId == 0)
				{
					continue;
				}

				animation.id = v["id"].GetInt();
				animation.modelId = v["modelId"].GetInt();

				std::string name = v["animationName"].GetString();
				std::transform(name.begin(), name.end(), name.begin(), ::tolower);
				animation.animationName = name;

				animation.startFrame = v["start"].GetInt();
				animation.endFrame = v["end"].GetInt();
				float framesPerSecond = v["fps"].GetFloat();
				animation.startTime = (float)animation.startFrame / framesPerSecond;
				animation.endTime = (float)animation.endFrame / framesPerSecond;
				animation.fps = (UINT)framesPerSecond;
				animation.animationTypeName = GetAnimationTypeNameById(animation.animationTypeId);
				animation.animationType = (Animation::AnimationType)animation.animationTypeId;

				auto iterByModelId = Animations.find(animation.modelId);
				if (iterByModelId != Animations.end())
				{
					auto iterByAnimationTypeId = iterByModelId->second.find(animation.animationTypeId);
					if (iterByAnimationTypeId != iterByModelId->second.end())
					{
						iterByAnimationTypeId->second.insert(std::pair<std::string, Animation>(animation.animationName, animation));
					}
					else
					{
						std::unordered_map<std::string, Animation> animationsByString;
						animationsByString[animation.animationName] = animation;

						iterByModelId->second.insert(std::pair<UINT, std::unordered_map<std::string, Animation>>(animation.animationTypeId, animationsByString));
					}
				}
				else
				{
					std::unordered_map<std::string, Animation> animationsByString;
					animationsByString[animation.animationName] = animation;

					std::unordered_map<UINT, std::unordered_map<std::string, Animation>> m_animationsByModelId;
					m_animationsByModelId.insert(std::pair<UINT, std::unordered_map<std::string, Animation>>(animation.animationTypeId, animationsByString));
					Animations[animation.modelId] = m_animationsByModelId;
				}
			}
		}
	}

	AssimpAnimations::AssimpAnimations() : AssimpFactory()
	{

	}

	AssimpAnimations::~AssimpAnimations()
	{

	}

	void AssimpAnimations::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	{
		m_animationCompute = std::make_unique<AnimationCompute>();
		m_animationCompute->CreateDeviceDependentResources(deviceResources);
	}

	void AssimpAnimations::Initialize(const std::string& fileName, unsigned int customFlags)
	{
		AssimpFactory::Initialize(fileName, customFlags);

		bool hasBones = false;
		// Initialize the meshes in the scene one by one
		for (UINT i = 0; i < m_meshEntries.size(); i++)
		{
			if (m_meshEntries[i].hasBones)
			{
				hasBones = true;
				const aiMesh* paiMesh = m_pScene->mMeshes[i];
				m_bones.resize(m_bones.size() + m_meshEntries[i].numVerts);
				LoadBones(i, paiMesh, m_bones);
			}
		}

		if (hasBones)
		{
			LoadJSON(); // this only loads once, so it is ok to call this for every model that has bones

			CreateSkeletonBones(m_pScene->mRootNode, &m_rootBone);

			m_ticksPerSecond = (float)(m_pScene->mAnimations[0]->mTicksPerSecond != 0 ? m_pScene->mAnimations[0]->mTicksPerSecond : 25.0f);
			m_duration = (float)m_pScene->mAnimations[0]->mDuration;

			m_animationPlayer = std::make_unique<AnimationPlayer>(this);
			// todo: remove after testing
			//m_animationPlayer->PlayClip(L"sword_action_3", true);
			//m_animationPlayer->PlayClip("walk", true);
			m_animationPlayer->PlayClipByAnimationType(Animation::AnimationType::idle, true);
		}
		else
		{
			DebugTrace("Does not have bones");
		}
	}

	void AssimpAnimations::BoneTransformBlended(float blendFactor, float timeInSecondsCurrent, float timeInSecondsTarget, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		float timeInTicksCurrent = timeInSecondsCurrent * m_ticksPerSecond;
		float animationTimeCurrent = fmod(timeInTicksCurrent, m_duration);

		float timeInTicksTarget = timeInSecondsTarget * m_ticksPerSecond;
		float animationTimeTarget = fmod(timeInTicksTarget, m_duration);

		ReadSkeletonBonesBlended(blendFactor, animationTimeCurrent, animationTimeTarget, &m_rootBone, XMMatrixIdentity(), bones, noGlobalBones, global);
	}

	void AssimpAnimations::BoneTransform(float timeInSeconds, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		float timeInTicks = timeInSeconds * m_ticksPerSecond;
		float animationTime = fmod(timeInTicks, m_duration);

		ReadSkeletonBones(animationTime, &m_rootBone, XMMatrixIdentity(), bones, noGlobalBones, global);
	}

	void AssimpAnimations::Update(DX::StepTimer const& timer)
	{
		if (m_animationPlayer)
		{
			m_animationPlayer->Update(timer);
		}
	}

	void AssimpAnimations::Release()
	{
		AssimpFactory::Release(); // base release

		m_animationPlayer.reset();
		m_animationPlayer = nullptr;

		m_animationCompute.reset();
		m_animationCompute = nullptr;
	}
}