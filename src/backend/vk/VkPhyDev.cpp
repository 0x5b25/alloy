#include "VkPhyDev.hpp"

#include <cassert>
#include <vector>

#include <volk.h>
#include "VkCommon.hpp"

namespace alloy::vk
{

    
struct vkd3d_instance;

struct vkd3d_instance_create_info
{
    /* If set to NULL, libvkd3d loads libvulkan. */
    PFN_vkGetInstanceProcAddr pfn_vkGetInstanceProcAddr;

    const char * const *instance_extensions;
    uint32_t instance_extension_count;

    const char * const *optional_instance_extensions;
    uint32_t optional_instance_extension_count;
};

struct vkd3d_device_create_info
{
    //D3D_FEATURE_LEVEL minimum_feature_level;

    struct vkd3d_instance *instance;
    const struct vkd3d_instance_create_info *instance_create_info;

    VkPhysicalDevice vk_physical_device;

    const char * const *device_extensions;
    uint32_t device_extension_count;

    const char * const *optional_device_extensions;
    uint32_t optional_device_extension_count;

    //IUnknown *parent;
    //LUID adapter_luid;
};

enum HRESULT {
    S_OK,

    E_FAIL,
    E_INVALIDARG,
    E_OUTOFMEMORY
};

struct vkd3d_optional_extension_info
{
    const char *extension_name;
    ptrdiff_t vulkan_info_offset;
    uint64_t enable_config_flags;
    uint64_t disable_config_flags;
    uint32_t minimum_spec_version;
};


void vkd3d_physical_device_info_init(struct vkd3d_physical_device_info *info, struct d3d12_device *device);

#define VK_EXTENSION(name, member) \
        {VK_ ## name ## _EXTENSION_NAME, offsetof(struct vkd3d_vulkan_info, member), 0, 0}

#define VK_EXTENSION_COND(name, member, required_flags) \
        {VK_ ## name ## _EXTENSION_NAME, offsetof(struct vkd3d_vulkan_info, member), required_flags, 0}
#define VK_EXTENSION_DISABLE_COND(name, member, disable_flags) \
        {VK_ ## name ## _EXTENSION_NAME, offsetof(struct vkd3d_vulkan_info, member), 0, disable_flags, 0}
#define VK_EXTENSION_VERSION(name, member, spec_version) \
        {VK_ ## name ## _EXTENSION_NAME, offsetof(struct vkd3d_vulkan_info, member), 0, 0, spec_version}

static const struct vkd3d_optional_extension_info optional_instance_extensions[] =
{
    /* EXT extensions */
    VK_EXTENSION_COND(EXT_DEBUG_UTILS, EXT_debug_utils, VKD3D_CONFIG_FLAG_DEBUG_UTILS | VKD3D_CONFIG_FLAG_FAULT),
};

static const struct vkd3d_optional_extension_info optional_device_extensions[] =
{
    /* KHR extensions */
    VK_EXTENSION(KHR_PUSH_DESCRIPTOR, KHR_push_descriptor),
    VK_EXTENSION(KHR_PIPELINE_LIBRARY, KHR_pipeline_library),
    VK_EXTENSION_DISABLE_COND(KHR_RAY_TRACING_PIPELINE, KHR_ray_tracing_pipeline, VKD3D_CONFIG_FLAG_NO_DXR),
    VK_EXTENSION_DISABLE_COND(KHR_ACCELERATION_STRUCTURE, KHR_acceleration_structure, VKD3D_CONFIG_FLAG_NO_DXR),
    VK_EXTENSION_DISABLE_COND(KHR_DEFERRED_HOST_OPERATIONS, KHR_deferred_host_operations, VKD3D_CONFIG_FLAG_NO_DXR),
    VK_EXTENSION_DISABLE_COND(KHR_RAY_QUERY, KHR_ray_query, VKD3D_CONFIG_FLAG_NO_DXR),
    VK_EXTENSION_DISABLE_COND(KHR_RAY_TRACING_MAINTENANCE_1, KHR_ray_tracing_maintenance1, VKD3D_CONFIG_FLAG_NO_DXR),
    VK_EXTENSION(KHR_FRAGMENT_SHADING_RATE, KHR_fragment_shading_rate),
    VK_EXTENSION(KHR_FRAGMENT_SHADER_BARYCENTRIC, KHR_fragment_shader_barycentric),
    VK_EXTENSION(KHR_PRESENT_ID, KHR_present_id),
    VK_EXTENSION(KHR_PRESENT_WAIT, KHR_present_wait),
    VK_EXTENSION(KHR_MAINTENANCE_5, KHR_maintenance5),
    VK_EXTENSION(KHR_MAINTENANCE_6, KHR_maintenance6),
    VK_EXTENSION(KHR_SHADER_MAXIMAL_RECONVERGENCE, KHR_shader_maximal_reconvergence),
    VK_EXTENSION(KHR_SHADER_QUAD_CONTROL, KHR_shader_quad_control),
    VK_EXTENSION(KHR_COMPUTE_SHADER_DERIVATIVES, KHR_compute_shader_derivatives),
#ifdef _WIN32
    VK_EXTENSION(KHR_EXTERNAL_MEMORY_WIN32, KHR_external_memory_win32),
    VK_EXTENSION(KHR_EXTERNAL_SEMAPHORE_WIN32, KHR_external_semaphore_win32),
#endif
    /* EXT extensions */
    VK_EXTENSION(EXT_CALIBRATED_TIMESTAMPS, EXT_calibrated_timestamps),
    VK_EXTENSION(EXT_CONDITIONAL_RENDERING, EXT_conditional_rendering),
    VK_EXTENSION(EXT_CONSERVATIVE_RASTERIZATION, EXT_conservative_rasterization),
    VK_EXTENSION(EXT_CUSTOM_BORDER_COLOR, EXT_custom_border_color),
    VK_EXTENSION(EXT_DEPTH_CLIP_ENABLE, EXT_depth_clip_enable),
    VK_EXTENSION(EXT_DEVICE_GENERATED_COMMANDS, EXT_device_generated_commands),
    VK_EXTENSION(EXT_IMAGE_VIEW_MIN_LOD, EXT_image_view_min_lod),
    VK_EXTENSION(EXT_ROBUSTNESS_2, EXT_robustness2),
    VK_EXTENSION(EXT_SHADER_STENCIL_EXPORT, EXT_shader_stencil_export),
    VK_EXTENSION(EXT_TRANSFORM_FEEDBACK, EXT_transform_feedback),
    VK_EXTENSION(EXT_VERTEX_ATTRIBUTE_DIVISOR, EXT_vertex_attribute_divisor),
    VK_EXTENSION(EXT_EXTENDED_DYNAMIC_STATE_2, EXT_extended_dynamic_state2),
    VK_EXTENSION(EXT_EXTENDED_DYNAMIC_STATE_3, EXT_extended_dynamic_state3),
    VK_EXTENSION(EXT_EXTERNAL_MEMORY_HOST, EXT_external_memory_host),
    VK_EXTENSION(EXT_SHADER_IMAGE_ATOMIC_INT64, EXT_shader_image_atomic_int64),
    VK_EXTENSION(EXT_MESH_SHADER, EXT_mesh_shader),
    VK_EXTENSION(EXT_MUTABLE_DESCRIPTOR_TYPE, EXT_mutable_descriptor_type),
    VK_EXTENSION(EXT_HDR_METADATA, EXT_hdr_metadata),
    VK_EXTENSION(EXT_SHADER_MODULE_IDENTIFIER, EXT_shader_module_identifier),
    VK_EXTENSION(EXT_DESCRIPTOR_BUFFER, EXT_descriptor_buffer),
    VK_EXTENSION_DISABLE_COND(EXT_PIPELINE_LIBRARY_GROUP_HANDLES, EXT_pipeline_library_group_handles, VKD3D_CONFIG_FLAG_NO_DXR),
    VK_EXTENSION(EXT_IMAGE_SLICED_VIEW_OF_3D, EXT_image_sliced_view_of_3d),
    VK_EXTENSION(EXT_GRAPHICS_PIPELINE_LIBRARY, EXT_graphics_pipeline_library),
    VK_EXTENSION(EXT_FRAGMENT_SHADER_INTERLOCK, EXT_fragment_shader_interlock),
    VK_EXTENSION(EXT_PAGEABLE_DEVICE_LOCAL_MEMORY, EXT_pageable_device_local_memory),
    VK_EXTENSION(EXT_MEMORY_PRIORITY, EXT_memory_priority),
    VK_EXTENSION(EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS, EXT_dynamic_rendering_unused_attachments),
    VK_EXTENSION(EXT_LINE_RASTERIZATION, EXT_line_rasterization),
    VK_EXTENSION(EXT_IMAGE_COMPRESSION_CONTROL, EXT_image_compression_control),
    VK_EXTENSION_COND(EXT_DEVICE_FAULT, EXT_device_fault, VKD3D_CONFIG_FLAG_FAULT),
    VK_EXTENSION(EXT_MEMORY_BUDGET, EXT_memory_budget),
    VK_EXTENSION_COND(EXT_DEVICE_ADDRESS_BINDING_REPORT, EXT_device_address_binding_report, VKD3D_CONFIG_FLAG_FAULT),
    VK_EXTENSION(EXT_DEPTH_BIAS_CONTROL, EXT_depth_bias_control),
    /* AMD extensions */
    VK_EXTENSION(AMD_BUFFER_MARKER, AMD_buffer_marker),
    VK_EXTENSION(AMD_DEVICE_COHERENT_MEMORY, AMD_device_coherent_memory),
    VK_EXTENSION(AMD_SHADER_CORE_PROPERTIES, AMD_shader_core_properties),
    VK_EXTENSION(AMD_SHADER_CORE_PROPERTIES_2, AMD_shader_core_properties2),
    /* NV extensions */
    VK_EXTENSION(NV_OPTICAL_FLOW, NV_optical_flow),
    VK_EXTENSION(NV_SHADER_SM_BUILTINS, NV_shader_sm_builtins),
    VK_EXTENSION(NVX_BINARY_IMPORT, NVX_binary_import),
    VK_EXTENSION(NVX_IMAGE_VIEW_HANDLE, NVX_image_view_handle),
    VK_EXTENSION(NV_FRAGMENT_SHADER_BARYCENTRIC, NV_fragment_shader_barycentric),
    VK_EXTENSION(NV_COMPUTE_SHADER_DERIVATIVES, NV_compute_shader_derivatives),
    VK_EXTENSION_COND(NV_DEVICE_DIAGNOSTIC_CHECKPOINTS, NV_device_diagnostic_checkpoints, VKD3D_CONFIG_FLAG_BREADCRUMBS | VKD3D_CONFIG_FLAG_BREADCRUMBS_TRACE),
    VK_EXTENSION(NV_DEVICE_GENERATED_COMMANDS, NV_device_generated_commands),
    VK_EXTENSION(NV_SHADER_SUBGROUP_PARTITIONED, NV_shader_subgroup_partitioned),
    VK_EXTENSION(NV_MEMORY_DECOMPRESSION, NV_memory_decompression),
    VK_EXTENSION(NV_DEVICE_GENERATED_COMMANDS_COMPUTE, NV_device_generated_commands_compute),
    VK_EXTENSION_VERSION(NV_LOW_LATENCY_2, NV_low_latency2, 2),
    VK_EXTENSION(NV_RAW_ACCESS_CHAINS, NV_raw_access_chains),
    /* VALVE extensions */
    VK_EXTENSION(VALVE_MUTABLE_DESCRIPTOR_TYPE, VALVE_mutable_descriptor_type),
    /* MESA extensions */
    VK_EXTENSION(MESA_IMAGE_ALIGNMENT_CONTROL, MESA_image_alignment_control),
};

static const struct vkd3d_optional_extension_info optional_extensions_user[] =
{
    VK_EXTENSION(EXT_SURFACE_MAINTENANCE_1, EXT_surface_maintenance1),
    VK_EXTENSION(EXT_SWAPCHAIN_MAINTENANCE_1, EXT_swapchain_maintenance1),
};


static unsigned int get_spec_version(const std::vector<VkExtensionProperties>& extensions,
        const char *extension_name)
{
    unsigned int i;

    for (auto& ext : extensions)
    {
        if (!strcmp(ext.extensionName, extension_name))
            return ext.specVersion;
    }
    return 0;
}

static bool is_extension_disabled(const char *extension_name)
{
    //char disabled_extensions[VKD3D_PATH_MAX];

    //if (!vkd3d_get_env_var("VKD3D_DISABLE_EXTENSIONS", disabled_extensions, sizeof(disabled_extensions)))
        return false;

    //return vkd3d_debug_list_has_member(disabled_extensions, extension_name);
}


static void vkd3d_mark_enabled_user_extensions(struct vkd3d_vulkan_info *vulkan_info,
        const char * const *optional_user_extensions,
        unsigned int optional_user_extension_count,
        const bool *user_extension_supported)
{
    unsigned int i, j;
    for (i = 0; i < optional_user_extension_count; ++i)
    {
        if (!user_extension_supported[i])
            continue;

        /* Mark these external extensions as supported if outer code explicitly requested them,
         * otherwise, ignore. */
        for (j = 0; j < ARRAY_SIZE(optional_extensions_user); j++)
        {
            if (!strcmp(optional_extensions_user[j].extension_name, optional_user_extensions[i]))
            {
                ptrdiff_t offset = optional_extensions_user[j].vulkan_info_offset;
                bool *supported = (bool *)((uintptr_t)vulkan_info + offset);
                *supported = true;
                break;
            }
        }
    }
}
    
static unsigned int vkd3d_append_extension(const char *extensions[],
        unsigned int extension_count, const char *extension_name)
{
    unsigned int i;

    /* avoid duplicates */
    for (i = 0; i < extension_count; ++i)
    {
        if (!strcmp(extensions[i], extension_name))
            return extension_count;
    }

    extensions[extension_count++] = extension_name;
    return extension_count;
}


static bool has_extension(const std::vector<VkExtensionProperties>& extensions,
    const char *extension_name, uint32_t minimum_spec_version)
{
    unsigned int i;

    for (auto& ext : extensions)
    {
        if (is_extension_disabled(extension_name))
        {
            WARN("Extension %s is disabled.\n", debugstr_a(extension_name));
            continue;
        }
        if (!strcmp(ext.extensionName, extension_name) &&
                (ext.specVersion >= minimum_spec_version))
            return true;
    }
    return false;
}

static unsigned int vkd3d_check_extensions(const std::vector<VkExtensionProperties> &extensions,
        const char * const *required_extensions, unsigned int required_extension_count,
        const struct vkd3d_optional_extension_info *optional_extensions, unsigned int optional_extension_count,
        const char * const *user_extensions, unsigned int user_extension_count,
        const char * const *optional_user_extensions, unsigned int optional_user_extension_count,
        bool *user_extension_supported, struct vkd3d_vulkan_info *vulkan_info, const char *extension_type)
{
    unsigned int extension_count = 0;
    unsigned int i;

    for (i = 0; i < required_extension_count; ++i)
    {
        if (!has_extension(extensions, required_extensions[i], 0))
            ERR("Required %s extension %s is not supported.\n",
                    extension_type, debugstr_a(required_extensions[i]));
        ++extension_count;
    }

    for (i = 0; i < optional_extension_count; ++i)
    {
        uint64_t disable_flags = optional_extensions[i].disable_config_flags;
        const char *extension_name = optional_extensions[i].extension_name;
        uint64_t enable_flags = optional_extensions[i].enable_config_flags;
        uint32_t minimum_spec_version = optional_extensions[i].minimum_spec_version;
        ptrdiff_t offset = optional_extensions[i].vulkan_info_offset;
        bool *supported = (void *)((uintptr_t)vulkan_info + offset);

        if (enable_flags && !(vkd3d_config_flags & enable_flags))
            continue;
        if (disable_flags && (vkd3d_config_flags & disable_flags))
            continue;

        if ((*supported = has_extension(extensions, extension_name, minimum_spec_version)))
        {
            TRACE("Found %s extension.\n", debugstr_a(extension_name));
            ++extension_count;
        }
    }

    for (i = 0; i < user_extension_count; ++i)
    {
        if (!has_extension(extensions, user_extensions[i], 0))
            ERR("Required user %s extension %s is not supported.\n",
                    extension_type, debugstr_a(user_extensions[i]));
        ++extension_count;
    }

    assert(!optional_user_extension_count || user_extension_supported);
    for (i = 0; i < optional_user_extension_count; ++i)
    {
        if (has_extension(extensions, optional_user_extensions[i], 0))
        {
            user_extension_supported[i] = true;
            ++extension_count;
        }
        else
        {
            user_extension_supported[i] = false;
            WARN("Optional user %s extension %s is not supported.\n",
                    extension_type, debugstr_a(optional_user_extensions[i]));
        }
    }

    return extension_count;
}

static unsigned int vkd3d_enable_extensions(const char *extensions[],
        const char * const *required_extensions, unsigned int required_extension_count,
        const struct vkd3d_optional_extension_info *optional_extensions, unsigned int optional_extension_count,
        const char * const *user_extensions, unsigned int user_extension_count,
        const char * const *optional_user_extensions, unsigned int optional_user_extension_count,
        const bool *user_extension_supported, const struct vkd3d_vulkan_info *vulkan_info)
{
    unsigned int extension_count = 0;
    unsigned int i;

    for (i = 0; i < required_extension_count; ++i)
    {
        extensions[extension_count++] = required_extensions[i];
    }
    for (i = 0; i < optional_extension_count; ++i)
    {
        ptrdiff_t offset = optional_extensions[i].vulkan_info_offset;
        const bool *supported = (void *)((uintptr_t)vulkan_info + offset);

        if (*supported)
            extensions[extension_count++] = optional_extensions[i].extension_name;
    }

    for (i = 0; i < user_extension_count; ++i)
    {
        extension_count = vkd3d_append_extension(extensions, extension_count, user_extensions[i]);
    }
    assert(!optional_user_extension_count || user_extension_supported);
    for (i = 0; i < optional_user_extension_count; ++i)
    {
        if (!user_extension_supported[i])
            continue;
        extension_count = vkd3d_append_extension(extensions, extension_count, optional_user_extensions[i]);
    }

    return extension_count;
}


static HRESULT vkd3d_init_device_extensions(struct d3d12_device *device,
        const struct vkd3d_device_create_info *create_info,
        uint32_t *device_extension_count, bool *user_extension_supported)
{
    const struct vkd3d_vk_instance_procs *vk_procs = &device->vkd3d_instance->vk_procs;
    VkPhysicalDevice physical_device = device->vk_physical_device;
    struct vkd3d_vulkan_info *vulkan_info = &device->vk_info;
    std::vector<VkExtensionProperties> vk_extensions;
    uint32_t count;

    *device_extension_count = 0;

    VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &count, NULL));
    //{
    //    ERR("Failed to enumerate device extensions, vr %d.\n", vr);
    //    return hresult_from_vk_result(vr);
    //}
    if (!count)
        return S_OK;

    vk_extensions.resize(count);

    TRACE("Enumerating %u device extensions.\n", count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &count, vk_extensions.data()));
    //{
    //    ERR("Failed to enumerate device extensions, vr %d.\n", vr);
    //    vkd3d_free(vk_extensions);
    //    return hresult_from_vk_result(vr);
    //}

    *device_extension_count = vkd3d_check_extensions(vk_extensions, NULL, 0,
            optional_device_extensions, ARRAY_SIZE(optional_device_extensions),
            create_info->device_extensions,
            create_info->device_extension_count,
            create_info->optional_device_extensions,
            create_info->optional_device_extension_count,
            user_extension_supported, vulkan_info, "device");

    if (get_spec_version(vk_extensions, VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME) < 3)
        vulkan_info->EXT_vertex_attribute_divisor = false;

    return S_OK;
}

static uint32_t vkd3d_find_queue(const std::vector<VkQueueFamilyProperties>& properties,
        VkQueueFlags mask, VkQueueFlags flags)
{
    unsigned int i;

    for (i = 0; i < properties.size(); i++)
    {
        if ((properties[i].queueFlags & mask) == flags)
            return i;
    }

    return VK_QUEUE_FAMILY_IGNORED;
}

#define VKD3D_MAX_QUEUE_COUNT_PER_FAMILY (4u)

/* The queue priorities list contains VKD3D_MAX_QUEUE_COUNT_PER_FAMILY + 1 priorities
 * because it is possible for low latency to add an additional queue for out of band work
 * submission. */
static float queue_priorities[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

static HRESULT vkd3d_select_queues(const struct d3d12_device *device,
        VkPhysicalDevice physical_device, struct vkd3d_device_queue_info *info)
{
    const struct vkd3d_vk_instance_procs *vk_procs = &device->vkd3d_instance->vk_procs;
    std::vector<VkQueueFamilyProperties> queue_properties;
    VkDeviceQueueCreateInfo *queue_info = NULL;
    bool duplicate, single_queue;
    unsigned int i, j;
    uint32_t count;

    memset(info, 0, sizeof(*info));
    single_queue = !!(vkd3d_config_flags & VKD3D_CONFIG_FLAG_SINGLE_QUEUE);

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, NULL);
    queue_properties.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_properties.data());

    info->family_index[VKD3D_QUEUE_FAMILY_GRAPHICS] = vkd3d_find_queue(queue_properties,
            VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    info->family_index[VKD3D_QUEUE_FAMILY_COMPUTE] = vkd3d_find_queue(queue_properties,
            VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, VK_QUEUE_COMPUTE_BIT);

    /* Try to find a dedicated sparse queue family. We may use the sparse queue for initialization purposes,
     * and adding that kind of sync will be quite problematic since we get unintentional stalls, especially in graphics queue. */
    info->family_index[VKD3D_QUEUE_FAMILY_SPARSE_BINDING] = vkd3d_find_queue(queue_properties,
            VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT,
            VK_QUEUE_SPARSE_BINDING_BIT);

    /* Try to find a queue family that isn't graphics. */
    if (info->family_index[VKD3D_QUEUE_FAMILY_SPARSE_BINDING] == VK_QUEUE_FAMILY_IGNORED)
        info->family_index[VKD3D_QUEUE_FAMILY_SPARSE_BINDING] = vkd3d_find_queue(queue_properties,
                VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_SPARSE_BINDING_BIT, VK_QUEUE_SPARSE_BINDING_BIT);

    /* Last resort, pick any queue family that supports sparse. */
    if (info->family_index[VKD3D_QUEUE_FAMILY_SPARSE_BINDING] == VK_QUEUE_FAMILY_IGNORED)
        info->family_index[VKD3D_QUEUE_FAMILY_SPARSE_BINDING] = vkd3d_find_queue(queue_properties,
                VK_QUEUE_SPARSE_BINDING_BIT, VK_QUEUE_SPARSE_BINDING_BIT);

    if (device->vk_info.NV_optical_flow)
        info->family_index[VKD3D_QUEUE_FAMILY_OPTICAL_FLOW] = vkd3d_find_queue(queue_properties,
                VK_QUEUE_OPTICAL_FLOW_BIT_NV, VK_QUEUE_OPTICAL_FLOW_BIT_NV);

    if (info->family_index[VKD3D_QUEUE_FAMILY_COMPUTE] == VK_QUEUE_FAMILY_IGNORED)
        info->family_index[VKD3D_QUEUE_FAMILY_COMPUTE] = info->family_index[VKD3D_QUEUE_FAMILY_GRAPHICS];

    /* Vulkan transfer queue cannot represent some esoteric edge cases that D3D12 copy queue can. */
    if (vkd3d_config_flags & VKD3D_CONFIG_FLAG_TRANSFER_QUEUE)
    {
        info->family_index[VKD3D_QUEUE_FAMILY_TRANSFER] = vkd3d_find_queue(queue_properties,
                VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, VK_QUEUE_TRANSFER_BIT);
    }
    else
    {
        info->family_index[VKD3D_QUEUE_FAMILY_TRANSFER] = info->family_index[VKD3D_QUEUE_FAMILY_COMPUTE];
    }

    /* Prefer compute queues for transfer. When using concurrent sharing, DMA queue tends to force compression off. */
    if (info->family_index[VKD3D_QUEUE_FAMILY_TRANSFER] == VK_QUEUE_FAMILY_IGNORED)
        info->family_index[VKD3D_QUEUE_FAMILY_TRANSFER] = info->family_index[VKD3D_QUEUE_FAMILY_COMPUTE];
    info->family_index[VKD3D_QUEUE_FAMILY_INTERNAL_COMPUTE] = info->family_index[VKD3D_QUEUE_FAMILY_COMPUTE];

    if (single_queue)
    {
        info->family_index[VKD3D_QUEUE_FAMILY_COMPUTE] = info->family_index[VKD3D_QUEUE_FAMILY_GRAPHICS];
        info->family_index[VKD3D_QUEUE_FAMILY_TRANSFER] = info->family_index[VKD3D_QUEUE_FAMILY_GRAPHICS];
    }

    for (i = 0; i < VKD3D_QUEUE_FAMILY_COUNT; ++i)
    {
        if (info->family_index[i] == VK_QUEUE_FAMILY_IGNORED)
            continue;

        info->vk_properties[i] = queue_properties[info->family_index[i]];

        /* Ensure that we don't create the same queue multiple times */
        duplicate = false;

        for (j = 0; j < i && !duplicate; j++)
            duplicate = info->family_index[i] == info->family_index[j];

        if (duplicate)
            continue;

        queue_info = &info->vk_queue_create_info[info->vk_family_count++];
        queue_info->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info->pNext = NULL;
        queue_info->flags = 0;
        queue_info->queueFamilyIndex = info->family_index[i];
        queue_info->queueCount = std::min(info->vk_properties[i].queueCount, VKD3D_MAX_QUEUE_COUNT_PER_FAMILY);
        queue_info->pQueuePriorities = queue_priorities;

        if (single_queue)
            queue_info->queueCount = 1;

    }

    if (info->family_index[VKD3D_QUEUE_FAMILY_GRAPHICS] == VK_QUEUE_FAMILY_IGNORED)
    {
        ERR("Could not find a suitable queue family for a direct command queue.\n");
        return E_FAIL;
    }

    return S_OK;
}

static HRESULT vkd3d_create_vk_device(struct d3d12_device *device,
        const struct vkd3d_device_create_info *create_info)
{
    const struct vkd3d_vk_instance_procs *vk_procs = &device->vkd3d_instance->vk_procs;
    struct vkd3d_device_queue_info device_queue_info;
    VkPhysicalDeviceProperties device_properties;
    std::vector<bool> user_extension_supported;
    VkPhysicalDevice physical_device;
    VkDeviceCreateInfo device_info;
    unsigned int device_index;
    uint32_t extension_count;
    const char **extensions;
    VkDevice vk_device;
    VkResult vr;
    HRESULT hr;

    TRACE("device %p, create_info %p.\n", device, create_info);

    physical_device = create_info->vk_physical_device;

    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    device->api_version = std::min(device_properties.apiVersion, VKD3D_MAX_API_VERSION);

    vkGetPhysicalDeviceMemoryProperties(physical_device, &device->memory_properties);

    if (create_info->optional_device_extension_count)
    {
        user_extension_supported.resize(create_info->optional_device_extension_count);
    }

    if (FAILED(hr = vkd3d_init_device_extensions(device, create_info,
            &extension_count, user_extension_supported)))
    {
        vkd3d_free(user_extension_supported);
        return hr;
    }

    /* Mark any user extensions that might be of use to us.
     * Need to do this here so that we can pass down PDF2 as necessary. */
    vkd3d_mark_enabled_user_extensions(&device->vk_info,
            create_info->optional_device_extensions,
            create_info->optional_device_extension_count,
            user_extension_supported);

    vkd3d_physical_device_info_init(&device->device_info, device);

    if (FAILED(hr = vkd3d_init_device_caps(device, create_info, &device->device_info)))
    {
        vkd3d_free(user_extension_supported);
        return hr;
    }

    if (!(extensions = vkd3d_calloc(extension_count, sizeof(*extensions))))
    {
        vkd3d_free(user_extension_supported);
        return E_OUTOFMEMORY;
    }

    if (FAILED(hr = vkd3d_select_queues(device, physical_device, &device_queue_info)))
    {
        vkd3d_free(user_extension_supported);
        vkd3d_free(extensions);
        return hr;
    }

    TRACE("Using queue family %u for direct command queues.\n",
            device_queue_info.family_index[VKD3D_QUEUE_FAMILY_GRAPHICS]);
    TRACE("Using queue family %u for compute command queues.\n",
            device_queue_info.family_index[VKD3D_QUEUE_FAMILY_COMPUTE]);
    TRACE("Using queue family %u for copy command queues.\n",
            device_queue_info.family_index[VKD3D_QUEUE_FAMILY_TRANSFER]);
    TRACE("Using queue family %u for sparse binding.\n",
            device_queue_info.family_index[VKD3D_QUEUE_FAMILY_SPARSE_BINDING]);
    TRACE("Using queue family %u for optical flow.\n",
            device_queue_info.family_index[VKD3D_QUEUE_FAMILY_OPTICAL_FLOW]);

    /* Create device */
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = device->device_info.features2.pNext;
    device_info.flags = 0;
    device_info.queueCreateInfoCount = device_queue_info.vk_family_count;
    device_info.pQueueCreateInfos = device_queue_info.vk_queue_create_info;
    device_info.enabledLayerCount = 0;
    device_info.ppEnabledLayerNames = NULL;
    device_info.enabledExtensionCount = vkd3d_enable_extensions(extensions, NULL, 0,
            optional_device_extensions, ARRAY_SIZE(optional_device_extensions),
            create_info->device_extensions,
            create_info->device_extension_count,
            create_info->optional_device_extensions,
            create_info->optional_device_extension_count,
            user_extension_supported, &device->vk_info);
    device_info.ppEnabledExtensionNames = extensions;
    device_info.pEnabledFeatures = &device->device_info.features2.features;
    vkd3d_free(user_extension_supported);

    vr = VK_CALL(vkCreateDevice(physical_device, &device_info, NULL, &vk_device));
    if (vr == VK_ERROR_INITIALIZATION_FAILED &&
            vkd3d_disable_nvx_extensions(device, extensions, &device_info.enabledExtensionCount))
    {
        WARN("Disabled extensions that can cause Vulkan device creation to fail, retrying.\n");
        vr = VK_CALL(vkCreateDevice(physical_device, &device_info, NULL, &vk_device));
    }

    device->vk_device = vk_device;

    //if (FAILED(hr = d3d12_device_create_vkd3d_queues(device, &device_queue_info)))
    //{
    //    ERR("Failed to create queues, hr %#x.\n", hr);
    //    device->vk_procs.vkDestroyDevice(vk_device, NULL);
    //    return hr;
    //}

    device->vk_info.extension_count = device_info.enabledExtensionCount;
    device->vk_info.extension_names = extensions;

    //d3d12_device_init_workarounds(device);

    TRACE("Created Vulkan device %p.\n", vk_device);

    return hr;
}



static void vkd3d_init_shader_extensions(struct d3d12_device *device)
{
    bool allow_denorm_control;
    device->vk_info.shader_extension_count = 0;

    device->vk_info.shader_extensions.push_back(
            VKD3D_SHADER_TARGET_EXTENSION_SPV_KHR_INTEGER_DOT_PRODUCT);
    device->vk_info.shader_extensions.push_back(
            VKD3D_SHADER_TARGET_EXTENSION_SPV_EXT_DEMOTE_TO_HELPER_INVOCATION);

    if (d3d12_device_determine_additional_typed_uav_support(device))
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_READ_STORAGE_IMAGE_WITHOUT_FORMAT);
    }

