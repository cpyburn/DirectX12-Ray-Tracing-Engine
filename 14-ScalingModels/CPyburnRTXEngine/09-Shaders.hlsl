#include "common.hlsli"

RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);
Texture2D<float> gDepthBuffer : register(t1); // linear -z depth
Texture2D<float4> gRasterColor : register(t2);

// 15.4.a
struct STriVertex
{
    float3 vertex;
    float2 texture;
    float3 normal;
    float3 tangent;
};
StructuredBuffer<STriVertex> BTriVertex : register(t0, space1);
StructuredBuffer<uint3> gIndices : register(t1, space1);
struct RtxModelData
{
    uint verticesSrvIndex;
    uint indicesSrvIndex;
    uint baseColorTexIndex;
    uint normalTexIndex;
    uint ormTexIndex;
};
StructuredBuffer<RtxModelData> gModels : register(t2, space1);
Texture2D<float4> gTextures[] : register(t3, space1);
SamplerState gSampler : register(s0);

struct EnvironmentData
{
    float3 lightDirection;
};

// 10.1.a
cbuffer Environment : register(b1, space0)
{
    EnvironmentData gEnvironmentData;
}

float3 linearToSrgb(float3 c)
{
    // Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    float3 srgb = 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * c;
    return srgb;
}

// 12.0 hybrid
float GetHardwareDepthFromWorldPos(float3 posW)
{
    float4 clip = mul(gProj, mul(gView, float4(posW, 1.0f)));
    return clip.z / clip.w; // same depth space as the raster DSV/SRV
}

// 12.0 hybrid
struct RayPayload
{
    float3 color;
    float hitViewDepth;
    uint hit; // DXR sees a bool as 4, but c++ sees bool as 1, this is a work around to force explicit behavior
};

[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    uint2 pixel = launchIndex.xy;

    float2 uv = (launchIndex.xy + 0.5) / launchDim.xy;
    uv = uv * 2.0 - 1.0;
    uv.y *= -1.0;

    float4 clip = float4(uv, 1.0, 1.0);
    float4 view = mul(gInvProj, clip);
    view /= view.w;

    float4 world = mul(gInvView, float4(view.xyz, 0.0));
    float3 rayDir = normalize(world.xyz);

    RayDesc ray;
    ray.Origin = gCameraPos;
    ray.Direction = rayDir;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.color = float3(0, 0, 0);
    payload.hitViewDepth = 1e30f;
    payload.hit = 0;

    TraceRay(gRtScene, 0, 0xFF, 0, 2, 0, ray, payload);

    float4 rayColor = float4(linearToSrgb(payload.color), 1.0f);
    float rasterDepth = gDepthBuffer.Load(int3(pixel, 0));
    float4 rasterColor = gRasterColor.Load(int3(pixel, 0));
    float4 finalColor = rayColor;

    // reversed-Z: larger depth means closer
    if (rasterDepth > 0.0f && rasterDepth > payload.hitViewDepth)
    {
        finalColor = rasterColor;
    }

    // to test grayscale depth
    //float d = gDepthBuffer.Load(int3(pixel, 0));
    //gOutput[pixel] = float4(d, d, d, 1);
    
    gOutput[pixel] = finalColor;
}

// 7.2 Miss Shader
[shader("miss")]
void miss(inout RayPayload payload)
{
    // 12.0 hybrid
    payload.hit = 0;
    payload.hitViewDepth = 0.0f;
    payload.color = float3(0.4, 0.6, 0.2);
}

