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

cbuffer PrefilterConstantBuffer : register(b1)
{
  float roughness;
};

TextureCube SkyboxMap : register(t0);
SamplerState SkyboxSampler : register(s0);

static const float PI = 3.14159265359f;
// ----------------------------------------------------------------------------
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits)
{
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
float2 Hammersley(uint i, uint N)
{
  return float2(float(i) / float(N), RadicalInverse_VdC(i));
}
// ----------------------------------------------------------------------------
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
  float a = roughness * roughness;

  float phi = 2.0 * PI * Xi.x;
  float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
  float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

  // from spherical coordinates to cartesian coordinates - halfway vector
  float3 H;
  H.x = cos(phi) * sinTheta;
  H.y = sin(phi) * sinTheta;
  H.z = cosTheta;

  // from tangent-space H vector to world-space sample vector
  float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
  float3 tangent = normalize(cross(up, N));
  float3 bitangent = cross(N, tangent);

  float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
  return normalize(sampleVec);
}

float4 PSMain(PSInput input) : SV_TARGET {
  float3 N = normalize(input.worldPos);

  // make the simplyfying assumption that V equals R equals the normal 
  float3 R = N;
  float3 V = R;

  const uint SAMPLE_COUNT = 1024u;
  float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
  float totalWeight = 0.0f;

  for (uint i = 0u; i < SAMPLE_COUNT; ++i)
  {
    // generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
    float2 Xi = Hammersley(i, SAMPLE_COUNT);
    float3 H = ImportanceSampleGGX(Xi, N, roughness);
    float3 L = normalize(2.0 * dot(V, H) * H - V);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL > 0.0)
    {
      prefilteredColor += SkyboxMap.Sample(SkyboxSampler, L).rgb * NdotL;
      totalWeight += NdotL;
    }
  }

  prefilteredColor = prefilteredColor / totalWeight;

  return(float4(prefilteredColor, 1.0));
}