    if (device->device_info.ray_tracing_pipeline_features.rayTraversalPrimitiveCulling)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_RAY_TRACING_PRIMITIVE_CULLING);
    }

    if (device->device_info.vulkan_1_2_features.scalarBlockLayout)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_SCALAR_BLOCK_LAYOUT);

        if (device->device_info.properties2.properties.vendorID == VKD3D_VENDOR_ID_AMD)
        {
            /* Raw load-store instructions on AMD are bounds checked correctly per component.
             * In other cases, we must be careful when emitting byte address buffers and block
             * any attempt to vectorize vec3.
             * We can still vectorize vec3 structured buffers however as long as SCALAR_BLOCK_LAYOUT
             * is supported. */
            device->vk_info.shader_extensions.push_back(
                    VKD3D_SHADER_TARGET_EXTENSION_ASSUME_PER_COMPONENT_SSBO_ROBUSTNESS);
        }
    }

    if (device->device_info.barycentric_features_khr.fragmentShaderBarycentric)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_BARYCENTRIC_KHR);
    }

    if (device->device_info.compute_shader_derivatives_features_khr.computeDerivativeGroupLinear)
    {
        enum vkd3d_shader_target_extension compute_derivative_ext = VKD3D_SHADER_TARGET_EXTENSION_COMPUTE_SHADER_DERIVATIVES_NV;

        if (device->vk_info.KHR_compute_shader_derivatives)
            compute_derivative_ext = VKD3D_SHADER_TARGET_EXTENSION_COMPUTE_SHADER_DERIVATIVES_KHR;

        device->vk_info.shader_extensions.push_back(compute_derivative_ext);
    }

    if (device->d3d12_caps.options4.Native16BitShaderOpsSupported)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_MIN_PRECISION_IS_NATIVE_16BIT);
    }

    /* NV driver implies denorm preserve by default in FP16 and 64, but there's an issue where
     * the explicit denorm preserve state spills into FP32 even when we explicitly want FTZ for FP32.
     * Denorm preserve is only exposed on FP16 on this implementation,
     * so it's technically in-spec to do this,
     * but the only way we can make NV pass our tests is to *not* emit anything at all for 16 and 64. */
    allow_denorm_control =
            device->device_info.vulkan_1_2_properties.driverID != VK_DRIVER_ID_NVIDIA_PROPRIETARY ||
            (vkd3d_config_flags & VKD3D_CONFIG_FLAG_SKIP_DRIVER_WORKAROUNDS);

    if (device->device_info.vulkan_1_2_properties.denormBehaviorIndependence !=
            VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE &&
            allow_denorm_control)
    {
        if (device->device_info.vulkan_1_2_properties.shaderDenormPreserveFloat16)
        {
            device->vk_info.shader_extensions.push_back(
                    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP16_DENORM_PRESERVE);
        }

        if (device->device_info.vulkan_1_2_properties.shaderDenormFlushToZeroFloat32)
        {
            device->vk_info.shader_extensions.push_back(
                    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP32_DENORM_FLUSH);
        }

        if (device->device_info.vulkan_1_2_properties.shaderDenormPreserveFloat64)
        {
            device->vk_info.shader_extensions.push_back(
                    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP64_DENORM_PRESERVE);
        }
    }

    if (device->device_info.vulkan_1_2_properties.shaderSignedZeroInfNanPreserveFloat16)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP16_INF_NAN_PRESERVE);
    }

    if (device->device_info.vulkan_1_2_properties.shaderSignedZeroInfNanPreserveFloat32)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP32_INF_NAN_PRESERVE);
    }

    if (device->device_info.vulkan_1_2_properties.shaderSignedZeroInfNanPreserveFloat64)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP64_INF_NAN_PRESERVE);
    }

    if (device->vk_info.NV_shader_subgroup_partitioned &&
            (device->device_info.vulkan_1_1_properties.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV))
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_SUBGROUP_PARTITIONED_NV);
    }

    if (device->device_info.shader_maximal_reconvergence_features.shaderMaximalReconvergence &&
            device->device_info.shader_quad_control_features.shaderQuadControl)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_QUAD_CONTROL_RECONVERGENCE);
    }

    if (device->device_info.raw_access_chains_nv.shaderRawAccessChains)
    {
        device->vk_info.shader_extensions.push_back(
                VKD3D_SHADER_TARGET_EXTENSION_RAW_ACCESS_CHAINS_NV);
    }
}

