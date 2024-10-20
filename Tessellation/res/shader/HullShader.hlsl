#include "ShaderCommon.hlsli"

// パッチ定数データを返す関数
PatchConstantData PatchConstantFunc(InputPatch<HSInput, 4> input)
{
    PatchConstantData pcd;
    pcd.tessFactor[0] = gScene.tessParam.y;
    pcd.tessFactor[1] = gScene.tessParam.y;
    pcd.tessFactor[2] = gScene.tessParam.y;
    pcd.tessFactor[3] = gScene.tessParam.y;
    pcd.innerTessFactor[0] = gScene.tessParam.x;
    pcd.innerTessFactor[1] = gScene.tessParam.x;
    return pcd;
}

// ハルシェーダーの制御点関数
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstantFunc")]
DSInput main(InputPatch<HSInput, 4> input, uint id : SV_OutputControlPointID)
{
    DSInput output;
    output.position = input[id].position;
    return output;
}
