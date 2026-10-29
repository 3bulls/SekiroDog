#include "Common.hlsli"

// 地面和参照物用的纯色光照
struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PSIn
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 nrm : TEXCOORD1;
};

PSIn VSMain(VSIn v)
{
    PSIn o;
    float4 wp = mul(float4(v.pos, 1.0), World);
    o.worldPos = wp.xyz;
    o.pos = mul(mul(wp, View), Proj);
    o.nrm = mul(v.nrm, (float3x3)World);
    return o;
}

float4 PSMain(PSIn i) : SV_Target
{
    float3 albedo = Color.rgb;
    if (Params.x > 0.5)
    {
        // 1 米一格的棋盘格，方便观察移动
        float cell = fmod(abs(floor(i.worldPos.x) + floor(i.worldPos.z)), 2.0);
        albedo *= lerp(1.0, 0.8, cell);
    }

    float3 N = normalize(i.nrm);
    float ndl = saturate(dot(N, -LightDir));
    float3 color = albedo * (SunColor * ndl + Ambient(N));
    return float4(ApplyFog(Tonemap(color), i.worldPos), 1.0);
}
