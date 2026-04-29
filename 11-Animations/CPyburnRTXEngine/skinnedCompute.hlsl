StructuredBuffer<float3> BasePositions : register(t0);
StructuredBuffer<float3> BaseNormals : register(t1);
StructuredBuffer<uint4> BoneIndices : register(t2);
StructuredBuffer<float4> BoneWeights : register(t3);
StructuredBuffer<float4x4> BoneMatrices : register(t4);

RWStructuredBuffer<float3> SkinnedPositions : register(u0);
RWStructuredBuffer<float3> SkinnedNormals : register(u1);

[numthreads(256, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    uint v = id.x;

    float3 p = BasePositions[v];
    float3 n = BaseNormals[v];

    uint4 bi = BoneIndices[v];
    float4 bw = BoneWeights[v];

    float4 skinnedPos = 0;
    float3 skinnedNrm = 0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float4x4 m = BoneMatrices[bi[i]];
        skinnedPos += mul(m, float4(p, 1.0f)) * bw[i];

        // For normals, use 3x3 or inverse-transpose if needed.
        skinnedNrm += mul((float3x3) m, n) * bw[i];
    }

    SkinnedPositions[v] = skinnedPos.xyz;
    SkinnedNormals[v] = normalize(skinnedNrm);
}