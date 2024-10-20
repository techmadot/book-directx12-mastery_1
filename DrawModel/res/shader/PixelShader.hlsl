#include "ShaderCommon.hlsli"

Texture2D gTex : register(t0);
SamplerState gSampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    // 光源に向かうベクトル.
    float3 worldNormal = normalize(input.worldNormal.xyz);
    float3 toLightDir = -normalize(gScene.lightDir.xyz);
    float dotNL = saturate(dot(worldNormal, toLightDir));
      
    float4 diffuse = gTex.Sample(gSampler, input.uv0);
    
    if (gMesh.mode == 1)
    {
        if(diffuse.a < 0.5)
        {
            discard;
        }
    }
    float4 color = diffuse;
    color.xyz *= dotNL;
    
    // 環境項(アンビエント項).
    color.xyz += diffuse.xyz * gMesh.ambient.xyz;
    
    // スペキュラー項.
    float3 toEyeDir = normalize(gScene.eyePosition.xyz - input.worldPosition.xyz);
    float3 R = normalize(reflect(-toEyeDir, worldNormal));
    float shininess = gMesh.specular.w;
    float spec = pow(saturate(dot(toLightDir, R)), shininess);
    color.xyz += spec * gMesh.specular.xyz;
    return color;
}