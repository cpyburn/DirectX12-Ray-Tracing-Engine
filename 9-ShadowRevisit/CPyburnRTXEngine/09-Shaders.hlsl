// 4.2 Ray - Tracing Shaders 04 - Shaders.hlsl

// 4.3.a Ray-Generation Shader
RaytracingAccelerationStructure gRtScene : register(t0, space0);
RWTexture2D<float4> gOutput : register(u0, space0);

// 15.4.a
struct STriVertex
{
    float3 vertex;
    float2 texture;
    float3 normal;
    float3 tangent;
    float3 binormal; // todo: drop for ray tracing
};
StructuredBuffer<STriVertex> BTriVertex : register(t0, space1);
StructuredBuffer<uint3> gIndices : register(t1, space1);
Texture2D<float4> gDiffuseTexture : register(t2, space1);
SamplerState gSampler : register(s0);

cbuffer Camera : register(b0, space0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvView;
    float4x4 gInvProj;

    float3 gCameraPos;
}

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

// 7.1 Payload
struct RayPayload
{
    float3 color;
};

// 7.0
[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 uv = (launchIndex + 0.5) / launchDim;
    uv = uv * 2.0 - 1.0;

    // Flip Y for DX coordinate space
    uv.y *= -1.0;

    // Reconstruct clip space position
    float4 clip = float4(uv, 1.0, 1.0);

    // View space
    float4 view = mul(gInvProj, clip);
    view /= view.w;

    // World space direction
    float4 world = mul(gInvView, float4(view.xyz, 0.0));
    float3 rayDir = normalize(world.xyz);

    RayDesc ray;
    ray.Origin = gCameraPos;
    ray.Direction = rayDir;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2 /* 13.4 MultiplierForGeometryContributionToShaderIndex */, 0, ray, payload);
    float3 col = linearToSrgb(payload.color);
    gOutput[launchIndex.xy] = float4(col, 1);
}

// 7.2 Miss Shader
[shader("miss")]
void miss(inout RayPayload payload)
{
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
    // instance index (from the TLAS)
    //uint instanceIndex = InstanceID();
    
    uint primIndex = PrimitiveIndex();
    uint3 indices = gIndices[primIndex];

    STriVertex v0 = BTriVertex[indices.x];
    STriVertex v1 = BTriVertex[indices.y];
    STriVertex v2 = BTriVertex[indices.z];

    float2 bary = attribs.barycentrics;
    float3 weights = float3(1.0 - bary.x - bary.y, bary.x, bary.y);

    float2 uv =
        v0.texture * weights.x +
        v1.texture * weights.y +
        v2.texture * weights.z;

    float4 texColor = gDiffuseTexture.SampleLevel(gSampler, uv, 0);
    
    float hitT = RayTCurrent();
    float3 rayDirW = WorldRayDirection();
    float3 rayOriginW = WorldRayOrigin();

    // 13.5.b Find the world-space hit position
    float3 posW = rayOriginW + hitT * rayDirW;
    
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
    payload.color = texColor.rgb * factor;
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
    payload.hit = true;
}

// 13.1.c
[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
    payload.hit = false;
}

