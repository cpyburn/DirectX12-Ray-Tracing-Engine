struct VSVertices
{
    float3 position;
    float2 tex;
    float3 normal;
    float3 tangent;
    float3 binormal;
};

struct VertexBoneData
{
    unsigned int IDs[4];
    float Weights[4];
};

StructuredBuffer<VSVertices> BaseVertices : register(t0);
StructuredBuffer<VertexBoneData> BoneData : register(t1);
StructuredBuffer<float4x4> BoneMatrices : register(t2);

RWStructuredBuffer<VSVertices> OutVertices : register(u0);

[numthreads(256, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    uint v = id.x;

    VSVertices vin = BaseVertices[v];
    VertexBoneData bin = BoneData[v];

    float4 pos = float4(vin.position, 1.0);
    float3 nrm = vin.normal;

    float4 skinnedPos = 0;
    float3 skinnedNrm = 0;

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float4x4 m = BoneMatrices[bin.IDs[i]];
        skinnedPos += mul(m, pos) * bin.Weights[i];
        skinnedNrm += mul((float3x3) m, nrm) * bin.Weights[i];
    }

    vin.position = skinnedPos.xyz;
    vin.normal = normalize(skinnedNrm);

    OutVertices[v] = vin;
}