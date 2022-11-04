cbuffer EquirectangularToCubemapConstantBuffer : register(b0)
{
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
  result.position = mul(inputPosition, view);
  result.position = mul(result.position, projection);

  return result;
}

Texture2D HDRMap : register(t0);
SamplerState HDRSampler : register(s0);

float2 SampleSphericalMap(float3 direction)
{
  float2 uv = float2(atan2(direction.z, direction.x), asin(direction.y));
  const float2 invAtan = float2(0.1591, 0.3183);  // (1/2pi, 1/pi)
  uv = uv * invAtan;
  uv += 0.5f;

  return uv;
}

float4 PSMain(PSInput input) : SV_TARGET{
  float2 uv = SampleSphericalMap(normalize(input.worldPos));

  return HDRMap.Sample(HDRSampler, uv);
}