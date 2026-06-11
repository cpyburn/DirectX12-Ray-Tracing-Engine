#include "common.hlsli"

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float3 pos : POSITION;
	float4 color : COLOR;
    float4x4 world0 : WORLD0;
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
    pos = mul(input.world0, pos); // if you want to transpose and keep everything consistent you can, or you can swap the mul vector matrix to force the shader to do it correctly
    pos = mul(gView, pos);
    pos = mul(gProj, pos);

    output.pos = pos;
    output.color = input.color;
    return output;
}

// A pass-through function for the (interpolated) color data.
float4 mainPS(PixelShaderInput input) : SV_TARGET
{
    return input.color;
}
