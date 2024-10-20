struct VSInput
{
  float3 Position : POSITION;
  float3 Color : COLOR;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float3 Color : COLOR;
};

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  result.Position = float4(In.Position, 1);
  result.Color = In.Color;
  return result;
}