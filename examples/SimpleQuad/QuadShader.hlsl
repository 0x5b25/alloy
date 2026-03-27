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
StructuredBuffer<SceneDescriptor> sceneDesc : register(t0);

Texture2D<float4> tex1 : register(t1);
sampler samp1 : register(s0);

struct UniformBufferObject{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBufferObject> ubo  : register(b0);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 uv : COLOR0;
    float4 color : COLOR1;
};

PSInput VSMain(float4 position : POSITION, float4 uv : TEXCOORD, float4 color : COLOR)
{
    PSInput result;

    float4 modelPos = {position.x, 0, position.y, 1};

    //result.position = ubo.proj * ubo.view * ubo.model * float4(position.xy, 0.0, 1.0);
    result.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, modelPos)));
    result.color = color + sceneDesc[0].offset;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{

    return tex1.Sample(samp1, input.uv.xy);// + input.color;
    //return saturate(input.color);
    //return input.uv;
}