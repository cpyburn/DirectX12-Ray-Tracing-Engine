#include "PositionColorInstancedCommon.hlsli"

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
	return input.color;
}
