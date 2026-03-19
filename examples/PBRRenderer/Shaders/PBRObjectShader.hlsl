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


struct SceneDescriptor
{
    float4 offset;
    float4 padding[15];
};
SceneDescriptor sceneDesc : register(t0);

struct UniformBufferObject{
    float4 cameraPos;
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBufferObject> ubo  : register(b0);

struct Vertex {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normal   : NORMAL ;
    float3 tangent  : TANGENT;
    float3 bitangent: BINORMAL;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv     : COLOR0;
    float4 color  : COLOR1;
};

PSInput VSMain(Vertex input)
{
    PSInput result;

    result.worldPos = mul(ubo.model, float4(input.position, 1.0)).xyz;
    result.position = mul(ubo.proj, mul(ubo.view, float4(result.worldPos, 1.0)));
    result.uv = input.texCoord;
    result.color = float4(0.5, 0.5, 0.5, 1.0);
    result.normal = input.normal;

    //result.position.w = 1;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 cameraPos = ubo.cameraPos.xyz;
    float3 cameraDir = normalize(cameraPos - input.worldPos);
    float intensity = dot(cameraDir, input.normal);
    float3 plainColor = input.color.xyz * intensity;
    return float4(plainColor, 1);
    //return input.uv;
}
