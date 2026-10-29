#include "Common.hlsli"

#define MAX_BONES 128

cbuffer BonesCB : register(b2)
{
    float4x4 Bones[MAX_BONES];
};

Texture2D BaseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D MetalSmoothMap : register(t2);   // R = Metallic, A = Smoothness
SamplerState LinearWrap : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float4 tan : TANGENT;      // w = 副切线方向
    float2 uv : TEXCOORD0;
    uint4 bones : BLENDINDICES;
    float4 weights : BLENDWEIGHT;
};

struct PSIn
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 nrm : TEXCOORD1;
    float4 tan : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

PSIn VSMain(VSIn v)
{
    float4x4 skin = Bones[v.bones.x] * v.weights.x
                  + Bones[v.bones.y] * v.weights.y
                  + Bones[v.bones.z] * v.weights.z
                  + Bones[v.bones.w] * v.weights.w;
    float4x4 m = mul(skin, World);

    PSIn o;
    float4 wp = mul(float4(v.pos, 1.0), m);
    o.worldPos = wp.xyz;
    o.pos = mul(mul(wp, View), Proj);
    o.nrm = mul(v.nrm, (float3x3)m);
    o.tan = float4(mul(v.tan.xyz, (float3x3)m), v.tan.w);
    o.uv = v.uv;
    return o;
}

float4 PSMain(PSIn i) : SV_Target
{
    float3 albedo = BaseMap.Sample(LinearWrap, i.uv).rgb;
    float2 ms = MetalSmoothMap.Sample(LinearWrap, i.uv).ra;
    float metal = ms.x;
    float smooth = ms.y;

    float3 V = normalize(CameraPos - i.worldPos);
    float3 N = normalize(i.nrm);
    if (dot(N, V) < 0.0) N = -N;   // 双面渲染（衣摆等薄片）
    float3 T = normalize(i.tan.xyz - N * dot(i.tan.xyz, N));
    float3 B = cross(N, T) * i.tan.w;
    float3 nt = NormalMap.Sample(LinearWrap, i.uv).xyz * 2.0 - 1.0;
    N = normalize(nt.x * T + nt.y * B + nt.z * N);

    float3 L = -LightDir;
    float3 H = normalize(L + V);
    float ndl = saturate(dot(N, L));
    float specPower = exp2(10.0 * smooth + 1.0);
    float3 f0 = lerp(0.04, albedo, metal);
    float3 spec = f0 * pow(saturate(dot(N, H)), specPower) * (specPower + 8.0) / 8.0;
    float3 diffuse = albedo * (1.0 - metal);

    float3 color = (diffuse + spec) * SunColor * ndl + Ambient(N) * albedo;
    return float4(ApplyFog(Tonemap(color), i.worldPos), 1.0);
}
