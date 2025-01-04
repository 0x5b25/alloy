#pragma once

#include <volk.h>

namespace alloy::vk
{

} // namespace alloy::vk
constexpr auto VKD3D_MAX_API_VERSION = VK_API_VERSION_1_3;
constexpr auto VKD3D_MIN_API_VERSION = VK_API_VERSION_1_1;

enum vkd3d_queue_family
{
    VKD3D_QUEUE_FAMILY_GRAPHICS,
    VKD3D_QUEUE_FAMILY_COMPUTE,
    VKD3D_QUEUE_FAMILY_TRANSFER,
    VKD3D_QUEUE_FAMILY_SPARSE_BINDING,
    VKD3D_QUEUE_FAMILY_OPTICAL_FLOW,
    /* Keep internal queues at the end */
    VKD3D_QUEUE_FAMILY_INTERNAL_COMPUTE,

    VKD3D_QUEUE_FAMILY_COUNT
};

enum vkd3d_shader_target_extension
{
    VKD3D_SHADER_TARGET_EXTENSION_NONE,

    VKD3D_SHADER_TARGET_EXTENSION_SPV_EXT_DEMOTE_TO_HELPER_INVOCATION,
    VKD3D_SHADER_TARGET_EXTENSION_READ_STORAGE_IMAGE_WITHOUT_FORMAT,
    VKD3D_SHADER_TARGET_EXTENSION_SPV_KHR_INTEGER_DOT_PRODUCT,
    VKD3D_SHADER_TARGET_EXTENSION_RAY_TRACING_PRIMITIVE_CULLING,
    VKD3D_SHADER_TARGET_EXTENSION_SCALAR_BLOCK_LAYOUT,

    /* When using scalar block layout with a vec3 array on a byte address buffer,
     * there is diverging behavior across hardware.
     * On AMD, robustness is checked per component, which means we can implement ByteAddressBuffer
     * without further hackery. On NVIDIA, robustness does not seem to work this way, so it's either
     * all in range, or all out of range. We can implement structured buffer vectorization of vec3,
     * but not byte address buffer. */
    VKD3D_SHADER_TARGET_EXTENSION_ASSUME_PER_COMPONENT_SSBO_ROBUSTNESS,
    VKD3D_SHADER_TARGET_EXTENSION_BARYCENTRIC_KHR,
    VKD3D_SHADER_TARGET_EXTENSION_MIN_PRECISION_IS_NATIVE_16BIT,
    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP16_DENORM_PRESERVE,
    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP32_DENORM_FLUSH,
    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP64_DENORM_PRESERVE,
    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP16_INF_NAN_PRESERVE,
    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP32_INF_NAN_PRESERVE,
    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_FP64_INF_NAN_PRESERVE,
    VKD3D_SHADER_TARGET_EXTENSION_SUPPORT_SUBGROUP_PARTITIONED_NV,
    VKD3D_SHADER_TARGET_EXTENSION_COMPUTE_SHADER_DERIVATIVES_NV,
    VKD3D_SHADER_TARGET_EXTENSION_COMPUTE_SHADER_DERIVATIVES_KHR,
    VKD3D_SHADER_TARGET_EXTENSION_QUAD_CONTROL_RECONVERGENCE,
    VKD3D_SHADER_TARGET_EXTENSION_RAW_ACCESS_CHAINS_NV,
    VKD3D_SHADER_TARGET_EXTENSION_COUNT,
};


/* Bindless */
enum vkd3d_bindless_flags
{
    VKD3D_BINDLESS_CBV_AS_SSBO                      = (1u << 0),
    VKD3D_BINDLESS_RAW_SSBO                         = (1u << 1),
    VKD3D_SSBO_OFFSET_BUFFER                        = (1u << 2),
    VKD3D_TYPED_OFFSET_BUFFER                       = (1u << 3),
    VKD3D_RAW_VA_ROOT_DESCRIPTOR_CBV                = (1u << 4),
    VKD3D_RAW_VA_ROOT_DESCRIPTOR_SRV_UAV            = (1u << 5),
    VKD3D_BINDLESS_MUTABLE_TYPE                     = (1u << 6),
    VKD3D_HOIST_STATIC_TABLE_CBV                    = (1u << 7),
    VKD3D_BINDLESS_MUTABLE_TYPE_RAW_SSBO            = (1u << 8),
    VKD3D_BINDLESS_MUTABLE_EMBEDDED                 = (1u << 9),
    VKD3D_BINDLESS_MUTABLE_EMBEDDED_PACKED_METADATA = (1u << 10),
    VKD3D_FORCE_COMPUTE_ROOT_PARAMETERS_PUSH_UBO    = (1u << 11),
    VKD3D_BINDLESS_MUTABLE_TYPE_SPLIT_RAW_TYPED     = (1u << 12),
};

