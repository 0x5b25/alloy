struct FrameConstants
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

struct PushConstants
{
    uint frameConstantsIndex;
    uint textureIndex;
    uint samplerIndex;
    uint padding;
};

ConstantBuffer<PushConstants> pc : register(b0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput VSMain(float2 position : POSITION, float2 uv : TEXCOORD, float4 color : COLOR)
{
    ConstantBuffer<FrameConstants> frame =
        ResourceDescriptorHeap[pc.frameConstantsIndex];

    PSInput result;

    float4 modelPos = float4(position.x, 0.0f, position.y, 1.0f);
    result.position = mul(frame.proj, mul(frame.view, mul(frame.model, modelPos)));
    result.uv = uv;
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    Texture2D<float4> texture = ResourceDescriptorHeap[pc.textureIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[pc.samplerIndex];

    float4 texel = texture.Sample(linearSampler, input.uv);
    return texel * input.color;
}
