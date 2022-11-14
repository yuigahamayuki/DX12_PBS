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
  float metallic : COLOR0;
  float roughness : COLOR1;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD,
  float3 translation : INSTANCEPOS, float3 pbrProperty : INSTANCEPBRPROPERTIES) {
  PSInput result;
  float4 inputPosition = float4(position, 1.0f);
  float4x4 instanceModel =
  {
    1.f,0.f,0.f,0.f,
    0.f,1.f,0.f,0.f,
    0.f,0.f,1.f,0.f,
    translation.x,translation.y,translation.z,1.f
  };
  result.position = mul(inputPosition, instanceModel);
  result.worldPos = result.position;
  result.position = mul(result.position, view);
  result.position = mul(result.position, projection);

  result.metallic = pbrProperty.r;
  result.roughness = pbrProperty.g;
  return result;
}

#define NUM_LIGHTS 4
struct LightState
{
  float3 position;
  float4 color;
};
cbuffer LightStatesConstantBuffer : register(b1)
{
  LightState lights[NUM_LIGHTS];
};

TextureCube SkyboxMap : register(t0);
SamplerState SkyboxSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET {
  return float4(input.metallic, 0.0, input.roughness, 1.0);
}