/* Vulkan queues */
struct vkd3d_device_queue_info
{
    unsigned int family_index[VKD3D_QUEUE_FAMILY_COUNT];
    VkQueueFamilyProperties vk_properties[VKD3D_QUEUE_FAMILY_COUNT];

    unsigned int vk_family_count;
    VkDeviceQueueCreateInfo vk_queue_create_info[VKD3D_QUEUE_FAMILY_COUNT];
};
    
struct vkd3d_vulkan_info
{
    /* EXT instance extensions */
    bool EXT_debug_utils;

    /* KHR device extensions */
    bool KHR_push_descriptor;
    bool KHR_ray_tracing_pipeline;
    bool KHR_acceleration_structure;
    bool KHR_deferred_host_operations;
    bool KHR_pipeline_library;
    bool KHR_ray_query;
    bool KHR_fragment_shading_rate;
    bool KHR_ray_tracing_maintenance1;
    bool KHR_fragment_shader_barycentric;
    bool KHR_external_memory_win32;
    bool KHR_external_semaphore_win32;
    bool KHR_present_wait;
    bool KHR_present_id;
    bool KHR_maintenance5;
    bool KHR_maintenance6;
    bool KHR_shader_maximal_reconvergence;
    bool KHR_shader_quad_control;
    bool KHR_compute_shader_derivatives;
    /* EXT device extensions */
    bool EXT_calibrated_timestamps;
    bool EXT_conditional_rendering;
    bool EXT_conservative_rasterization;
    bool EXT_custom_border_color;
    bool EXT_depth_clip_enable;
    bool EXT_device_generated_commands;
    bool EXT_image_view_min_lod;
    bool EXT_robustness2;
    bool EXT_shader_stencil_export;
    bool EXT_transform_feedback;
    bool EXT_vertex_attribute_divisor;
    bool EXT_extended_dynamic_state2;
    bool EXT_extended_dynamic_state3;
    bool EXT_external_memory_host;
    bool EXT_shader_image_atomic_int64;
    bool EXT_mesh_shader;
    bool EXT_mutable_descriptor_type; /* EXT promotion of VALVE one. */
    bool EXT_hdr_metadata;
    bool EXT_shader_module_identifier;
    bool EXT_descriptor_buffer;
    bool EXT_pipeline_library_group_handles;
    bool EXT_image_sliced_view_of_3d;
    bool EXT_graphics_pipeline_library;
    bool EXT_fragment_shader_interlock;
    bool EXT_pageable_device_local_memory;
    bool EXT_memory_priority;
    bool EXT_dynamic_rendering_unused_attachments;
    bool EXT_line_rasterization;
    bool EXT_image_compression_control;
    bool EXT_device_fault;
    bool EXT_memory_budget;
    bool EXT_device_address_binding_report;
    bool EXT_depth_bias_control;
    /* AMD device extensions */
    bool AMD_buffer_marker;
    bool AMD_device_coherent_memory;
    bool AMD_shader_core_properties;
    bool AMD_shader_core_properties2;
    /* NV device extensions */
    bool NV_optical_flow;
    bool NV_shader_sm_builtins;
    bool NVX_binary_import;
    bool NVX_image_view_handle;
    bool NV_fragment_shader_barycentric;
    bool NV_compute_shader_derivatives;
    bool NV_device_diagnostic_checkpoints;
    bool NV_device_generated_commands;
    bool NV_shader_subgroup_partitioned;
    bool NV_memory_decompression;
    bool NV_device_generated_commands_compute;
    bool NV_low_latency2;
    bool NV_raw_access_chains;
    /* VALVE extensions */
    bool VALVE_mutable_descriptor_type;
    /* MESA extensions */
    bool MESA_image_alignment_control;

    /* Optional extensions which are enabled externally as optional extensions
     * if swapchain/surface extensions are enabled. */
    bool EXT_surface_maintenance1;
    bool EXT_swapchain_maintenance1;

    unsigned int extension_count;
    const char* const* extension_names;

    bool rasterization_stream;
    unsigned int max_vertex_attrib_divisor;

    VkPhysicalDeviceLimits device_limits;
    VkPhysicalDeviceSparseProperties sparse_properties;

    std::vector<vkd3d_shader_target_extension> shader_extensions;
};



