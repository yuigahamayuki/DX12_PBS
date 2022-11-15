cbuffer SceneConstantBuffer : register(b0)
{
  float4x4 model;
  float4x4 view;
  float4x4 projection;
  float3 camPos;
};

struct PSInput
{
  float4 position : SV_POSITION;
  float3 worldPos : POSITION0;
  float3 normal : NORMAL;
  float3 camPos : POSITION1;
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

  float4 inputNormal = float4(normal, 0.0f);
  result.normal = mul(inputNormal, instanceModel);

  result.camPos = camPos;

  result.metallic = pbrProperty.r;
  result.roughness = pbrProperty.g;
  return result;
}

#define NUM_LIGHTS 4
struct LightState
{
  float3 position;
  float3 color;
};
cbuffer LightStatesConstantBuffer : register(b1)
{
  LightState lights[NUM_LIGHTS];
};

TextureCube SkyboxMap : register(t0);
SamplerState SkyboxSampler : register(s0);

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float nom = a2;
  float denom = NdotH2 * (a2 - 1.0) + 1.0;
  denom = PI * denom * denom;

  return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;

  float nom = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

float fresnelSchlick(float cosTheta, float3 F0) {
  return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float4 PSMain(PSInput input) : SV_TARGET {
  float3 N = normalize(input.normal);
  float3 V = normalize(input.camPos - input.worldPos);

  // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
  // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
  float3 F0 = float3(0.04, 0.04, 0.04);
  float3 albedo = float3(0.5, 0.0, 0.0);
  F0 = lerp(F0, albedo, input.metallic);

  float3 Lo = float3(0.0, 0.0, 0.0);
  for (int i = 0; i < NUM_LIGHTS; ++i) {
    float3 L = normalize(lights[i].position - input.worldPos);
    float3 H = normalize(V + L);
    float distance = length(lights[i].position - input.worldPos);
    float attenuation = 1.0 / (distance * distance);
    float3 radiance = lights[i].color * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, input.roughness);
    float G = GeometrySmith(N, V, L, input.roughness);
    float3 F = fresnelSchlick(saturate(dot(H, V)), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    float3 specular = numerator / denominator;

    // kS is equal to Fresnel
    float3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    float3 kD = 1.0 - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - input.metallic;

    float NdotL = max(dot(N, L), 0.0);

    Lo += (kD * albedo / PI + specular) * radiance * NdotL;
  }

  float3 ambient = 0.03 * albedo;

  float3 color = ambient + Lo;

  color = color / (color + 1.0);
  color = pow(color, 1.0 / 2.2);

  return float4(color, 1.0);
}