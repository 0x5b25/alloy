#pragma once

#include <volk.h>
//MARK: Pipelines

enum vkd3d_dynamic_state_flag
{
    VKD3D_DYNAMIC_STATE_VIEWPORT              = (1 << 0),
    VKD3D_DYNAMIC_STATE_SCISSOR               = (1 << 1),
    VKD3D_DYNAMIC_STATE_BLEND_CONSTANTS       = (1 << 2),
    VKD3D_DYNAMIC_STATE_STENCIL_REFERENCE     = (1 << 3),
    VKD3D_DYNAMIC_STATE_DEPTH_BOUNDS          = (1 << 4),
    VKD3D_DYNAMIC_STATE_TOPOLOGY              = (1 << 5),
    VKD3D_DYNAMIC_STATE_VERTEX_BUFFER_STRIDE  = (1 << 6),
    VKD3D_DYNAMIC_STATE_FRAGMENT_SHADING_RATE = (1 << 7),
    VKD3D_DYNAMIC_STATE_PRIMITIVE_RESTART     = (1 << 8),
    VKD3D_DYNAMIC_STATE_PATCH_CONTROL_POINTS  = (1 << 9),
    VKD3D_DYNAMIC_STATE_DEPTH_WRITE_ENABLE    = (1 << 10),
    VKD3D_DYNAMIC_STATE_STENCIL_WRITE_MASK    = (1 << 11),
    VKD3D_DYNAMIC_STATE_DEPTH_BIAS            = (1 << 12),
    VKD3D_DYNAMIC_STATE_RASTERIZATION_SAMPLES = (1 << 13),
};


struct vkd3d_pipeline_key
{
    D3D12_PRIMITIVE_TOPOLOGY topology;
    VkFormat dsv_format;
    VkSampleCountFlagBits rasterization_samples;

    bool dynamic_topology;
};

struct vkd3d_compiled_pipeline
{
    struct list entry;
    struct vkd3d_pipeline_key key;
    VkPipeline vk_pipeline;
    uint32_t dynamic_state_flags;
};

