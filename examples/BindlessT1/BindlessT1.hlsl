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

struct UniformBufferObject
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    uint textureIndex;
    float3 padding;
};

ConstantBuffer<UniformBufferObject> ubo : register(b0);
Texture2D<float4> textures[4] : register(t1);
SamplerState linearSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput VSMain(float2 position : POSITION, float2 uv : TEXCOORD, float4 color : COLOR)
{
    PSInput result;

    float4 modelPos = float4(position.x, 0.0f, position.y, 1.0f);
    result.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, modelPos)));
    result.uv = uv;
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    uint textureIndex = ubo.textureIndex;
    float4 texel = textures[textureIndex].Sample(linearSampler, input.uv);
    return texel * input.color;
}