static HRESULT d3d12_device_init(struct d3d12_device *device,
        struct vkd3d_instance *instance, const struct vkd3d_device_create_info *create_info)
{
    const struct vkd3d_vk_device_procs *vk_procs;
    HRESULT hr;
    int rc;

    device->vk_info = instance->vk_info;
    device->vk_info.extension_count = 0;
    device->vk_info.extension_names = NULL;

    device->removed_reason = S_OK;

    device->vk_device = VK_NULL_HANDLE;


    if (FAILED(hr = vkd3d_create_vk_device(device, create_info)))
        goto out_free_fragment_output_lock;


    if (FAILED(hr = vkd3d_memory_info_init(&device->memory_info, device)))
        goto out_cleanup_format_info;


    d3d12_device_caps_init(device);

    vkd3d_init_shader_extensions(device);
    return S_OK;

    return hr;
}

HRESULT d3d12_device_create(struct vkd3d_instance *instance,
        const struct vkd3d_device_create_info *create_info, struct d3d12_device **device)
{
    struct d3d12_device *object;
    HRESULT hr;

    pthread_mutex_lock(&d3d12_device_map_mutex);


    if (FAILED(hr = d3d12_device_init(object, instance, create_info)))
    {
        vkd3d_free_aligned(object);
        pthread_mutex_unlock(&d3d12_device_map_mutex);
        return hr;
    }

    //if (!d3d12_device_supports_feature_level(object, create_info->minimum_feature_level))
    //{
    //    WARN("Feature level %#x is not supported.\n", create_info->minimum_feature_level);
    //    d3d12_device_destroy(object);
    //    vkd3d_free_aligned(object);
    //    pthread_mutex_unlock(&d3d12_device_map_mutex);
    //    return E_INVALIDARG;
    //}

    //TRACE("Created device %p (dummy d3d12_device_AddRef for debug grep purposes).\n", object);

    //d3d12_add_device_singleton(object, create_info->adapter_luid);

    //pthread_mutex_unlock(&d3d12_device_map_mutex);

    *device = object;

    return S_OK;
}