// 13.1.a
struct ShadowPayload
{
    bool hit;
};

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint primIndex = PrimitiveIndex();
    uint3 indices = gIndices[primIndex];
    
    RtxModelData material = gModels[InstanceID()];
    StructuredBuffer<STriVertex> vb = ResourceDescriptorHeap[NonUniformResourceIndex(material.verticesSrvIndex)];
    StructuredBuffer<uint> ib = ResourceDescriptorHeap[NonUniformResourceIndex(material.indicesSrvIndex)];

    uint triBase = PrimitiveIndex() * 3;
    uint i0 = ib[triBase + 0];
    uint i1 = ib[triBase + 1];
    uint i2 = ib[triBase + 2];
    
    // Vertex offset lets you pack many meshes into one vertex pool.
    STriVertex v0 = vb[i0];
    STriVertex v1 = vb[i1];
    STriVertex v2 = vb[i2];

    float2 bary = attribs.barycentrics;
    float3 weights = float3(1.0 - bary.x - bary.y, bary.x, bary.y);

    float2 uv =
        v0.texture * weights.x +
        v1.texture * weights.y +
        v2.texture * weights.z;

    Texture2D<float4> baseColorTex = gTextures[NonUniformResourceIndex(material.baseColorTexIndex)];
    Texture2D<float4> normalTex = gTextures[NonUniformResourceIndex(material.normalTexIndex)];
    Texture2D<float4> ormTex = gTextures[NonUniformResourceIndex(material.ormTexIndex)];

    float3 baseColor = baseColorTex.SampleLevel(gSampler, uv, 0).rgb;
    float3 orm = ormTex.SampleLevel(gSampler, uv, 0).rgb;

    float ao = orm.r;
    float roughness = orm.g;
    float metallic = orm.b;

    float3 normalOS =
        v0.normal * weights.x +
        v1.normal * weights.y +
        v2.normal * weights.z;
    normalOS = normalize(normalOS);

    float3 tangentOS =
        v0.tangent * weights.x +
        v1.tangent * weights.y +
        v2.tangent * weights.z;
    tangentOS = normalize(tangentOS);

    float3 bitangentOS = normalize(cross(normalOS, tangentOS));

    float3 normalTS = normalTex.SampleLevel(gSampler, uv, 0).xyz * 2.0f - 1.0f;

    float3x3 TBN = float3x3(
        normalize(mul((float3x3) ObjectToWorld3x4(), tangentOS)),
        normalize(mul((float3x3) ObjectToWorld3x4(), bitangentOS)),
        normalize(mul((float3x3) ObjectToWorld3x4(), normalOS))
    );

    float3 normalWS = normalize(mul(normalTS, TBN));

    float hitT = RayTCurrent();
    float3 rayOriginW = WorldRayOrigin();
    float3 rayDirW = WorldRayDirection();
    float3 posW = rayOriginW + hitT * rayDirW;
    
    // 12.0 hybrid
    payload.hit = 1;
    payload.hitViewDepth = GetHardwareDepthFromWorldPos(posW);

    float3 lightDir = normalize(gEnvironmentData.lightDirection);
    float ndotl = saturate(dot(normalWS, lightDir));

    RayDesc ray;
    ray.Origin = posW + normalWS * 0.01;
    ray.Direction = lightDir;
    ray.TMin = 0.01;
    ray.TMax = 100000.0;

    ShadowPayload shadowPayload;
    TraceRay(gRtScene, 0, 0xFF, 1, 0, 1, ray, shadowPayload);

    float shadow = shadowPayload.hit ? 0.1 : 1.0;

    float3 ambient = 0.15 * baseColor * ao;
    float3 diffuse = ndotl * baseColor;

    payload.color = (ambient + diffuse) * shadow;
}

// 12.1.a
[shader("closesthit")]
void planeChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    // 13.5.a
    float hitT = RayTCurrent();
    float3 rayDirW = WorldRayDirection();
    float3 rayOriginW = WorldRayOrigin();

    // 13.5.b Find the world-space hit position
    float3 posW = rayOriginW + hitT * rayDirW;
    
    // 12.0 hybrid
    payload.hit = 1;
    payload.hitViewDepth = GetHardwareDepthFromWorldPos(posW);

    // Fire a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer
    RayDesc ray;
    ray.Origin = posW;
    // 13.5.c
    ray.Direction = normalize(gEnvironmentData.lightDirection);
    // 13.5.d
    ray.TMin = 0.01;
    ray.TMax = 100000;
    // 13.5.e
    ShadowPayload shadowPayload;
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, ray, shadowPayload);
    // 13.5.f
    float factor = shadowPayload.hit ? 0.1 : 1.0;
    payload.color = float3(0.9f, 0.9f, 0.9f) * factor;
}

// 13.1.b
[shader("closesthit")]
void shadowChs(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.hit = 1;
}

// 13.1.c
[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
    payload.hit = 0;
}

