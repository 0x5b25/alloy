#define AS_GROUP_SIZE 32

/*********************************************************
 *  Shader resources
 *********************************************************/

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

/*********************************************************
 *  Shader input/output
 *********************************************************/

struct MeshOutput {
    float4 Position : SV_POSITION;
    float3 Color    : COLOR;
};

struct Payload {
    float3 offsets[AS_GROUP_SIZE];
};
groupshared Payload sPayload;


/*********************************************************
 *  Utility function
 *********************************************************/

uint RandomNumber(uint seed)
{
    uint r = seed;
    r ^= r << 13;
    r ^= r >> 17;
    r ^= r << 5;
    return r;
}

/*********************************************************
 *  Shader entries
 *********************************************************/

[numthreads(AS_GROUP_SIZE, 1, 1)]
void ASMain(
    uint gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID,
    uint gid  : SV_GroupID
)
{
    const float3 delta = { 0.01, 0.01, 0.001 };
    sPayload.offsets[gtid] = delta * dtid;
    // Assumes all meshlets are visible
    DispatchMesh(AS_GROUP_SIZE, 1, 1, sPayload);
}


[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void MSMain(
                  uint       gtid : SV_GroupThreadID, 
                  uint       gid  : SV_GroupID, 
     in  payload  Payload    payload,
     out indices  uint3      triangles[1],
     out vertices MeshOutput vertices[3]
) {
    float4 offset = {payload.offsets[gid], 0};
    
    // Must be called before writing the geometry output
    SetMeshOutputCounts(3, 1); // 3 vertices, 1 primitive

    triangles[0] = uint3(0, 1, 2);

    vertices[0].Position = float4(-0.5, 0.5, 0.0, 1.0) + offset;
    vertices[0].Color = float3(1.0, 0.0, 0.0);

    vertices[1].Position = float4(0.5, 0.5, 0.0, 1.0) + offset;
    vertices[1].Color = float3(0.0, 1.0, 0.0);

    vertices[2].Position = float4(0.0, -0.5, 0.0, 1.0) + offset;
    vertices[2].Color = float3(0.0, 0.0, 1.0);
}

float4 PSMain(MeshOutput input) : SV_TARGET
{
    return float4(input.Color, 1);
}