//MARK: PhyDevInfo
    struct vkd3d_physical_device_info
    {
        /* properties */
        VkPhysicalDeviceVulkan11Properties vulkan_1_1_properties;
        VkPhysicalDeviceVulkan12Properties vulkan_1_2_properties;
        VkPhysicalDeviceVulkan13Properties vulkan_1_3_properties;
        VkPhysicalDevicePushDescriptorPropertiesKHR push_descriptor_properties;
        VkPhysicalDeviceTransformFeedbackPropertiesEXT xfb_properties;
        VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT vertex_divisor_properties;
        VkPhysicalDeviceCustomBorderColorPropertiesEXT custom_border_color_properties;
        VkPhysicalDeviceShaderCorePropertiesAMD shader_core_properties;
        VkPhysicalDeviceShaderCoreProperties2AMD shader_core_properties2;
        VkPhysicalDeviceShaderSMBuiltinsPropertiesNV shader_sm_builtins_properties;
        VkPhysicalDeviceRobustness2PropertiesEXT robustness2_properties;
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT external_memory_host_properties;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_properties;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties;
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR fragment_shading_rate_properties;
        VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservative_rasterization_properties;
        VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV device_generated_commands_properties_nv;
        VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT device_generated_commands_properties_ext;
        VkPhysicalDeviceMeshShaderPropertiesEXT mesh_shader_properties;
        VkPhysicalDeviceShaderModuleIdentifierPropertiesEXT shader_module_identifier_properties;
        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties;
        VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT graphics_pipeline_library_properties;
        VkPhysicalDeviceMemoryDecompressionPropertiesNV memory_decompression_properties;
        VkPhysicalDeviceMaintenance5PropertiesKHR maintenance_5_properties;
        VkPhysicalDeviceMaintenance6PropertiesKHR maintenance_6_properties;
        VkPhysicalDeviceLineRasterizationPropertiesEXT line_rasterization_properties;
        VkPhysicalDeviceComputeShaderDerivativesPropertiesKHR compute_shader_derivatives_properties_khr;
    
        VkPhysicalDeviceProperties2KHR properties2;
    
        /* features */
        VkPhysicalDeviceVulkan11Features vulkan_1_1_features;
        VkPhysicalDeviceVulkan12Features vulkan_1_2_features;
        VkPhysicalDeviceVulkan13Features vulkan_1_3_features;
        VkPhysicalDeviceConditionalRenderingFeaturesEXT conditional_rendering_features;
        VkPhysicalDeviceDepthClipEnableFeaturesEXT depth_clip_features;
        VkPhysicalDeviceTransformFeedbackFeaturesEXT xfb_features;
        VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT vertex_divisor_features;
        VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border_color_features;
        VkPhysicalDeviceRobustness2FeaturesEXT robustness2_features;
        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extended_dynamic_state2_features;
        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state3_features;
        VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mutable_descriptor_features;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features;
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragment_shading_rate_features;
        VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV barycentric_features_nv;
        VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentric_features_khr;
        VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features;
        VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR compute_shader_derivatives_features_khr;
        VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT shader_image_atomic_int64_features;
        VkPhysicalDeviceImageViewMinLodFeaturesEXT image_view_min_lod_features;
        VkPhysicalDeviceCoherentMemoryFeaturesAMD device_coherent_memory_features_amd;
        VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR ray_tracing_maintenance1_features;
        VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV device_generated_commands_features_nv;
        VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT device_generated_commands_features_ext;
        VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features;
        VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT shader_module_identifier_features;
        VkPhysicalDevicePresentIdFeaturesKHR present_id_features;
        VkPhysicalDevicePresentWaitFeaturesKHR present_wait_features;
        VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features;
        VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT pipeline_library_group_handles_features;
        VkPhysicalDeviceImageSlicedViewOf3DFeaturesEXT image_sliced_view_of_3d_features;
        VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphics_pipeline_library_features;
        VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragment_shader_interlock_features;
        VkPhysicalDeviceMemoryPriorityFeaturesEXT memory_priority_features;
        VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageable_device_memory_features;
        VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamic_rendering_unused_attachments_features;
        VkPhysicalDeviceMemoryDecompressionFeaturesNV memory_decompression_features;
        VkPhysicalDeviceDeviceGeneratedCommandsComputeFeaturesNV device_generated_commands_compute_features_nv;
        VkPhysicalDeviceMaintenance5FeaturesKHR maintenance_5_features;
        VkPhysicalDeviceMaintenance6FeaturesKHR maintenance_6_features;
        VkPhysicalDeviceLineRasterizationFeaturesEXT line_rasterization_features;
        VkPhysicalDeviceImageCompressionControlFeaturesEXT image_compression_control_features;
        VkPhysicalDeviceFaultFeaturesEXT fault_features;
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance1_features;
        VkPhysicalDeviceShaderMaximalReconvergenceFeaturesKHR shader_maximal_reconvergence_features;
        VkPhysicalDeviceShaderQuadControlFeaturesKHR shader_quad_control_features;
        VkPhysicalDeviceRawAccessChainsFeaturesNV raw_access_chains_nv;
        VkPhysicalDeviceAddressBindingReportFeaturesEXT address_binding_report_features;
        VkPhysicalDeviceImageAlignmentControlFeaturesMESA image_alignment_control_features;
        VkPhysicalDeviceImageAlignmentControlPropertiesMESA image_alignment_control_properties;
        VkPhysicalDeviceDepthBiasControlFeaturesEXT depth_bias_control_features;
        VkPhysicalDeviceOpticalFlowFeaturesNV optical_flow_nv_features;
    
        VkPhysicalDeviceFeatures2 features2;
    
        /* others, for extensions that have no feature bits */
        uint32_t time_domains;  /* vkd3d_time_domain_flag */
    
        bool additional_shading_rates_supported; /* d3d12 additional fragment shading rates cap */
    };


