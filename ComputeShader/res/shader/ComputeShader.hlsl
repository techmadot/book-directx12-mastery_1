#include "ShaderCommon.hlsli"

Texture2D<float4> gSourceTex : register(t0);
RWTexture2D<float4> gDestinationTex : register(u0);

float3 rgb2hsv(float3 rgbColor)
{
    float4 k = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(rgbColor.bg, k.wz), float4(rgbColor.gb, k.xy), step(rgbColor.b, rgbColor.g));
    float4 q = lerp(float4(p.xyw, rgbColor.r), float4(rgbColor.r, p.yzx), step(p.x, rgbColor.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 hsv2rgb(float3 color)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(color.xxx + K.xyz) * 6.0 - K.www);
    return color.z * lerp(K.xxx, saturate(p - K.xxx), color.y);
}


[numthreads(16,16,1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width = 0, height = 0;
    gSourceTex.GetDimensions(width, height);
    if (!(dtid.x < width && dtid.y < height))
    {
        return;
    }
    float4 color = gSourceTex[dtid.xy];
    if(gScene.modeParams.x < 0.5)
    {
        // セピア化.
        float3x3 toSepia = float3x3(
          0.393, 0.349, 0.272,
          0.769, 0.686, 0.534,
          0.189, 0.168, 0.131);
        color.xyz = mul(color.xyz, toSepia);
    }
    else
    {
        // 色相シフト.
        float3 hsv = rgb2hsv(color.xyz);
        hsv.x = frac(hsv.x + gScene.modeParams.y);
        color.xyz = hsv2rgb(hsv);

    }
    gDestinationTex[dtid.xy] = color;
}

// 参考文献:
// HSV変換コードは https://gist.github.com/983/e170a24ae8eba2cd174f より.
// https://stackoverflow.com/questions/15095909/from-rgb-to-hsv-in-opengl-glsl にも同様コードが掲載されている.
// これらを元にHLSL化して使用しています.
