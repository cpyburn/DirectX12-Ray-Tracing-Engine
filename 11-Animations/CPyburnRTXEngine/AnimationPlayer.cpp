#include "pchlib.h"
#include "AnimationPlayer.h"

#include "AssimpAnimations.h"

namespace CPyburnRTXEngine
{
	AnimationPlayer::AnimationPlayer(AssimpAnimations* skinnedMesh) :
		m_skinnedMesh(skinnedMesh)
	{
		m_currentClip.bones.resize(MAX_BONES);
		m_currentClip.noGlobalBones.resize(MAX_BONES);
		m_currentClip.global.resize(MAX_BONES);

		m_targetClip.bones.resize(MAX_BONES);
		m_targetClip.noGlobalBones.resize(MAX_BONES);
		m_targetClip.global.resize(MAX_BONES);

		// todo:
		//m_animationsByModelId = &ForwardPlusSkinned::Animations[skinnedMesh->GetModelId()];
	}

	AnimationPlayer::~AnimationPlayer()
	{
		m_currentClip.bones.clear();
		m_currentClip.noGlobalBones.clear();
		m_currentClip.global.clear();

		m_targetClip.bones.clear();
		m_targetClip.noGlobalBones.clear();
		m_targetClip.global.clear();
	}

	void AnimationPlayer::PlayClip(const std::string& clipName, bool repeat, bool forward)
	{
		m_currentClip.name = clipName;
		m_currentClip.animation = GetAnimation(clipName);
		m_currentClip.time = (float)m_currentClip.animation->startTime;
		m_currentClip.repeat = repeat;
		m_currentClip.forward = forward;
	}

	void AnimationPlayer::BlendClip(const std::string& clipName, bool repeat, bool forward)
	{
		m_currentBlendTime = 0.0f;

		m_targetClip.name = clipName;
		m_targetClip.animation = GetAnimation(clipName);
		m_targetClip.time = (float)m_targetClip.animation->startTime;
		m_targetClip.repeat = repeat;
		m_targetClip.forward = forward;
	}

	void AnimationPlayer::PlayClipByAnimationType(const Animation::AnimationType& animationType, bool repeat, bool forward)
	{
		m_currentClip.animationType = animationType;

		std::string animationName = GetAnimationByAnimationType(animationType);
		PlayClip(animationName, repeat, forward);
	}

	void AnimationPlayer::BlendClipByAnimationType(const Animation::AnimationType& animationType, bool repeat, bool forward)
	{
		m_targetClip.animationType = animationType;

		std::string animationName = GetAnimationByAnimationType(animationType);
		// todo: eventually need to fix this (remember you need to use the Calculate Scale, Rotation, Translation from forward plus skinned)
		// NOTE: almost positive the model has to have baked animation to blend properly (even baked, may still need to play with the above)
		BlendClip(animationName, repeat, forward);
		//PlayClip(animationName, repeat, forward);
	}

	//void AnimationPlayer::PlayClipByAnimationTypeName(const string& animationTypeName, bool repeat, bool forward)
	//{
	//	string animationName = GetAnimationByAnimationTypeName(animationTypeName);
	//	PlayClip(animationName, repeat, forward);
	//}

	//void AnimationPlayer::BlendClipByAnimationTypeName(const string& animationTypeName, bool repeat, bool forward)
	//{
	//	string animationName = GetAnimationByAnimationTypeName(animationTypeName);
	//	BlendClip(animationName, repeat, forward);
	//}

