// A custom resolve kernel that averages color at all sample points.
#include <metal_stdlib>
using namespace metal;

kernel void
averageResolveKernel(texture2d_ms<float, access::read> multisampledTexture [[texture(0)]],
                     texture2d<float, access::write> resolvedTexture [[texture(1)]],
                     uint2 gid [[thread_position_in_grid]])
{
    const uint count = multisampledTexture.get_num_samples();


    float4 resolved_color = 0;


    for (uint i = 0; i < count; ++i)
    {
        resolved_color += multisampledTexture.read(gid, i);
    }


    resolved_color /= count;


    resolvedTexture.write(resolved_color, gid);
}