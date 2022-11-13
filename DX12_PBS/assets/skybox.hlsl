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

PSInput VSMain(float3 position : POSITION) {
  PSInput result;
  float4 inputPosition = float4(position, 1.0f);
  result.worldPos = position;
  float3x3 view_3by3 = (float3x3)view;
  float4x4 view_remove_translate = { 1.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 1.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 1.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f, 1.0f };
  view_remove_translate._m00_m01_m02 = view_3by3._m00_m01_m02;
  view_remove_translate._m10_m11_m12 = view_3by3._m10_m11_m12;
  view_remove_translate._m20_m21_m22 = view_3by3._m20_m21_m22;
  result.position = mul(inputPosition, view_remove_translate);
  result.position = mul(result.position, projection);
  result.position.z = result.position.w;
  return result;
}

TextureCube SkyboxMap : register(t0);
SamplerState SkyboxSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET {
  float3 envColor = SkyboxMap.Sample(SkyboxSampler, input.worldPos).rgb;
  envColor = envColor / (envColor + 1.0);
  envColor = pow(envColor, 1.0 / 2.2);
  return float4(envColor, 1.0);
}