static HRESULT vkd3d_init_instance_caps(struct vkd3d_instance *instance,
        const struct vkd3d_instance_create_info *create_info,
        uint32_t *instance_extension_count, bool *user_extension_supported)
{
    struct vkd3d_vulkan_info *vulkan_info = &instance->vk_info;
    std::vector<VkExtensionProperties> vk_extensions;
    uint32_t count;
    VkResult vr;

    memset(vulkan_info, 0, sizeof(*vulkan_info));
    *instance_extension_count = 0;

    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &count, NULL));
    if (!count)
        return S_OK;

    vk_extensions .resize(count);

    TRACE("Enumerating %u instance extensions.\n", count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &count, vk_extensions.data()));

    *instance_extension_count = vkd3d_check_extensions(vk_extensions, NULL, 0,
            optional_instance_extensions, ARRAY_SIZE(optional_instance_extensions),
            create_info->instance_extensions,
            create_info->instance_extension_count,
            create_info->optional_instance_extensions,
            create_info->optional_instance_extension_count,
            user_extension_supported, vulkan_info, "instance");

    return S_OK;
}

static HRESULT vkd3d_instance_init(struct vkd3d_instance *instance,
        const struct vkd3d_instance_create_info *create_info)
{
    const struct vkd3d_vk_global_procs *vk_global_procs = &instance->vk_global_procs;
    const char *debug_layer_name = "VK_LAYER_KHRONOS_validation";
    bool *user_extension_supported = NULL;
    VkApplicationInfo application_info;
    VkInstanceCreateInfo instance_info;
    char application_name[VKD3D_PATH_MAX];
    VkLayerProperties *layers;
    uint32_t extension_count;
    const char **extensions;
    uint32_t layer_count, i;
    VkInstance vk_instance;
    bool dirty_tree_build;
    VkResult vr;
    HRESULT hr;
    uint32_t loader_version = VK_API_VERSION_1_0;

    TRACE("Build: %s.\n", vkd3d_version);

    memset(instance, 0, sizeof(*instance));

    //vkd3d_config_flags_init();

    //if (FAILED(hr = vkd3d_init_vk_global_procs(instance, create_info->pfn_vkGetInstanceProcAddr)))
    //{
    //    ERR("Failed to initialize Vulkan global procs, hr %#x.\n", hr);
    //    return hr;
    //}

    if (create_info->optional_instance_extension_count)
    {
        if (!(user_extension_supported = vkd3d_calloc(create_info->optional_instance_extension_count, sizeof(bool))))
            return E_OUTOFMEMORY;
    }

    if (FAILED(hr = vkd3d_init_instance_caps(instance, create_info,
            &extension_count, user_extension_supported)))
    {
        vkd3d_free(user_extension_supported);
        return hr;
    }

    vkEnumerateInstanceVersion(&loader_version);

    if (loader_version < VKD3D_MIN_API_VERSION)
    {
        ERR("Vulkan %u.%u not supported by loader.\n",
                VK_VERSION_MAJOR(VKD3D_MIN_API_VERSION),
                VK_VERSION_MINOR(VKD3D_MIN_API_VERSION));
        vkd3d_free(user_extension_supported);
        return E_INVALIDARG;
    }

    /* Do not opt-in to versions we don't need yet. */
    if (loader_version > VKD3D_MAX_API_VERSION)
        loader_version = VKD3D_MAX_API_VERSION;

    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pNext = NULL;
    application_info.pApplicationName = NULL;
    application_info.applicationVersion = 0;
    application_info.pEngineName = "vkd3d";
    application_info.engineVersion = 1;
    application_info.apiVersion = loader_version;

    //if (vkd3d_get_program_name(application_name))
    //    application_info.pApplicationName = application_name;

    //TRACE("Application: %s.\n", debugstr_a(application_info.pApplicationName));

    if (!(extensions = vkd3d_calloc(extension_count, sizeof(*extensions))))
    {
        vkd3d_free(user_extension_supported);
        return E_OUTOFMEMORY;
    }

    vkd3d_mark_enabled_user_extensions(&instance->vk_info,
            create_info->optional_instance_extensions,
            create_info->optional_instance_extension_count,
            user_extension_supported);

    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = NULL;
    instance_info.flags = 0;
    instance_info.pApplicationInfo = &application_info;
    instance_info.enabledLayerCount = 0;
    instance_info.ppEnabledLayerNames = NULL;
    instance_info.enabledExtensionCount = vkd3d_enable_extensions(extensions, NULL, 0,
            optional_instance_extensions, ARRAY_SIZE(optional_instance_extensions),
            create_info->instance_extensions,
            create_info->instance_extension_count,
            create_info->optional_instance_extensions,
            create_info->optional_instance_extension_count,
            user_extension_supported, &instance->vk_info);
    instance_info.ppEnabledExtensionNames = extensions;
    vkd3d_free(user_extension_supported);

    if (vkd3d_config_flags & VKD3D_CONFIG_FLAG_VULKAN_DEBUG)
    {
        layers = NULL;

        if (vk_global_procs->vkEnumerateInstanceLayerProperties(&layer_count, NULL) == VK_SUCCESS &&
            layer_count &&
            (layers = vkd3d_malloc(layer_count * sizeof(*layers))) &&
            vk_global_procs->vkEnumerateInstanceLayerProperties(&layer_count, layers) == VK_SUCCESS)
        {
            for (i = 0; i < layer_count; i++)
            {
                if (strcmp(layers[i].layerName, debug_layer_name) == 0)
                {
                    instance_info.enabledLayerCount = 1;
                    instance_info.ppEnabledLayerNames = &debug_layer_name;
                    break;
                }
            }
        }

        if (instance_info.enabledLayerCount == 0)
        {
            ERR("Failed to enumerate instance layers, will not use VK_LAYER_KHRONOS_validation directly.\n"
                "Use VKD3D_CONFIG=vk_debug VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation instead!\n");
        }
        vkd3d_free(layers);
    }

    VK_CHECK(vkCreateInstance(&instance_info, NULL, &vk_instance));

    instance->vk_instance = vk_instance;
    instance->instance_version = loader_version;

    instance->vk_info.extension_count = instance_info.enabledExtensionCount;
    instance->vk_info.extension_names = extensions;

    TRACE("Created Vulkan instance %p, version %u.%u.\n", vk_instance,
            VK_VERSION_MAJOR(loader_version),
            VK_VERSION_MINOR(loader_version));

    instance->refcount = 1;

    instance->vk_debug_callback = VK_NULL_HANDLE;
    if (instance->vk_info.EXT_debug_utils && (vkd3d_config_flags & VKD3D_CONFIG_FLAG_VULKAN_DEBUG))
        vkd3d_init_debug_messenger_callback(instance);

#ifdef VKD3D_ENABLE_RENDERDOC
    /* Need to init this sometime after creating the instance so that the layer has loaded. */
    vkd3d_renderdoc_init();
#endif

#ifdef VKD3D_ENABLE_DESCRIPTOR_QA
    vkd3d_descriptor_debug_init();
#endif

    return S_OK;
}


