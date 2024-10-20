#include "ShaderCommon.hlsli"

Texture2D gTex : register(t0);
SamplerState gSampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 color = gTex.Sample(gSampler, input.uv0);
    return color;
}