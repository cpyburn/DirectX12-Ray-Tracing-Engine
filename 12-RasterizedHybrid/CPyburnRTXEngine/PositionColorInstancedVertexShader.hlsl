// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	float4x4 model;
	float4x4 view;
	float4x4 projection;
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float3 pos : POSITION;
	float4 color : COLOR;
    matrix World : WORLD;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos: SV_POSITION;
	float4 color : COLOR;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);

	// Calculate the position of the vertex against the world, view, and projection matrices.
    pos = mul(pos, input.World);
	pos = mul(pos, model);
	pos = mul(pos, view);
	pos = mul(pos, projection);
	output.pos = pos;

	// Store the texture coordinates for the pixel shader.
	output.color = input.color;

	return output;
}
