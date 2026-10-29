// 与 Graphics.h 中的 FrameConstants / ObjectConstants 对应
cbuffer FrameCB : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float3 LightDir;     // 光线传播方向（指向地面）
    float FramePad0;
    float3 CameraPos;
    float FramePad1;
};

cbuffer ObjectCB : register(b1)
{
    float4x4 World;
    float4 Color;
    float4 Params;       // x: 1 = 棋盘格
};

static const float3 SunColor = float3(1.0, 0.95, 0.85) * 2.2;
static const float3 FogColor = float3(0.55, 0.62, 0.72);

float3 Ambient(float3 n)
{
    return lerp(float3(0.10, 0.09, 0.08), float3(0.32, 0.36, 0.44), saturate(n.y * 0.5 + 0.5));
}

// ACES 近似色调映射
float3 Tonemap(float3 x)
{
    return saturate(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14));
}

float3 ApplyFog(float3 c, float3 worldPos)
{
    float d = distance(CameraPos, worldPos);
    return lerp(c, FogColor, saturate((d - 25.0) / 100.0));
}
