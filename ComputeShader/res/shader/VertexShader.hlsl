#include "ShaderCommon.hlsli"

PSInput main(VSInput input)
{
    PSInput result = (PSInput) 0;
    float4x4 mtxPV = mul(gScene.mtxProj, gScene.mtxView);
    
    result.position = mul(mtxPV, input.position);
    result.uv0 = input.texcoord0;
    return result;
}