HRESULT vkd3d_create_instance(const struct vkd3d_instance_create_info *create_info,
        struct vkd3d_instance **instance)
{
    struct vkd3d_instance *object;
    HRESULT hr = S_OK;

   
    TRACE("create_info %p, instance %p.\n", create_info, instance);

    if (!(object = vkd3d_malloc(sizeof(*object))))
    {
        hr = E_OUTOFMEMORY;
        goto out_unlock;
    }

    if (FAILED(hr = vkd3d_instance_init(object, create_info)))
    {
        vkd3d_free(object);
        goto out_unlock;
    }

    TRACE("Created instance %p.\n", object);

    *instance = object;
    return hr;
}

static HRESULT vkd3d_create_instance_global(struct vkd3d_instance **out_instance)
{
    struct vkd3d_instance_create_info instance_create_info;
    HRESULT hr = S_OK;

    static const char * const instance_extensions[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
        "VK_KHR_win32_surface",
#else
        /* TODO: We need to attempt to dlopen() native DXVK DXGI and handle this more gracefully. */
        "VK_KHR_xcb_surface",
#endif
    };

    static const char * const optional_instance_extensions[] =
    {
        VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    };

    if (!load_modules())
    {
        ERR("Failed to load Vulkan library.\n");
        return E_FAIL;
    }

    memset(&instance_create_info, 0, sizeof(instance_create_info));
    instance_create_info.pfn_vkGetInstanceProcAddr = vulkan_vkGetInstanceProcAddr;
    instance_create_info.instance_extensions = instance_extensions;
    instance_create_info.instance_extension_count = ARRAY_SIZE(instance_extensions);
    instance_create_info.optional_instance_extensions = optional_instance_extensions;
    instance_create_info.optional_instance_extension_count = ARRAY_SIZE(optional_instance_extensions);



    if (FAILED(hr = vkd3d_create_instance(&instance_create_info, out_instance)))
        WARN("Failed to create vkd3d instance, hr %#x.\n", hr);



    return hr;
}


