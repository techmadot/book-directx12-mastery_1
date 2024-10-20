#include "ShaderCommon.hlsli"

HSInput main(VSInput input)
{
  HSInput result = (HSInput) 0;
  result.position = input.position;
  return result;
}