struct SceneParameters
{
    float4x4 mtxView;
    float4x4 mtxProj;
    float4 tessParam;
    float time;
    float3 reserved;
};

ConstantBuffer<SceneParameters> gScene : register(b0);

struct PatchConstantData
{
    float tessFactor[4] : SV_TessFactor;
    float innerTessFactor[2] : SV_InsideTessFactor;
};

struct VSInput
{
    float3 position : POSITION;
};

struct HSInput
{
    float3 position : POSITION;
};

struct DSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation float4 color : COLOR; // êFÇÃï‚ä‘ÇÇµÇ»Ç¢Ç≈ëóÇÈ.
};