HRESULT vkd3d_create_device(const struct vkd3d_device_create_info *create_info)
{
    struct vkd3d_instance *instance;
    struct d3d12_device *object;
    HRESULT hr;

    TRACE("create_info %p, iid %s, device %p.\n", create_info, debugstr_guid(iid), device);

    if (FAILED(hr = vkd3d_create_instance(create_info->instance_create_info, &instance)))
    {
        WARN("Failed to create instance, hr %#x.\n", hr);
        return E_FAIL;
    }

    hr = d3d12_device_create(instance, create_info, &object);
    
    return hr;
}

static HRESULT d3d12core_CreateDevice()
{
    struct vkd3d_device_create_info device_create_info;
    struct vkd3d_instance *instance;

    static const char * const device_extensions[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    static const char * const optional_device_extensions[] =
    {
        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
    };


    if (FAILED(hr = vkd3d_create_instance_global(&instance)))
        return hr;

    memset(&device_create_info, 0, sizeof(device_create_info));
    device_create_info.instance = instance;
    device_create_info.instance_create_info = NULL;
    device_create_info.device_extensions = device_extensions;
    device_create_info.device_extension_count = ARRAY_SIZE(device_extensions);
    device_create_info.optional_device_extensions = optional_device_extensions;
    device_create_info.optional_device_extension_count = ARRAY_SIZE(optional_device_extensions);


    hr = vkd3d_create_device(&device_create_info);

    return hr;
}

static inline void vk_prepend_struct(void *header, void *structure)
{
    auto *vk_header = (VkBaseOutStructure*)header, *vk_structure = (VkBaseOutStructure*)structure;

    assert(!vk_structure->pNext);
    vk_structure->pNext = vk_header->pNext;
    vk_header->pNext = vk_structure;
}
    
static void QueryVulkanExtensionInfo(DeviceExtensionInfo& info, VkPhysicalDevice physical_device)
{


}

    
static void vkd3d_physical_device_info_init(struct vkd3d_physical_device_info *info, struct d3d12_device *device)
{
    const struct vkd3d_vk_instance_procs *vk_procs = &device->vkd3d_instance->vk_procs;
    struct vkd3d_vulkan_info *vulkan_info = &device->vk_info;

    memset(info, 0, sizeof(*info));

    info->features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    info->properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    info->vulkan_1_1_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vk_prepend_struct(&info->features2, &info->vulkan_1_1_features);
    info->vulkan_1_1_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
    vk_prepend_struct(&info->properties2, &info->vulkan_1_1_properties);

    info->vulkan_1_2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk_prepend_struct(&info->features2, &info->vulkan_1_2_features);
    info->vulkan_1_2_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
    vk_prepend_struct(&info->properties2, &info->vulkan_1_2_properties);

    info->vulkan_1_3_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk_prepend_struct(&info->features2, &info->vulkan_1_3_features);
    info->vulkan_1_3_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
    vk_prepend_struct(&info->properties2, &info->vulkan_1_3_properties);

    if (vulkan_info->KHR_push_descriptor)
    {
        info->push_descriptor_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
        vk_prepend_struct(&info->properties2, &info->push_descriptor_properties);
    }

    if (vulkan_info->EXT_calibrated_timestamps)
        info->time_domains = vkd3d_physical_device_get_time_domains(device);

    if (vulkan_info->EXT_conditional_rendering)
    {
        info->conditional_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->conditional_rendering_features);
    }

    if (vulkan_info->EXT_conservative_rasterization)
    {
        info->conservative_rasterization_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT;
        vk_prepend_struct(&info->properties2, &info->conservative_rasterization_properties);
    }

    if (vulkan_info->EXT_custom_border_color)
    {
        info->custom_border_color_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->custom_border_color_features);
        info->custom_border_color_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT;
        vk_prepend_struct(&info->properties2, &info->custom_border_color_properties);
    }

    if (vulkan_info->EXT_depth_clip_enable)
    {
        info->depth_clip_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->depth_clip_features);
    }

    if (vulkan_info->EXT_device_generated_commands)
    {
        info->device_generated_commands_features_ext.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_EXT;
        info->device_generated_commands_properties_ext.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT;
        vk_prepend_struct(&info->features2, &info->device_generated_commands_features_ext);
        vk_prepend_struct(&info->properties2, &info->device_generated_commands_properties_ext);
    }

    if (vulkan_info->EXT_robustness2)
    {
        info->robustness2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->robustness2_features);
        info->robustness2_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT;
        vk_prepend_struct(&info->properties2, &info->robustness2_properties);
    }

    if (vulkan_info->EXT_transform_feedback)
    {
        info->xfb_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->xfb_features);
        info->xfb_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
        vk_prepend_struct(&info->properties2, &info->xfb_properties);
    }

    if (vulkan_info->EXT_vertex_attribute_divisor)
    {
        info->vertex_divisor_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->vertex_divisor_features);
        info->vertex_divisor_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;
        vk_prepend_struct(&info->properties2, &info->vertex_divisor_properties);
    }

    if (vulkan_info->EXT_extended_dynamic_state2)
    {
        info->extended_dynamic_state2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->extended_dynamic_state2_features);
    }

    if (vulkan_info->EXT_extended_dynamic_state3)
    {
        info->extended_dynamic_state3_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->extended_dynamic_state3_features);
    }

    if (vulkan_info->EXT_external_memory_host)
    {
        info->external_memory_host_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
        vk_prepend_struct(&info->properties2, &info->external_memory_host_properties);
    }

    if (vulkan_info->AMD_shader_core_properties)
    {
        info->shader_core_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD;
        vk_prepend_struct(&info->properties2, &info->shader_core_properties);
    }

    if (vulkan_info->AMD_shader_core_properties2)
    {
        info->shader_core_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD;
        vk_prepend_struct(&info->properties2, &info->shader_core_properties2);
    }

    if (vulkan_info->NV_shader_sm_builtins)
    {
        info->shader_sm_builtins_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV;
        vk_prepend_struct(&info->properties2, &info->shader_sm_builtins_properties);
    }

    if (vulkan_info->VALVE_mutable_descriptor_type || vulkan_info->EXT_mutable_descriptor_type)
    {
        info->mutable_descriptor_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->mutable_descriptor_features);
    }

    if (vulkan_info->EXT_image_view_min_lod)
    {
        info->image_view_min_lod_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->image_view_min_lod_features);
    }

    

    if (vulkan_info->KHR_acceleration_structure && vulkan_info->KHR_ray_tracing_pipeline &&
        vulkan_info->KHR_deferred_host_operations)
    {
        info->acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        info->acceleration_structure_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
        info->ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        info->ray_tracing_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        vk_prepend_struct(&info->features2, &info->acceleration_structure_features);
        vk_prepend_struct(&info->features2, &info->ray_tracing_pipeline_features);
        vk_prepend_struct(&info->properties2, &info->acceleration_structure_properties);
        vk_prepend_struct(&info->properties2, &info->ray_tracing_pipeline_properties);
    }

    if (vulkan_info->KHR_ray_query)
    {
        info->ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->ray_query_features);
    }

    if (vulkan_info->KHR_ray_tracing_maintenance1)
    {
        info->ray_tracing_maintenance1_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->ray_tracing_maintenance1_features);
    }

    if (vulkan_info->KHR_fragment_shading_rate)
    {
        info->fragment_shading_rate_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
        info->fragment_shading_rate_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
        vk_prepend_struct(&info->properties2, &info->fragment_shading_rate_properties);
        vk_prepend_struct(&info->features2, &info->fragment_shading_rate_features);
    }

    if (vulkan_info->NV_fragment_shader_barycentric && !vulkan_info->KHR_fragment_shader_barycentric)
    {
        info->barycentric_features_nv.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV;
        vk_prepend_struct(&info->features2, &info->barycentric_features_nv);
    }

    if (vulkan_info->KHR_fragment_shader_barycentric)
    {
        info->barycentric_features_khr.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->barycentric_features_khr);
    }

    if (vulkan_info->NV_device_generated_commands)
    {
        info->device_generated_commands_features_nv.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV;
        info->device_generated_commands_properties_nv.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV;
        vk_prepend_struct(&info->features2, &info->device_generated_commands_features_nv);
        vk_prepend_struct(&info->properties2, &info->device_generated_commands_properties_nv);
    }

    if (vulkan_info->NV_device_generated_commands_compute)
    {
        info->device_generated_commands_compute_features_nv.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_COMPUTE_FEATURES_NV;
        vk_prepend_struct(&info->features2, &info->device_generated_commands_compute_features_nv);
    }

    if (vulkan_info->EXT_shader_image_atomic_int64)
    {
        info->shader_image_atomic_int64_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->shader_image_atomic_int64_features);
    }

    if (vulkan_info->AMD_device_coherent_memory)
    {
        info->device_coherent_memory_features_amd.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD;
        vk_prepend_struct(&info->features2, &info->device_coherent_memory_features_amd);
    }

    if (vulkan_info->EXT_mesh_shader)
    {
        info->mesh_shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        info->mesh_shader_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
        vk_prepend_struct(&info->features2, &info->mesh_shader_features);
        vk_prepend_struct(&info->properties2, &info->mesh_shader_properties);
    }

    if (vulkan_info->EXT_shader_module_identifier)
    {
        info->shader_module_identifier_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT;
        info->shader_module_identifier_properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_PROPERTIES_EXT;
        vk_prepend_struct(&info->features2, &info->shader_module_identifier_features);
        vk_prepend_struct(&info->properties2, &info->shader_module_identifier_properties);
    }

    if (vulkan_info->KHR_present_id)
    {
        info->present_id_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->present_id_features);
    }

    if (vulkan_info->KHR_present_wait)
    {
        info->present_wait_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->present_wait_features);
    }

    if (vulkan_info->KHR_maintenance5)
    {
        info->maintenance_5_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
        info->maintenance_5_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR;
        vk_prepend_struct(&info->features2, &info->maintenance_5_features);
        vk_prepend_struct(&info->properties2, &info->maintenance_5_properties);
    }

    if (vulkan_info->KHR_maintenance6)
    {
        info->maintenance_6_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR;
        info->maintenance_6_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR;
        vk_prepend_struct(&info->features2, &info->maintenance_6_features);
        vk_prepend_struct(&info->properties2, &info->maintenance_6_properties);
    }

    if (vulkan_info->KHR_shader_maximal_reconvergence)
    {
        info->shader_maximal_reconvergence_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MAXIMAL_RECONVERGENCE_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->shader_maximal_reconvergence_features);
    }

    if (vulkan_info->KHR_shader_quad_control)
    {
        info->shader_quad_control_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_QUAD_CONTROL_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->shader_quad_control_features);
    }

    if (vulkan_info->KHR_compute_shader_derivatives || vulkan_info->NV_compute_shader_derivatives)
    {
        info->compute_shader_derivatives_features_khr.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR;
        vk_prepend_struct(&info->features2, &info->compute_shader_derivatives_features_khr);
    }

    if (vulkan_info->KHR_compute_shader_derivatives)
    {
        info->compute_shader_derivatives_properties_khr.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_PROPERTIES_KHR;
        vk_prepend_struct(&info->properties2, &info->compute_shader_derivatives_properties_khr);
    }

    if (vulkan_info->EXT_descriptor_buffer)
    {
        info->descriptor_buffer_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
        info->descriptor_buffer_properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
        vk_prepend_struct(&info->features2, &info->descriptor_buffer_features);
        vk_prepend_struct(&info->properties2, &info->descriptor_buffer_properties);
    }

    if (vulkan_info->EXT_pipeline_library_group_handles)
    {
        info->pipeline_library_group_handles_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_LIBRARY_GROUP_HANDLES_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->pipeline_library_group_handles_features);
    }

    if (vulkan_info->EXT_image_sliced_view_of_3d)
    {
        info->image_sliced_view_of_3d_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_SLICED_VIEW_OF_3D_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->image_sliced_view_of_3d_features);
    }

    if (vulkan_info->EXT_graphics_pipeline_library)
    {
        info->graphics_pipeline_library_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
        info->graphics_pipeline_library_properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;
        vk_prepend_struct(&info->features2, &info->graphics_pipeline_library_features);
        vk_prepend_struct(&info->properties2, &info->graphics_pipeline_library_properties);
    }

    if (vulkan_info->EXT_fragment_shader_interlock)
    {
        info->fragment_shader_interlock_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->fragment_shader_interlock_features);
    }

    if (vulkan_info->EXT_memory_priority)
    {
        info->memory_priority_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->memory_priority_features);
    }

    if (vulkan_info->EXT_pageable_device_local_memory)
    {
        info->pageable_device_memory_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->pageable_device_memory_features);
    }

    /* This EXT only exists to remove VUID for validation, it does not add new functionality. */
    if (vulkan_info->EXT_dynamic_rendering_unused_attachments)
    {
        info->dynamic_rendering_unused_attachments_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->dynamic_rendering_unused_attachments_features);
    }

    if (vulkan_info->EXT_line_rasterization)
    {
        info->line_rasterization_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
        info->line_rasterization_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT;
        vk_prepend_struct(&info->features2, &info->line_rasterization_features);
        vk_prepend_struct(&info->properties2, &info->line_rasterization_properties);
    }

    if (vulkan_info->EXT_image_compression_control)
    {
        info->image_compression_control_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->image_compression_control_features);
    }

    if (vulkan_info->NV_memory_decompression)
    {
        info->memory_decompression_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_FEATURES_NV;
        info->memory_decompression_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_PROPERTIES_NV;
        vk_prepend_struct(&info->features2, &info->memory_decompression_features);
        vk_prepend_struct(&info->properties2, &info->memory_decompression_properties);
    }

    if (vulkan_info->EXT_device_fault)
    {
        info->fault_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->fault_features);
    }

    if (vulkan_info->EXT_swapchain_maintenance1)
    {
        info->swapchain_maintenance1_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->swapchain_maintenance1_features);
    }

    if (vulkan_info->NV_raw_access_chains)
    {
        info->raw_access_chains_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV;
        vk_prepend_struct(&info->features2, &info->raw_access_chains_nv);
    }

    if (vulkan_info->EXT_device_address_binding_report)
    {
        info->address_binding_report_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->address_binding_report_features);
    }

    if (vulkan_info->EXT_depth_bias_control)
    {
        info->depth_bias_control_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT;
        vk_prepend_struct(&info->features2, &info->depth_bias_control_features);
    }

    if (vulkan_info->MESA_image_alignment_control)
    {
        info->image_alignment_control_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_FEATURES_MESA;
        info->image_alignment_control_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_PROPERTIES_MESA;
        vk_prepend_struct(&info->features2, &info->image_alignment_control_features);
        vk_prepend_struct(&info->properties2, &info->image_alignment_control_properties);
    }

    if (vulkan_info->NV_optical_flow)
    {
        info->optical_flow_nv_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV;
        vk_prepend_struct(&info->features2, &info->optical_flow_nv_features);
    }

    vkGetPhysicalDeviceFeatures2(device->vk_physical_device, &info->features2);
    vkGetPhysicalDeviceProperties2(device->vk_physical_device, &info->properties2);
}

} // namespace alloy::vk

