#pragma once

#include "pchlib.h"

namespace CPyburnRTXEngine
{
	class AnimationStructs
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
	};
}