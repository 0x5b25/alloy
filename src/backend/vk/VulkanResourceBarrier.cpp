#include "VulkanResourceBarrier.hpp"

#include <volk.h>

namespace Veldrid
{

    
static bool vk_barrier_parameters_from_d3d12_resource_state(unsigned int state, unsigned int stencil_state,
        const struct d3d12_resource *resource, VkQueueFlags vk_queue_flags, const struct vkd3d_vulkan_info *vk_info,
        VkAccessFlags *access_mask, VkPipelineStageFlags *stage_flags, VkImageLayout *image_layout)
{
    bool is_swapchain_image = resource && (resource->flags & VKD3D_RESOURCE_PRESENT_STATE_TRANSITION);
    VkPipelineStageFlags queue_shader_stages = 0;

    if (vk_queue_flags & VK_QUEUE_GRAPHICS_BIT)
    {
        queue_shader_stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (vk_queue_flags & VK_QUEUE_COMPUTE_BIT)
        queue_shader_stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    switch (state)
    {
        case D3D12_RESOURCE_STATE_COMMON: /* D3D12_RESOURCE_STATE_PRESENT */
            /* The COMMON state is used for ownership transfer between
             * DIRECT/COMPUTE and COPY queues. Additionally, a texture has to
             * be in the COMMON state to be accessed by CPU. Moreover,
             * resources can be implicitly promoted to other states out of the
             * COMMON state, and the resource state can decay to the COMMON
             * state when GPU finishes execution of a command list. */
            if (is_swapchain_image)
            {
                if (resource->present_state == D3D12_RESOURCE_STATE_PRESENT)
                {
                    *access_mask = VK_ACCESS_MEMORY_READ_BIT;
                    *stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    if (image_layout)
                        *image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    return true;
                }
                else if (resource->present_state != D3D12_RESOURCE_STATE_COMMON)
                {
                    vk_barrier_parameters_from_d3d12_resource_state(resource->present_state, 0,
                            resource, vk_queue_flags, vk_info, access_mask, stage_flags, image_layout);
                    return true;
                }
            }

            *access_mask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
            *stage_flags = VK_PIPELINE_STAGE_HOST_BIT;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_GENERAL;
            return true;

        /* Handle write states. */
        case D3D12_RESOURCE_STATE_RENDER_TARGET:
            *access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            *stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            return true;

        case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
            *access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            *stage_flags = queue_shader_stages;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_GENERAL;
            return true;

        case D3D12_RESOURCE_STATE_DEPTH_WRITE:
            *access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            *stage_flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            if (image_layout)
            {
                if (!stencil_state || (stencil_state & D3D12_RESOURCE_STATE_DEPTH_WRITE))
                    *image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                else
                    *image_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
            }
            return true;

        case D3D12_RESOURCE_STATE_COPY_DEST:
        case D3D12_RESOURCE_STATE_RESOLVE_DEST:
            *access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            *stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            return true;

        case D3D12_RESOURCE_STATE_STREAM_OUT:
            *access_mask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
                    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT
                    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
            *stage_flags = VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT
                    | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            return true;

        /* Set the Vulkan image layout for read-only states. */
        case D3D12_RESOURCE_STATE_DEPTH_READ:
        case D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
        case D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
        case D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
            *access_mask = 0;
            *stage_flags = 0;
            if (image_layout)
            {
                if (stencil_state & D3D12_RESOURCE_STATE_DEPTH_WRITE)
                {
                    *image_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
                    *access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                else
                {
                    *image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                }
            }
            break;

        case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
        case D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
        case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
            *access_mask = 0;
            *stage_flags = 0;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;

        case D3D12_RESOURCE_STATE_COPY_SOURCE:
        case D3D12_RESOURCE_STATE_RESOLVE_SOURCE:
            *access_mask = 0;
            *stage_flags = 0;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;

        default:
            *access_mask = 0;
            *stage_flags = 0;
            if (image_layout)
                *image_layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
    }

    /* Handle read-only states. */
    assert(!is_write_resource_state(state));

    if (state & D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
    {
        *access_mask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                | VK_ACCESS_UNIFORM_READ_BIT;
        *stage_flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                | queue_shader_stages;
        state &= ~D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }

    if (state & D3D12_RESOURCE_STATE_INDEX_BUFFER)
    {
        *access_mask |= VK_ACCESS_INDEX_READ_BIT;
        *stage_flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        state &= ~D3D12_RESOURCE_STATE_INDEX_BUFFER;
    }

    if (state & D3D12_RESOURCE_STATE_DEPTH_READ)
    {
        *access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        *stage_flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        state &= ~D3D12_RESOURCE_STATE_DEPTH_READ;
    }

    if (state & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
    {
        *access_mask |= VK_ACCESS_SHADER_READ_BIT;
        *stage_flags |= (queue_shader_stages & ~VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        state &= ~D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
    if (state & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
        *access_mask |= VK_ACCESS_SHADER_READ_BIT;
        *stage_flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        state &= ~D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    if (state & D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) /* D3D12_RESOURCE_STATE_PREDICATION */
    {
        *access_mask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        *stage_flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        if (vk_info->EXT_conditional_rendering)
        {
            *access_mask |= VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT;
            *stage_flags |= VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT;
        }
        state &= ~D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }

    if (state & (D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_RESOLVE_SOURCE))
    {
        *access_mask |= VK_ACCESS_TRANSFER_READ_BIT;
        *stage_flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        state &= ~(D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    }

    if (state)
    {
        WARN("Invalid resource state %#x.\n", state);
        return false;
    }
    return true;
}


} // namespace Veldrid

