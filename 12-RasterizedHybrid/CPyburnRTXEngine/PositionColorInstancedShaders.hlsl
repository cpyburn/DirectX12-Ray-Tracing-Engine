//#include "Common.hlsli"

cbuffer Camera : register(b0, space0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvView;
    float4x4 gInvProj;

    float3 gCameraPos;
}

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float3 pos : POSITION;
	float4 color : COLOR;
    float4 world0 : WORLD0;
    float4 world1 : WORLD1;
    float4 world2 : WORLD2;
    float4 world3 : WORLD3;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput mainVS(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);

    float4x4 world = float4x4(input.world0, input.world1, input.world2, input.world3);
    
	// Calculate the position of the vertex against the world, view, and projection matrices.
    pos = mul(pos, world);
	//pos = mul(pos, model);
    pos = mul(pos, gView);
    pos = mul(pos, gProj);
	output.pos = pos;

	// Store the texture coordinates for the pixel shader.
	output.color = input.color;

	return output;
}

// A pass-through function for the (interpolated) color data.
float4 mainPS(PixelShaderInput input) : SV_TARGET
{
    return input.color;
}