	void AnimationPlayer::Update(DX::StepTimer const& timer)
	{

#pragma region currentClip
		if (m_currentClip.forward)
		{
			if (m_currentClip.time < m_currentClip.animation->endTime)
			{
				m_currentClip.time += (float)timer.GetElapsedSeconds() * m_animationSpeed;
			}
			else if (m_currentClip.repeat)
			{
				m_currentClip.time = (float)m_currentClip.animation->startTime;
			}
			else
			{
				m_currentClip.time = m_currentClip.animation->endTime;
				m_currentClip.animationType = Animation::AnimationType::none; // what the animation type turns to when completed
			}
		}
		// reverse
		else
		{
			if (m_currentClip.time > (float)m_currentClip.animation->startTime)
			{
				m_currentClip.time -= (float)timer.GetElapsedSeconds() * m_animationSpeed;
			}
			else if (m_currentClip.repeat)
			{
				m_currentClip.time = (float)m_currentClip.animation->endTime;
			}
			else
			{
				m_currentClip.time = m_currentClip.animation->startTime;
				m_currentClip.animationType = Animation::AnimationType::none; // what the animation type turns to when completed
			}
		}
#pragma endregion

		if (m_currentBlendTime < m_maxBlendTime)
		{
			m_currentBlendTime += (float)timer.GetElapsedSeconds();
			float blendFactor = m_currentBlendTime / m_maxBlendTime;

			if (blendFactor >= 1.0f)
			{
				m_currentClip = m_targetClip;
				m_targetClip.animationType = Animation::AnimationType::none;

				blendFactor = 1.0f;
			}

			m_skinnedMesh->BoneTransformBlended(blendFactor, m_currentClip.time, m_targetClip.time, &m_currentClip.bones[0], &m_currentClip.noGlobalBones[0], &m_currentClip.global[0]);

#pragma region targetClip
			if (m_targetClip.forward)
			{
				if (m_targetClip.time < (float)m_targetClip.animation->endTime)
				{
					m_targetClip.time += (float)timer.GetElapsedSeconds() * m_animationSpeed;
				}
				else if (m_targetClip.repeat)
				{
					m_targetClip.time = (float)m_targetClip.animation->startTime;
				}
			}
			else
			{
				if (m_targetClip.time > (float)m_targetClip.animation->startTime)
				{
					m_targetClip.time -= (float)timer.GetElapsedSeconds() * m_animationSpeed;
				}
				else if (m_targetClip.repeat)
				{
					m_targetClip.time = (float)m_targetClip.animation->endTime;
				}
			}
#pragma endregion

			//XMVECTOR currentRotQuat, targetRotQuat, finalRotQuat;
			//XMVECTOR currentTran, targetTran, finalTran;
			//// not really needed except for out 
			//XMVECTOR currentScale, targetScale, finalScale;

			//XMVECTOR currentGlobalRotQuat, targetGlobalRotQuat, finalGlobalRotQuat;
			//XMVECTOR currentGlobalTran, targetGlobalTran, finalGlobalTran;
			//// not really needed except for out
			//XMVECTOR currentGlobalScale, targetGlobalScale, finalGlobalScale;

			//for (size_t i = 0; i < m_skinnedMesh->GetNumBones(); i++)
			//{
			//	XMMatrixDecompose(&currentScale, &currentRotQuat, &currentTran, XMMatrixTranspose(m_currentClip.noGlobalBones[i]));
			//	XMMatrixDecompose(&targetScale, &targetRotQuat, &targetTran, XMMatrixTranspose(m_targetClip.noGlobalBones[i]));
			//	finalRotQuat = XMQuaternionSlerp(currentRotQuat, targetRotQuat, blendFactor);
			//	finalTran = XMVectorLerp(currentTran, targetTran, blendFactor);
			//	//finalScale = XMVectorLerp(currentScale, targetScale, blendFactor);
			//	//XMMATRIX finalMultM = XMMatrixTranspose(XMMatrixScalingFromVector(finalScale) * XMMatrixRotationQuaternion(finalRotQuat) * XMMatrixTranslationFromVector(finalTran));
			//	XMMATRIX finalMultM = XMMatrixTranspose(XMMatrixRotationQuaternion(finalRotQuat) * XMMatrixTranslationFromVector(finalTran));

			//	XMMatrixDecompose(&currentGlobalScale, &currentGlobalRotQuat, &currentGlobalTran, XMMatrixTranspose(m_currentClip.global[i]));
			//	XMMatrixDecompose(&targetGlobalScale, &targetGlobalRotQuat, &targetGlobalTran, XMMatrixTranspose(m_targetClip.global[i]));
			//	finalGlobalRotQuat = XMQuaternionSlerp(currentGlobalRotQuat, targetGlobalRotQuat, blendFactor);
			//	finalGlobalTran = XMVectorLerp(currentGlobalTran, targetGlobalTran, blendFactor);
			//	//finalGlobalScale = XMVectorLerp(currentGlobalScale, targetGlobalScale, blendFactor);
			//	//XMMATRIX finalGlobalMultM = XMMatrixTranspose(XMMatrixScalingFromVector(finalGlobalScale) * XMMatrixRotationQuaternion(finalGlobalRotQuat) * XMMatrixTranslationFromVector(finalGlobalTran));
			//	XMMATRIX finalGlobalMultM = XMMatrixTranspose(XMMatrixRotationQuaternion(finalGlobalRotQuat) * XMMatrixTranslationFromVector(finalGlobalTran));

			//	m_currentClip.bones[i] = finalGlobalMultM * finalMultM;
			//}
		}
		else
			m_skinnedMesh->BoneTransform(m_currentClip.time, &m_currentClip.bones[0], &m_currentClip.noGlobalBones[0], &m_currentClip.global[0]);
	}
}