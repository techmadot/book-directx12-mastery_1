struct SceneParameters
{
    float4x4 mtxView;
    float4x4 mtxProj;
    float4 modeParams;
};

ConstantBuffer<SceneParameters> gScene : register(b0);

struct VSInput
{
    float4 position : POSITION;
    float2 texcoord0 : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv0 : TEXCOORD0;
};