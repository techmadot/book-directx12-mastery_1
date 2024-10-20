#include "ShaderCommon.hlsli"

PSInput main(VSInput input)
{
    PSInput result = (PSInput) 0;
    float4x4 mtxVP = mul(gScene.mtxView, gScene.mtxProj);
    
    float4 worldPos = mul(input.position, gMesh.mtxWorld);
    float3 worldNormal = mul(input.normal, (float3x3) gMesh.mtxWorld);
    result.position = mul(worldPos, mtxVP);
    result.worldPosition = worldPos;
    result.worldNormal = normalize(worldNormal);
    result.uv0 = input.texcoord0;
    return result;
}