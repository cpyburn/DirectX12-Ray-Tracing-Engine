// 4.2 Ray - Tracing Shaders 04 - Shaders.hlsl

// 4.3.a Ray-Generation Shader
RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 15.4.a
struct STriVertex
{
    float3 vertex;
    float4 color;
};
StructuredBuffer<STriVertex> BTriVertex : register(t1);

cbuffer Camera : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvView;
    float4x4 gInvProj;

    float3 gCameraPos;
}

// 10.1.a
cbuffer PerFrame : register(b1)
{
    float3 A;
    float3 B;
    float3 C;
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

    //float2 crd = float2(launchIndex.xy);
    //float2 dims = float2(launchDim.xy);

    //float2 d = ((crd / dims) * 2.f - 1.f);
    //float aspectRatio = dims.x / dims.y;
    
    //RayDesc ray;
    //ray.Origin = camPos;
    //ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1));

    //ray.TMin = 0;
    //ray.TMax = 100000;

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

// 10.1.b
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    //payload.color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;

    // 15.4.b
    uint instance = InstanceID();
    if (instance < 2)
    {
        float3 hitColor = BTriVertex[instance].color * barycentrics.x + BTriVertex[instance].color * barycentrics.y + BTriVertex[instance].color * barycentrics.z;
        payload.color = hitColor;
        return;
    }
    //float3 hitColor = BTriVertex[instance].color * barycentrics.x + BTriVertex[instance].color * barycentrics.y + BTriVertex[instance].color * barycentrics.z;
    //payload.color = hitColor;
    payload.color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
}

// 13.1.a
struct ShadowPayload
{
    bool hit;
};

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
    ray.Direction = normalize(float3(0.5, 0.5, -0.5));
    // 13.5.d
    ray.TMin = 0.01;
    ray.TMax = 100000;
    // 13.5.e
    ShadowPayload shadowPayload;
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, ray, shadowPayload);
    // 13.5.f
    float factor = shadowPayload.hit ? 0.1 : 1.0;
    payload.color = float4(0.9f, 0.9f, 0.9f, 1.0f) * factor;
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

