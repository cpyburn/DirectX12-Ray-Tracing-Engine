#include "Common.hlsli"
#include "PositionColorInstancedCommon.hlsli"

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float3 pos : POSITION;
	float4 color : COLOR;
    matrix World : WORLD;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);

	// Calculate the position of the vertex against the world, view, and projection matrices.
    pos = mul(pos, input.World);
	//pos = mul(pos, model);
    pos = mul(pos, gView);
    pos = mul(pos, gProj);
	output.pos = pos;

	// Store the texture coordinates for the pixel shader.
	output.color = input.color;

	return output;
}