struct vkd3d_vertex_input_pipeline_desc
{
    VkVertexInputBindingDivisorDescriptionEXT vi_divisors[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    VkPipelineVertexInputDivisorStateCreateInfoEXT vi_divisor_info;

    VkVertexInputAttributeDescription vi_attributes[D3D12_VS_INPUT_REGISTER_COUNT];
    VkVertexInputBindingDescription vi_bindings[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    VkPipelineVertexInputStateCreateInfo vi_info;

    VkPipelineInputAssemblyStateCreateInfo ia_info;

    std::vector<VkDynamicState> dy_states;
    VkPipelineDynamicStateCreateInfo dy_info;
};

struct vkd3d_vertex_input_pipeline
{
    struct hash_map_entry entry;
    struct vkd3d_vertex_input_pipeline_desc desc;
    VkPipeline vk_pipeline;
};


struct vkd3d_fragment_output_pipeline_desc
{
    VkPipelineColorBlendAttachmentState cb_attachments[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
    VkPipelineColorBlendStateCreateInfo cb_info;

    VkSampleMask ms_sample_mask;
    VkPipelineMultisampleStateCreateInfo ms_info;

    VkFormat rt_formats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
    VkPipelineRenderingCreateInfo rt_info;

    VkDynamicState dy_states[VKD3D_MAX_FRAGMENT_OUTPUT_DYNAMIC_STATES];
    VkPipelineDynamicStateCreateInfo dy_info;
};

struct vkd3d_fragment_output_pipeline
{
    struct hash_map_entry entry;
    struct vkd3d_fragment_output_pipeline_desc desc;
    VkPipeline vk_pipeline;
};

struct d3d12_graphics_pipeline_state_cached_desc
{
    /* Information needed to compile to SPIR-V. */
    unsigned int ps_output_swizzle[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
    struct vkd3d_shader_parameter ps_shader_parameters[1];
    bool is_dual_source_blending;
    VkShaderStageFlagBits xfb_stage;
    struct vkd3d_shader_transform_feedback_info *xfb_info;
    struct vkd3d_shader_stage_io_map stage_io_map_ms_ps;

    D3D12_SHADER_BYTECODE bytecode[VKD3D_MAX_SHADER_STAGES];
    VkShaderStageFlagBits bytecode_stages[VKD3D_MAX_SHADER_STAGES];
    uint32_t bytecode_duped_mask;
};

struct d3d12_graphics_pipeline_state
{
    struct vkd3d_shader_debug_ring_spec_info spec_info[VKD3D_MAX_SHADER_STAGES];
    VkPipelineShaderStageCreateInfo stages[VKD3D_MAX_SHADER_STAGES];
    struct vkd3d_shader_code code[VKD3D_MAX_SHADER_STAGES];
    struct vkd3d_shader_code_debug code_debug[VKD3D_MAX_SHADER_STAGES];
    VkShaderStageFlags stage_flags;
    VkShaderModuleIdentifierEXT identifiers[VKD3D_MAX_SHADER_STAGES];
    VkPipelineShaderStageModuleIdentifierCreateInfoEXT identifier_create_infos[VKD3D_MAX_SHADER_STAGES];
    size_t stage_count;

    struct d3d12_graphics_pipeline_state_cached_desc cached_desc;

    VkVertexInputAttributeDescription attributes[D3D12_VS_INPUT_REGISTER_COUNT];
    VkVertexInputRate input_rates[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    VkVertexInputBindingDivisorDescriptionEXT instance_divisors[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    VkVertexInputBindingDescription attribute_bindings[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    uint32_t minimum_vertex_buffer_dynamic_stride[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    uint32_t vertex_buffer_stride_align_mask[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    size_t instance_divisor_count;
    size_t attribute_binding_count;
    size_t attribute_count;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type;
    uint32_t vertex_buffer_mask;

    VkPipelineColorBlendAttachmentState blend_attachments[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
    unsigned int rt_count;
    unsigned int null_attachment_mask;
    unsigned int rtv_active_mask;
    unsigned int patch_vertex_count;
    const struct vkd3d_format *dsv_format;
    VkFormat rtv_formats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
    uint32_t dsv_plane_optimal_mask;

    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE index_buffer_strip_cut_value;
    VkPipelineRasterizationStateCreateInfo rs_desc;
    VkPipelineMultisampleStateCreateInfo ms_desc;
    VkPipelineDepthStencilStateCreateInfo ds_desc;
    VkPipelineColorBlendStateCreateInfo blend_desc;

    VkSampleMask sample_mask;
    VkPipelineRasterizationConservativeStateCreateInfoEXT rs_conservative_info;
    VkPipelineRasterizationDepthClipStateCreateInfoEXT rs_depth_clip_info;
    VkPipelineRasterizationStateStreamCreateInfoEXT rs_stream_info;
    VkPipelineRasterizationLineStateCreateInfoEXT rs_line_info;
    VkDepthBiasRepresentationInfoEXT rs_depth_bias_info;

    /* vkd3d_dynamic_state_flag */
    uint32_t explicit_dynamic_states;
    uint32_t pipeline_dynamic_states;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkPipeline library;
    VkGraphicsPipelineLibraryFlagsEXT library_flags;
    VkPipelineCreateFlags library_create_flags;
    struct list compiled_fallback_pipelines;

    bool xfb_enabled;
    bool disable_optimization;
};

static inline unsigned int dsv_attachment_mask(const struct d3d12_graphics_pipeline_state *graphics)
{
    return 1u << graphics->rt_count;
}

struct d3d12_compute_pipeline_state
{
    VkPipeline vk_pipeline;
    struct vkd3d_shader_code code;
    struct vkd3d_shader_code_debug code_debug;
    VkShaderModuleIdentifierEXT identifier;
    VkPipelineShaderStageModuleIdentifierCreateInfoEXT identifier_create_info;
};

/* To be able to load a pipeline from cache, this information must match exactly,
 * otherwise, we must regard the PSO as incompatible (which is invalid usage and must be validated). */
struct vkd3d_pipeline_cache_compatibility
{
    uint64_t state_desc_compat_hash;
    uint64_t root_signature_compat_hash;
    uint64_t dxbc_blob_hashes[VKD3D_MAX_SHADER_STAGES];
};

struct d3d12_pipeline_state
{
    union
    {
        struct d3d12_graphics_pipeline_state graphics;
        struct d3d12_compute_pipeline_state compute;
    };

    enum vkd3d_pipeline_type pipeline_type;
    VkPipelineCache vk_pso_cache;
    //rwlock_t lock;

    struct vkd3d_pipeline_cache_compatibility pipeline_cache_compat;
    struct d3d12_root_signature *root_signature;
    struct d3d12_device *device;
    bool root_signature_compat_hash_is_dxbc_derived;
    bool pso_is_loaded_from_cached_blob;
    bool pso_is_fully_dynamic;

    //struct vkd3d_private_store private_store;
    //struct d3d_destruction_notifier destruction_notifier;
};


struct d3d12_pipeline_state_desc
{
    ID3D12RootSignature *root_signature;
    D3D12_SHADER_BYTECODE vs;
    D3D12_SHADER_BYTECODE ps;
    D3D12_SHADER_BYTECODE ds;
    D3D12_SHADER_BYTECODE hs;
    D3D12_SHADER_BYTECODE gs;
    D3D12_SHADER_BYTECODE cs;
    D3D12_SHADER_BYTECODE as;
    D3D12_SHADER_BYTECODE ms;
    D3D12_STREAM_OUTPUT_DESC stream_output;
    D3D12_BLEND_DESC blend_state;
    uint32_t sample_mask;
    D3D12_RASTERIZER_DESC2 rasterizer_state;
    D3D12_DEPTH_STENCIL_DESC2 depth_stencil_state;
    D3D12_INPUT_LAYOUT_DESC input_layout;
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE strip_cut_value;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type;
    D3D12_RT_FORMAT_ARRAY rtv_formats;
    DXGI_FORMAT dsv_format;
    DXGI_SAMPLE_DESC sample_desc;
    D3D12_VIEW_INSTANCING_DESC view_instancing_desc;
    uint32_t node_mask;
    struct d3d12_cached_pipeline_state cached_pso;
    D3D12_PIPELINE_STATE_FLAGS flags;
};

HRESULT d3d12_pipeline_state_create(struct d3d12_device *device, VkPipelineBindPoint bind_point,
        const struct d3d12_pipeline_state_desc *desc, struct d3d12_pipeline_state **state);

void d3d12_pipeline_state_init_shader_interface(struct d3d12_pipeline_state *state,
        struct d3d12_device *device,
        VkShaderStageFlagBits stage,
        struct vkd3d_shader_interface_info *shader_interface);

void d3d12_pipeline_state_init_compile_arguments(struct d3d12_pipeline_state *state,
        struct d3d12_device *device, VkShaderStageFlagBits stage,
        struct vkd3d_shader_compile_arguments *compile_arguments);
