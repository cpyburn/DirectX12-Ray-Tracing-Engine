#pragma once

#include "Animation.h"
#include "Random.h"

namespace CPyburnRTXEngine
{
	class AssimpAnimations; // forward declaration

	class AnimationPlayer
	{
	public:
#define MAX_BONES 100

		struct AnimationClip
		{
			std::string name;
			Animation::AnimationType animationType = Animation::AnimationType::none;
			Animation* animation = nullptr; // pointer to anination
			float time = 0;
			std::vector<XMMATRIX> bones;
			// used for animation blending
			std::vector<XMMATRIX> noGlobalBones;
			// used for animation blending
			std::vector<XMMATRIX> global;
			bool repeat = false;
			bool forward = true;
		};

	private:
		const float m_maxBlendTime = 0.25f; // seconds
		float m_currentBlendTime = m_maxBlendTime;
		float m_animationSpeed = 1.0; //0.01f is almost noticeable 
		RandomNumberGenerator s_RNG;

		AssimpAnimations* m_skinnedMesh = nullptr; // pointer to model
		std::map<UINT, std::map<std::string, Animation>>* m_animationsByModelId; // pointer to the map

		AnimationClip m_currentClip;
		AnimationClip m_targetClip;

		void PlayClip(const std::string& clipName, bool repeat = true, bool forward = true);
		void BlendClip(const std::string& clipName, bool repeat = true, bool forward = true);

	public:
		AnimationPlayer(AssimpAnimations* skinnedMesh);
		~AnimationPlayer();

		void PlayClipByAnimationType(const Animation::AnimationType& animationType, bool repeat = true, bool forward = true);
		void BlendClipByAnimationType(const Animation::AnimationType& animationType, bool repeat = true, bool forward = true);

		// obsolete: but leaving the code if needed later
		//void PlayClipByAnimationTypeName(const string& animationTypeName, bool repeat = true, bool forward = true);
		//void BlendClipByAnimationTypeName(const string& animationTypeName, bool repeat = true, bool forward = true);

		void Update(DX::StepTimer const& timer);
		XMMATRIX* GetBones() { return &m_currentClip.bones[0]; }
		AnimationClip* GetCurrentClip() { return &m_currentClip; }
		AnimationClip* GetTargetClip() { return &m_targetClip; }

		Animation* GetAnimation(const std::string& clipName)
		{
			for (std::map<UINT, std::map<std::string, Animation>>::iterator it = m_animationsByModelId->begin(); it != m_animationsByModelId->end(); ++it)
			{
				std::map<std::string, Animation>* animationByModelTypeId = &it->second;
				auto animationByTypeIter = animationByModelTypeId->find(clipName);
				if (animationByTypeIter != animationByModelTypeId->end())
					return &animationByTypeIter->second;
			}

			return nullptr;
		}

		bool DoesAnimationTypeExist(const Animation::AnimationType& animationType)
		{
			UINT animationTypeId = (UINT)animationType;
			auto iter = m_animationsByModelId->find((UINT)animationType);
			if (iter != m_animationsByModelId->end())
			{
				if (iter->second.size() > 0)
				{
					return true;
				}
			}
			return false;
		}

		std::string GetAnimationByAnimationType(const Animation::AnimationType& animationType)
		{
			auto iter = m_animationsByModelId->find((UINT)animationType);
			if (iter != m_animationsByModelId->end())
			{
				std::map<std::string, Animation>* animationByString = &iter->second;

				if (animationByString->size() == 0)
				{
					throw "missing this type of animation";
				}

				int randomInt = s_RNG.NextInt(0, static_cast<int>(animationByString->size()) - 1);

				auto it = animationByString->begin();
				std::advance(it, randomInt);
				return it->first;
			}

			return "";
		}

		//string GetAnimationByAnimationTypeName(const string& animationTypeName)
		//{
		//	for (map<UINT, string>::iterator animationTypeIter = ForwardPlusSkinned::AnimationTypes.begin(); animationTypeIter != ForwardPlusSkinned::AnimationTypes.end(); ++animationTypeIter)
		//	{
		//		if (animationTypeIter->second == animationTypeName)
		//		{
		//			UINT animationTypeId = animationTypeIter->first;
		//			map<UINT, map<string, Animation>>& animationsByModelId = *m_animationsByModelId;
		//			map<string, Animation>& animationByString = animationsByModelId[animationTypeId];
		//			
		//			int randomInt = s_RNG.NextInt(0, animationByString.size() - 1);
		//			
		//			int counter = 0;
		//			for (map<string, Animation>::iterator animationByStringIter = animationByString.begin(); animationByStringIter != animationByString.end(); ++animationByStringIter)
		//			{
		//				if (animationByStringIter->second.animationTypeName == animationTypeName)
		//				{
		//					if (randomInt == counter)
		//					{
		//						return animationByStringIter->first;
		//					}
		//					counter++;
		//				}
		//			}
		//		}
		//	}

		//	return "";
		//}

	};
}
