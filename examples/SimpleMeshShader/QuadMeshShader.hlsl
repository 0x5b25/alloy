#define AS_GROUP_SIZE 32
#define MS_GROUP_SIZE 128

static const float PI = 3.14159265f;

/*********************************************************
 *  Shader resources
 *********************************************************/

struct SceneDescriptor
{
    uint2 patchCnt;
    float2 patchSize;
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
    float4 clipSpacePosition   : SV_POSITION;
    float3 worldSpacePosition  : POSITION0;
    float3 worldSpaceNormal    : NORMAL0;
    float  rootHeight          : BLENDWEIGHT0; //Used for fake self shadow
    float  height              : BLENDWEIGHT1; //Used for fake self shadow
};

struct GrassPatchParams {
    float3 worldPos;
    float3 worldNormal;
    float3 worldTangent;
    float avgGrassHeight;
    float scatterRadius;
    uint2 patchIdx;
};

struct Payload {
    GrassPatchParams patches[AS_GROUP_SIZE];
};
groupshared Payload sPayload;


/*********************************************************
 *  Utility function
 *********************************************************/

uint CombineSeed(uint a, uint b) {
    uint h = 23; // Initial seed
    h = (h * 37) + a;
    h = (h * 37) + b;
    return h;
}

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint RandomUInt(inout uint seed)
{
    uint r = wang_hash(seed);
    seed = r;
    return r;
}

float RandomFloat(inout uint seed) {
    uint randInt = RandomUInt(seed);

    //Normalize [0, 1)
    return (float)randInt / 0xffffffff;
}

float3 Bezier(float3 p0, float3 p1, float3 p2, float t)
{
    float3 a = lerp(p0, p1, t);
    float3 b = lerp(p1, p2, t);
    return lerp(a, b, t);
}

float3 BezierDerivative(float3 p0, float3 p1, float3 p2, float t) {
    return 2. * (1. - t) * (p1 - p0) + 2. * t * (p2 - p1);
}
/*********************************************************
 *  Shader entries
 *********************************************************/

[numthreads(AS_GROUP_SIZE, 1, 1)]
void ASMain(
    uint gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID,
    uint gid  : SV_GroupID
) {

    //const uint2 patchShape = sceneDesc[0].patchCnt;
    const uint2 patchCnt = uint2(16, 16);
    const float2 patchSize = float2(1, 1);
    const uint totalPatchCnt = patchCnt.x * patchCnt.y;

    const float2 totalPatchSize = patchCnt * patchSize;
    const float2 totalPatchCenter = totalPatchSize * 0.5;


    bool patchValid = dtid < totalPatchCnt;
    if(patchValid) {
        uint entrySlot = WavePrefixCountBits(patchValid);

        const uint2 thisPatchIdx = {dtid % patchCnt.x, dtid / patchCnt.x};

        const float2 thisPatchCenter = patchSize * thisPatchIdx + 0.5 * (patchSize)
            - totalPatchCenter;

        sPayload.patches[entrySlot].worldPos = float3(thisPatchCenter.x, 0, thisPatchCenter.y);
        sPayload.patches[entrySlot].worldNormal = float3(0, 1, 0);
        sPayload.patches[entrySlot].worldTangent = float3(0, 0, 1);
        sPayload.patches[entrySlot].avgGrassHeight = 1;
        sPayload.patches[entrySlot].scatterRadius = length(patchSize * patchSize) * 0.5;
        sPayload.patches[entrySlot].patchIdx = thisPatchIdx;

    }

    // Assumes all meshlets are visible
    uint dispatchCnt = WaveActiveCountBits(patchValid);
    DispatchMesh(dispatchCnt, 1, 1, sPayload);
}

float4 MVPTransform(float4 vert) {
    return mul(ubo.proj, mul(ubo.view, mul(ubo.model, vert)));
}

/* Generating grass strand
 *
 *      Mesh shape              Bezier ctrl poin
 *
 *       v0-v1                   p1 ----- p2  (p2 - p1): bladeDir * grassLeaning
 *       | \ |                   |
 *      |   \ |                  |
 *     v2 --- v3                 |
 *     |  \    |                 |
 *     |    \  |                 |
 *    v4 ----- v5                | <--- bladeHeight
 *    | \       |                |
 *    |     \   |                |
 *   |         \ |               |
 *   v6 -------- v7              p0  <----- bladePos
 *
 *  DX12 limit max vertex count to 256 in a meshshader group
 *  Each strand has 8 verts, 6 tris, so we can have max 256 / 8 = 32 strands
 *  in a patch.
 */

#define VERT_PER_STRAND 8
#define PRIM_PER_STRAND 6
#define STRAND_PER_PATCH 32

#define VERT_PER_PATCH (STRAND_PER_PATCH * VERT_PER_STRAND)
#define PRIM_PER_PATCH (STRAND_PER_PATCH * PRIM_PER_STRAND)

