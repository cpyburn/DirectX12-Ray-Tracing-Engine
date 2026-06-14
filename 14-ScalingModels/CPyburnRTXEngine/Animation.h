#pragma once

#include "pchlib.h"

namespace CPyburnRTXEngine
{
	class Animation
	{
	public:
		enum class AnimationType
		{
			none = 0,
			idle = 1,
			walk = 2,
			walk_backwards = 3,
			run = 4,
			jump = 5,
			die = 6,
			take_hit = 7,
			idle_action = 8,
			action_kick = 9,
			action_standard_melee = 10,
			bow_draw = 11,
			idle_bow = 12,
			action_bow = 13,
			bow_withdraw = 14,
			sword_draw = 15,
			idle_sword = 16,
			action_sword = 17,
			sword_withdraw = 18,
		};

		UINT id = 0;
		UINT modelId = 0;
		std::string animationName = "";
		UINT startFrame = 0;
		UINT endFrame = 0;
		float startTime = 0;
		float endTime = 0;
		UINT fps = 0;
		UINT animationTypeId = 0;
		Animation::AnimationType animationType = Animation::AnimationType::none;
		std::string animationTypeName = "";
	};
}