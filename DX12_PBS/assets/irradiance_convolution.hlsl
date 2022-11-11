cbuffer ViewProjectionConstantBuffer : register(b0)
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

TextureCube SkyboxMap : register(t0);
SamplerState SkyboxSampler : register(s0);

static const float PI = 3.14159265359f;

float4 PSMain(PSInput input) : SV_TARGET {
  float3 normal = normalize(input.worldPos);
  float3 up = float3(0.0, 1.0, 0.0);
  float3 right = normalize(cross(up, normal));
  up = normalize(cross(normal, right));

  float sampleDelta = 0.025f;
  int nrSamples = 0;
  float3 irradiance = float3(0.0f, 0.0f, 0.0f);
  for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta) {
    for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta) {
      // spherical to cartesian (in tangent space)
      float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
      // tangent space to world
      float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
      sampleVec.y = -sampleVec.y;
      irradiance += SkyboxMap.Sample(SkyboxSampler, sampleVec).rgb * cos(theta) * sin(theta);
      nrSamples++;
    }
  }
  irradiance = PI * irradiance / nrSamples;

  return float4(irradiance.x, irradiance.y, irradiance.z, 1.0f);
}