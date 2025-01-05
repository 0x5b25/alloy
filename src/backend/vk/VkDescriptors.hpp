#pragma once

#include <volk.h>
#include <vector>

struct vkd3d_descriptor_binding
{
    uint8_t set;
    uint8_t binding;
};

struct vkd3d_bindless_state
{
    uint32_t flags; /* vkd3d_bindless_flags */

    /* For descriptor buffers, pre-baked array passed directly to vkCmdBindDescriptorBuffersEXT. */
    uint32_t vk_descriptor_buffer_indices[VKD3D_MAX_BINDLESS_DESCRIPTOR_SETS];
    struct vkd3d_bindless_set_info set_info[VKD3D_MAX_BINDLESS_DESCRIPTOR_SETS];
    unsigned int set_count;
    unsigned int cbv_srv_uav_count;

    /* NULL descriptor payloads are not necessarily all zero.
     * Access the array with vkd3d_bindless_state_get_null_descriptor_payload(). */
    DECLSPEC_ALIGN(16) uint8_t null_descriptor_payloads[6][VKD3D_MAX_DESCRIPTOR_SIZE];
    size_t descriptor_buffer_cbv_srv_uav_size;
    size_t descriptor_buffer_sampler_size;
    unsigned int descriptor_buffer_cbv_srv_uav_size_log2;
    unsigned int descriptor_buffer_sampler_size_log2;
    unsigned int descriptor_buffer_packed_raw_buffer_offset;
    unsigned int descriptor_buffer_packed_metadata_offset;


    
    RESULT init(struct d3d12_device *device);
    void cleanup(struct d3d12_device *device);
    bool find_binding(uint32_t flags, struct vkd3d_shader_descriptor_binding *binding) const;
    struct vkd3d_descriptor_binding find_set(uint32_t flags) const;
    uint32_t find_set_info_index( uint32_t flags) const;
};

//MARK: Root signature
struct d3d12_root_signature
{
    /* Compatibility for exact match. For PSO blob validation. */
    vkd3d_shader_hash_t pso_compatibility_hash;
    /* Compatiblity for ABI in RTPSOs. Match if the VkPipelineLayouts are equivalent. */
    vkd3d_shader_hash_t layout_compatibility_hash;

    struct d3d12_bind_point_layout graphics, mesh, compute, raygen;
    VkDescriptorSetLayout vk_sampler_descriptor_layout;
    VkDescriptorSetLayout vk_root_descriptor_layout;

    VkDescriptorPool vk_sampler_pool;
    VkDescriptorSet vk_sampler_set;

    struct vkd3d_shader_root_parameter *parameters;
    unsigned int parameter_count;

    uint32_t sampler_descriptor_set;
    uint32_t root_descriptor_set;

    uint64_t descriptor_table_mask;
    uint64_t root_constant_mask;
    uint64_t root_descriptor_raw_va_mask;
    uint64_t root_descriptor_push_mask;

    D3D12_ROOT_SIGNATURE_FLAGS d3d12_flags;

    unsigned int binding_count;
    struct vkd3d_shader_resource_binding *bindings;

    unsigned int root_constant_count;
    struct vkd3d_shader_push_constant_buffer *root_constants;

    struct vkd3d_shader_descriptor_binding push_constant_ubo_binding;
    struct vkd3d_shader_descriptor_binding raw_va_aux_buffer_binding;
    struct vkd3d_shader_descriptor_binding offset_buffer_binding;
#ifdef VKD3D_ENABLE_DESCRIPTOR_QA
    struct vkd3d_shader_descriptor_binding descriptor_qa_payload_binding;
    struct vkd3d_shader_descriptor_binding descriptor_qa_control_binding;
#endif

    std::vector<VkDescriptorSetLayout> set_layouts;

    uint32_t descriptor_table_offset;
    uint32_t descriptor_table_count;

    unsigned int static_sampler_count;
    VkSamplerCreateInfo *static_samplers_desc;//D3D12_STATIC_SAMPLER_DESC1 *static_samplers_desc;
    VkSampler *static_samplers;

    //struct vkd3d_descriptor_hoist_info hoist_info;

    struct d3d12_device *device;

    //struct vkd3d_private_store private_store;
    //struct d3d_destruction_notifier destruction_notifier;
};