[outputtopology("triangle")]
[numthreads(MS_GROUP_SIZE, 1, 1)]
void MSMain(
                  uint       gtid : SV_GroupThreadID,
                  uint       dtid : SV_DispatchThreadID,
                  uint       gid  : SV_GroupID,
     in  payload  Payload    payload,
     out indices  uint3      triangles[PRIM_PER_PATCH],
     out vertices MeshOutput vertices[VERT_PER_PATCH]
) {
    //Global seed
    uint2 patchIdx = payload.patches[gid].patchIdx;
    //uint globalSeed = gid + 1; //Seed of value 0 normally don't end up well
    uint globalSeed = CombineSeed(patchIdx.x, patchIdx.y);
    //globalSeed = CombineSeed(globalSeed, patchIdx.y);

    // Must be called before writing the geometry output
    SetMeshOutputCounts(VERT_PER_PATCH, PRIM_PER_PATCH); // 8 verts, 6 prims per strand

    {
        // each 2 verts constructs a line, so gtid acts like line id
        const uint thisBladeIdx = (gtid * 2) / VERT_PER_STRAND;

        //Scatter the grass blade
        uint seed = CombineSeed(globalSeed, thisBladeIdx);

        float3 normal = payload.patches[gid].worldNormal;
        float3 tangent = payload.patches[gid].worldTangent;
        float3 bitangent = normalize(cross(normal, tangent));

        float alpha = 2. * PI * RandomFloat(seed);//Pick direction to center in scatter circle
        float bladeRadius = payload.patches[gid].scatterRadius * RandomFloat(seed); //Pick distance in scatter circle
        float3 bladeOffset = bladeRadius * (cos(alpha) * tangent + sin(alpha) * bitangent);

        float beta = 2. * PI * RandomFloat(seed);
        float3 bladeDirection = cos(beta) * tangent + sin(beta) * bitangent;

        float3 bladePosition = payload.patches[gid].worldPos + bladeOffset;
        const float bladeHeight = payload.patches[gid].avgGrassHeight + (RandomFloat(seed) - 0.5) * 0.2;

        float3 bladeRightDirection = normalize(cross(normal, bladeDirection));

        //Calculate the control point
        //float3 p0 = bladePosition;
        //float3 p1 = p0 + float3(0, 0, bladeHeight);
        //float3 p2 = p1 + bladeDirection * bladeHeight * 0.3;
        const static float widthScale = 0.1;

        const static float w0 = 1.f * widthScale;
        const static float w1 = .7f * widthScale;
        const static float w2 = .3f * widthScale;

        //Generate the verts
        //float3 sideVec =
        //x: leaning, y: height, z: width
        float3 p2 = float3(0,                 0,           w0);
        float3 p1 = float3(0,                 bladeHeight, w1);
        float3 p0 = float3(bladeHeight * 0.3, bladeHeight, w2);

        const uint thisLineIdx = gtid % (VERT_PER_STRAND / 2);
        float t = thisLineIdx * 0.33; //0 to 0.99 percent for 4 lines

        float3 vertWeights = Bezier(p0, p1, p2, t);
        float3 vertTangent = BezierDerivative(p0, p1, p2, t);
        float2 vertNormalWeights = {-vertTangent.y, vertTangent.x};


        float3x3 modelRot = (float3x3)ubo.model;
        float3 localNormal = vertNormalWeights.x * bladeDirection
                           + vertNormalWeights.y * normal;
        float3 worldNormal = mul(modelRot, localNormal);

        // Each thread generate up to 2 verts
        uint baseVertIdx = gtid * 2;
        for(uint i = 0; i < 2; ++i) {
            const float widthFactor = i == 0 ? -1 : 1;
            float3 worldPos = bladePosition + vertWeights.x * bladeDirection
                                            + vertWeights.y * normal
                                            + widthFactor * vertWeights.z * bladeRightDirection;
            vertices[baseVertIdx + i].worldSpacePosition = worldPos;

            float4 clipPos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(worldPos, 1))));
            vertices[baseVertIdx + i].clipSpacePosition = clipPos;


            vertices[baseVertIdx + i].worldSpaceNormal = worldNormal;
            vertices[baseVertIdx + i].height = t;
            vertices[baseVertIdx + i].rootHeight = bladeHeight;
        }
    }

    {
        // Each thread generate up to 2 prims
        const uint thisBladeIdx = (gtid * 2) / PRIM_PER_STRAND;
        const uint basePrimIdx = gtid * 2;
        const uint thisPrimLineIdx = gtid % (PRIM_PER_STRAND / 2);
        const uint baseVertIdx = thisBladeIdx * VERT_PER_STRAND + thisPrimLineIdx * 2;
        for(uint i = 0; i < 2; ++i) {
            uint thisPrimIdx = basePrimIdx + i;
            if(thisPrimIdx >= PRIM_PER_PATCH) break;

            uint3 triangleIndices = i == 0? uint3(0, 1, 2) :
                                            uint3(3, 2, 1);

            triangles[basePrimIdx + i] = baseVertIdx + triangleIndices;
        }
    }

}

float4 PSMain(MeshOutput input/*, bool isFrontFace : SV_IsFrontFace*/) : SV_TARGET
{
    float4 baseColor;
    float pixelHeight = input.rootHeight * input.height;
    float selfshadow = clamp(pow((input.worldSpacePosition.y - pixelHeight)/input.height, 1.5), 0, 1);
    baseColor.rgb = pow(float3(0.41, 0.44, 0.29), 2.2) * selfshadow;
    //baseColor.rgb *= 0.75 + 0.25 * PerlinNoise2D(0.25 * input.worldSpacePosition.xy);
    baseColor.a = 1;

    //float3 normal = normalize(input.worldSpaceNormal);
    //
    //if (!isFrontFace) {
    //    normal = -normal;
    //}
    //
    //output.normal.xyz = normalize(lerp(float3(0, 0, 1), normal, 0.25));

    return baseColor;
}