struct d3d12_bind_point_layout
{
    VkPipelineLayout vk_pipeline_layout;
    VkShaderStageFlags vk_push_stages;
    unsigned int flags; /* vkd3d_root_signature_flag */
    uint32_t num_set_layouts;
    VkPushConstantRange push_constant_range;
};

//MARK: Device
struct d3d12_device
{

    VkDevice vk_device;
    uint32_t api_version;
    VkPhysicalDevice vk_physical_device;

    VkPhysicalDeviceMemoryProperties memory_properties;

    struct vkd3d_vulkan_info vk_info;
    struct vkd3d_physical_device_info device_info;

    struct vkd3d_queue_family_info *queue_families[VKD3D_QUEUE_FAMILY_COUNT];
    uint32_t queue_family_indices[VKD3D_QUEUE_FAMILY_COUNT];
    uint32_t queue_family_count;
    uint32_t unique_queue_mask;

    struct vkd3d_instance *vkd3d_instance;

    struct vkd3d_private_store private_store;
    struct d3d_destruction_notifier destruction_notifier;
    struct d3d12_caps d3d12_caps;

    struct vkd3d_memory_transfer_queue memory_transfers;
    struct vkd3d_memory_allocator memory_allocator;

    struct vkd3d_queue *internal_sparse_queue;
    VkSemaphore sparse_init_timeline;
    uint64_t sparse_init_timeline_value;

    struct d3d12_device_scratch_pool scratch_pools[VKD3D_SCRATCH_POOL_KIND_COUNT];

    struct vkd3d_query_pool query_pools[VKD3D_VIRTUAL_QUERY_POOL_COUNT];
    size_t query_pool_count;

    struct vkd3d_cached_command_allocator cached_command_allocators[VKD3D_CACHED_COMMAND_ALLOCATOR_COUNT];
    size_t cached_command_allocator_count;

    uint32_t *descriptor_heap_gpu_vas;
    size_t descriptor_heap_gpu_va_count;
    size_t descriptor_heap_gpu_va_size;
    uint32_t descriptor_heap_gpu_next;

    HRESULT removed_reason;

    const struct vkd3d_format *formats;
    const struct vkd3d_format *depth_stencil_formats;
    unsigned int format_compatibility_list_count;
    const struct vkd3d_format_compatibility_list *format_compatibility_lists;
    struct vkd3d_bindless_state bindless_state;
    struct vkd3d_queue_timeline_trace queue_timeline_trace;
    struct vkd3d_memory_info memory_info;
    struct vkd3d_meta_ops meta_ops;
    struct vkd3d_view_map sampler_map;
    struct vkd3d_sampler_state sampler_state;
    struct vkd3d_shader_debug_ring debug_ring;
    struct vkd3d_pipeline_library_disk_cache disk_cache;
    struct vkd3d_global_descriptor_buffer global_descriptor_buffer;
    struct vkd3d_address_binding_tracker address_binding_tracker;
    rwlock_t vertex_input_lock;
    struct hash_map vertex_input_pipelines;
    rwlock_t fragment_output_lock;
    struct hash_map fragment_output_pipelines;
#ifdef VKD3D_ENABLE_BREADCRUMBS
    struct vkd3d_breadcrumb_tracer breadcrumb_tracer;
#endif
#ifdef VKD3D_ENABLE_DESCRIPTOR_QA
    struct vkd3d_descriptor_qa_global_info *descriptor_qa_global_info;
#endif
    uint64_t shader_interface_key;
    uint32_t device_has_dgc_templates;

    struct vkd3d_device_swapchain_info swapchain_info;
    struct vkd3d_device_frame_markers frame_markers;

};




