#include "ShaderCommon.hlsli"

float pseudoRandom(float2 coord)
{
    return clamp(frac(sin(dot(coord.xy, float2(12.9898, 78.233))) * 43758.5453), 0, 1);
}

[domain("quad")]
PSInput main(PatchConstantData pcd, const OutputPatch<DSInput, 4> input, float2 domainUV : SV_DomainLocation)
{
    PSInput output;

    // domainUV座標で頂点位置を補間
    float3 position = lerp(
        lerp(input[0].position, input[1].position, domainUV.x),
        lerp(input[3].position, input[2].position, domainUV.x),
        domainUV.y
    );
    
    //// 波動移動
    float dist = length(position);
    float waveHeight = sin(position.x + gScene.time) * cos(position.z + gScene.time);
    position.y += waveHeight;

    // 画面座標変換
    matrix mtxPV = mul(gScene.mtxProj, gScene.mtxView);
    float4 worldPos = float4(position, 1.0);
    
    output.position = mul(mtxPV, worldPos);

    uint index = 0;
    if (gScene.tessParam.z > 0.5)
    {
        index = uint(pseudoRandom(domainUV.xy) * 10);
    }

    float3 colorTable[10] =
    {
        float3(0, 0, 0),
        float3(1, 0, 0),
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1),
        float3(1, 1, 0),
        float3(0, 1, 1),
        float3(1, 0, 1),
        float3(1, 0.5, 0),
        float3(0.5, 0.5, 0.5),
    };
    
    output.color = float4(colorTable[index], 1);
    return output;
}