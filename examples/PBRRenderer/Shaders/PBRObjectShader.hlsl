//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

static const float PI = 3.14159265f;

struct UniformBufferObject{
    float4 skyBoxSkyColor;
    float4 skyBoxLightDir;
    float4 skyBoxLightColor;
    
    float4 cameraPos;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBufferObject> ubo  : register(b0);

//Promote float3 to vec4 if use in constant buffers
// due to HLSL requires 16 byte alignment
struct Vertex {
    float3 position;
    float2 texCoord;
    float3 normal;
    float3 tangent;
    float3 bitangent;
};
StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);

struct PerObjData {
    float4x4 transform;
    //Material
    float4 color;
    float roughness, metallic;
};

StructuredBuffer<uint> vertObjIdx : register(t2);
StructuredBuffer<PerObjData> perObjData : register(t3);


struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv     : COLOR0;

    uint objIdx : SEM0;
};

PSInput VSMain(uint indiceIdx : SV_VertexID)
{
    PSInput result;

    uint vertIdx = indices[indiceIdx];
    uint objIdx = vertObjIdx[indiceIdx];

    float4x4 modelT = perObjData[objIdx].transform;

    float3x3 modelRot = (float3x3)modelT;

    result.worldPos = mul(modelT, float4(vertices[vertIdx].position, 1.0)).xyz;
    result.position = mul(ubo.proj, mul(ubo.view, float4(result.worldPos, 1.0)));
    result.uv = vertices[vertIdx].texCoord;
    result.normal = mul(modelRot, vertices[vertIdx].normal);

    //result.position.w = 1;
    result.objIdx = objIdx;

    return result;
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 matColor = perObjData[input.objIdx].color.xyz;
    float matRoughness = perObjData[input.objIdx].roughness;
    float matMetallic = perObjData[input.objIdx].metallic;

    float3 N = normalize(input.normal); 
    float3 V = normalize(ubo.cameraPos.xyz - input.worldPos);

    //For each light, do:
    //float L = normalize(lightPositions[i] - input.worldPos);
    //float distance    = length(lightPositions[i] - WorldPos);
    //float attenuation = 1.0 / (distance * distance);
    //float3 radiance     = lightColors[i] * attenuation; 

    //We only have skylight for the moment, use that
    float3 L = normalize(-ubo.skyBoxLightDir.xyz);
    float3 radiance = ubo.skyBoxLightColor.xyz * ubo.skyBoxLightColor.w;
    float3 H = normalize(V + L);

    //F(r) = kD * F(Lambert) + kS * F(Cook-Torrance)
    
    //Calculate specular and diffuse weights {

    float3 F0 = 0.04; 
    F0      = lerp(F0, matColor, matMetallic);

    float cosTheta = max(dot(H, V), 0.0);

    //fresnelSchlick(float cosTheta, float3 F0)
    float3 kS = F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);

    // reflection/specular fraction
    float3 kD = 1.0 - kS;                        // refraction/diffuse  fraction

    //Calculate specular component {
    //Calculate Cook-Torrance
    //                           D F G
    // F(Cook-Torrance) = ------------------
    //                     4(wo * n)(wi * n)

    float NDF = DistributionGGX(N, H, matRoughness);
    float G   = GeometrySmith(N, V, L, matRoughness);

    float3 specular = (NDF * G) / 
        (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001);

    //Calculate diffuse component {
    //Use lambert model
    float3 diffuse = max(dot(N, L), 0.f);
    
    //Skylight
    float3 skyColor = ubo.skyBoxSkyColor.xyz * ubo.skyBoxSkyColor.w;

    //Metal surfaces don't have refractions, limit the diffuse part
    //using metallic value
    float3 Fr =  kS * specular + kD * diffuse * (1.f - matMetallic) + skyColor * matColor;

    return float4(Fr, 1);
}
