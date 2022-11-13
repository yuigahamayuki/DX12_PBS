cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
  float4x4 model;
  float4x4 view;
  float4x4 projection;
};

struct PSInput
{
  float4 position : SV_POSITION;
  float3 worldPos : POSITION;
};

PSInput VSMain(float3 position : POSITION, float3 normal: NORMAL, float2 uv : TEXCOORD) {
  PSInput result;
  float4 inputPosition = float4(position, 1.0f);
  result.worldPos = position;
  result.position = mul(inputPosition, model);
  result.position = mul(result.position, view);
  result.position = mul(result.position, projection);

  return result;
}

TextureCube SkyboxMap : register(t0);
SamplerState SkyboxSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET{
  return float4(1.0, 0.0, 0.0, 1.0);
}