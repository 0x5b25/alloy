#include "VkDescriptors.hpp"

#include "VkPhyDev.hpp"
#include "VkCommon.hpp"

static uint32_t vkd3d_bindless_build_mutable_type_list(VkDescriptorType *list, uint32_t bindless_flags, uint32_t set_flags)
{
    uint32_t count = 0;

    if (set_flags & VKD3D_BINDLESS_SET_MUTABLE_RAW)
    {
        list[count++] = (bindless_flags & VKD3D_BINDLESS_CBV_AS_SSBO) ?
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        if ((bindless_flags & VKD3D_BINDLESS_RAW_SSBO) && !(bindless_flags & VKD3D_BINDLESS_CBV_AS_SSBO))
            list[count++] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }

    if (set_flags & VKD3D_BINDLESS_SET_MUTABLE_TYPED)
    {
        list[count++] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        list[count++] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        list[count++] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        list[count++] = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    }

    return count;
}

static bool vkd3d_bindless_supports_mutable_type(struct d3d12_device *device, uint32_t bindless_flags)
{
    VkDescriptorType descriptor_types[VKD3D_MAX_MUTABLE_DESCRIPTOR_TYPES];
    const struct vkd3d_vk_device_procs *vk_procs = &device->vk_procs;
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags;
    VkMutableDescriptorTypeCreateInfoEXT mutable_info;
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkMutableDescriptorTypeListVALVE mutable_list;
    VkDescriptorSetLayoutSupport supported;
    VkDescriptorBindingFlags binding_flag;
    VkDescriptorSetLayoutBinding binding;

    binding_flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    if (!d3d12_device_uses_descriptor_buffers(device))
    {
        binding_flag |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    }

    if (!device->device_info.mutable_descriptor_features.mutableDescriptorType)
        return false;

    mutable_info.sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT;
    mutable_info.pNext = NULL;
    mutable_info.pMutableDescriptorTypeLists = &mutable_list;
    mutable_info.mutableDescriptorTypeListCount = 1;

    mutable_list.descriptorTypeCount = vkd3d_bindless_build_mutable_type_list(descriptor_types, bindless_flags,
            VKD3D_BINDLESS_SET_MUTABLE_RAW | VKD3D_BINDLESS_SET_MUTABLE_TYPED);
    mutable_list.pDescriptorTypes = descriptor_types;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
    binding.descriptorCount = d3d12_max_descriptor_count_from_heap_type(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    binding.pImmutableSamplers = NULL;
    binding.stageFlags = VK_SHADER_STAGE_ALL;

    binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags.pNext = &mutable_info;
    binding_flags.bindingCount = 1;
    binding_flags.pBindingFlags = &binding_flag;

    set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext = &binding_flags;

    if (d3d12_device_uses_descriptor_buffers(device))
        set_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    else
        set_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    set_layout_info.bindingCount = 1;
    set_layout_info.pBindings = &binding;

    supported.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT;
    supported.pNext = NULL;
    vkGetDescriptorSetLayoutSupport(device->vk_device, &set_layout_info, &supported);
    if (!supported.supported)
        return false;

    if (!d3d12_device_uses_descriptor_buffers(device))
    {
        set_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_EXT;
        binding_flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        vkGetDescriptorSetLayoutSupport(device->vk_device, &set_layout_info, &supported);
    }

    return supported.supported == VK_TRUE;
}

static uint32_t vkd3d_bindless_state_get_bindless_flags(struct d3d12_device *device)
{
    const struct vkd3d_physical_device_info *device_info = &device->device_info;
    uint32_t flags = 0;

    if (!d3d12_device_uses_descriptor_buffers(device))
    {
        if (device_info->vulkan_1_2_properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers < VKD3D_MIN_VIEW_DESCRIPTOR_COUNT ||
                !device_info->vulkan_1_2_features.descriptorBindingUniformBufferUpdateAfterBind ||
                !device_info->vulkan_1_2_features.shaderUniformBufferArrayNonUniformIndexing)
            flags |= VKD3D_BINDLESS_CBV_AS_SSBO;
    }

    /* 16 is the cutoff due to requirements on ByteAddressBuffer.
     * We need tight 16 byte robustness on those and trying to emulate that with offset buffers
     * is too much of an ordeal. */
    if (device_info->properties2.properties.limits.minStorageBufferOffsetAlignment <= 16)
    {
        flags |= VKD3D_BINDLESS_RAW_SSBO;

        /* Descriptor buffers do not support SINGLE_SET layout.
         * We only enable descriptor buffers if we have verified that MUTABLE_SINGLE_SET hack is not required. */
        if (!d3d12_device_uses_descriptor_buffers(device))
        {
            /* Intel GPUs have smol descriptor heaps and only way we can fit a D3D12 heap is with
             * single set mutable. */
            if ((vkd3d_config_flags & VKD3D_CONFIG_FLAG_MUTABLE_SINGLE_SET) ||
                    device_info->properties2.properties.vendorID == VKD3D_VENDOR_ID_INTEL)
            {
                INFO("Enabling single descriptor set path for MUTABLE.\n");
                flags |= VKD3D_BINDLESS_MUTABLE_TYPE_RAW_SSBO;
            }
        }

        if (device_info->properties2.properties.limits.minStorageBufferOffsetAlignment > 4)
            flags |= VKD3D_SSBO_OFFSET_BUFFER;
    }

    /* Always use a typed offset buffer. Otherwise, we risk ending up with unbounded size on view maps.
     * Fortunately, we can place descriptors directly if we have descriptor buffers, so this is not required. */
    if (!d3d12_device_uses_descriptor_buffers(device))
        flags |= VKD3D_TYPED_OFFSET_BUFFER;

    /* We must use root SRV and UAV due to alignment requirements for 16-bit storage,
     * but root CBV is more lax. */
    flags |= VKD3D_RAW_VA_ROOT_DESCRIPTOR_SRV_UAV;
    /* CBV's really require push descriptors on NVIDIA to get maximum performance.
     * The difference in performance is profound (~15% in some cases).
     * On ACO, BDA with NonWritable can be promoted directly to scalar loads,
     * which is great. */
    if ((vkd3d_config_flags & VKD3D_CONFIG_FLAG_FORCE_RAW_VA_CBV) ||
            device_info->properties2.properties.vendorID != VKD3D_VENDOR_ID_NVIDIA)
        flags |= VKD3D_RAW_VA_ROOT_DESCRIPTOR_CBV;

    if (device_info->properties2.properties.vendorID == VKD3D_VENDOR_ID_NVIDIA &&
            !(flags & VKD3D_RAW_VA_ROOT_DESCRIPTOR_CBV))
    {
        /* On NVIDIA, it's preferable to hoist CBVs to push descriptors if we can.
         * Hoisting is only safe with push descriptors since we need to consider
         * robustness as well for STATIC_KEEPING_BUFFER_BOUNDS_CHECKS. */
        flags |= VKD3D_HOIST_STATIC_TABLE_CBV;
    }

    if ((vkd3d_config_flags & VKD3D_CONFIG_FLAG_REQUIRES_COMPUTE_INDIRECT_TEMPLATES) &&
            !device->device_info.device_generated_commands_compute_features_nv.deviceGeneratedCompute &&
            !device->device_info.device_generated_commands_features_ext.deviceGeneratedCommands)
    {
        INFO("Forcing push UBO path for compute root parameters.\n");
        flags |= VKD3D_FORCE_COMPUTE_ROOT_PARAMETERS_PUSH_UBO;
    }

    if (device->device_info.device_generated_commands_features_ext.deviceGeneratedCommands)
    {
        INFO("Enabling fast paths for advanced ExecuteIndirect() graphics and compute (EXT_dgc).\n");
    }
    else
    {
        if (device->device_info.device_generated_commands_compute_features_nv.deviceGeneratedCompute)
            INFO("Enabling fast paths for advanced ExecuteIndirect() compute (NV_dgc).\n");
        if (device->device_info.device_generated_commands_features_nv.deviceGeneratedCommands)
            INFO("Enabling fast paths for advanced ExecuteIndirect() graphics (NV_dgc).\n");
    }

    if (vkd3d_bindless_supports_mutable_type(device, flags))
    {
        INFO("Device supports VK_%s_mutable_descriptor_type.\n",
                device->vk_info.EXT_mutable_descriptor_type ? "EXT" : "VALVE");
        flags |= VKD3D_BINDLESS_MUTABLE_TYPE;

        /* If we can, opt in to extreme speed mode. */
        if (d3d12_device_uses_descriptor_buffers(device) &&
                vkd3d_bindless_supports_embedded_mutable_type(device, flags))
        {
            flags |= VKD3D_BINDLESS_MUTABLE_EMBEDDED;
            INFO("Device supports ultra-fast path for descriptor copies.\n");

            if (vkd3d_bindless_supports_embedded_packed_metadata(device))
            {
                flags |= VKD3D_BINDLESS_MUTABLE_EMBEDDED_PACKED_METADATA;
                INFO("Device supports packed metadata path for descriptor copies.\n");
            }
        }
    }
    else
    {
        INFO("Device does not support VK_EXT_mutable_descriptor_type (or VALVE).\n");
        flags &= ~VKD3D_BINDLESS_MUTABLE_TYPE_RAW_SSBO;
    }

    /* Shorthand formulation to make future checks nicer. */
    if ((flags & VKD3D_BINDLESS_MUTABLE_TYPE) &&
            (flags & VKD3D_BINDLESS_RAW_SSBO) &&
            !(flags & VKD3D_BINDLESS_MUTABLE_TYPE_RAW_SSBO))
    {
        flags |= VKD3D_BINDLESS_MUTABLE_TYPE_SPLIT_RAW_TYPED;
    }

    return flags;
}

RESULT vkd3d_bindless_state::init(struct d3d12_device *device) {
    const struct vkd3d_physical_device_info *device_info = &device->device_info;
    uint32_t extra_bindings = 0;
    HRESULT hr = E_FAIL;

    memset(bindless_state, 0, sizeof(*bindless_state));
    bindless_state->flags = vkd3d_bindless_state_get_bindless_flags(device);

    if (!device_info->vulkan_1_2_features.descriptorIndexing ||
            /* Some extra features not covered by descriptorIndexing meta-feature. */
            !device_info->vulkan_1_2_features.shaderStorageTexelBufferArrayNonUniformIndexing ||
            !device_info->vulkan_1_2_features.shaderStorageImageArrayNonUniformIndexing ||
            !device_info->vulkan_1_2_features.descriptorBindingVariableDescriptorCount)
    {
        ERR("Insufficient descriptor indexing support.\n");
        goto fail;
    }

    if (!d3d12_device_uses_descriptor_buffers(device))
    {
        /* UBO is optional. We can fall back to SSBO if required. */
        if (device_info->vulkan_1_2_properties.maxPerStageDescriptorUpdateAfterBindSampledImages < VKD3D_MIN_VIEW_DESCRIPTOR_COUNT ||
                device_info->vulkan_1_2_properties.maxPerStageDescriptorUpdateAfterBindStorageImages < VKD3D_MIN_VIEW_DESCRIPTOR_COUNT ||
                device_info->vulkan_1_2_properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers < VKD3D_MIN_VIEW_DESCRIPTOR_COUNT)
        {
            ERR("Insufficient descriptor indexing support.\n");
            goto fail;
        }
    }

    extra_bindings |= VKD3D_BINDLESS_SET_EXTRA_RAW_VA_AUX_BUFFER;
    if (bindless_state->flags & (VKD3D_SSBO_OFFSET_BUFFER | VKD3D_TYPED_OFFSET_BUFFER))
        extra_bindings |= VKD3D_BINDLESS_SET_EXTRA_OFFSET_BUFFER;

    if (vkd3d_descriptor_debug_active_descriptor_qa_checks())
    {
        extra_bindings |= VKD3D_BINDLESS_SET_EXTRA_FEEDBACK_PAYLOAD_INFO_BUFFER |
                VKD3D_BINDLESS_SET_EXTRA_FEEDBACK_CONTROL_INFO_BUFFER;
    }

    if (FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
            VKD3D_BINDLESS_SET_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER)))
        goto fail;

    if (bindless_state->flags & VKD3D_BINDLESS_MUTABLE_TYPE)
    {
        bool uses_raw_typed_split = !!(bindless_state->flags & VKD3D_BINDLESS_MUTABLE_TYPE_SPLIT_RAW_TYPED);
        uint32_t flags;

        flags = VKD3D_BINDLESS_SET_UAV | VKD3D_BINDLESS_SET_SRV |
                VKD3D_BINDLESS_SET_BUFFER | VKD3D_BINDLESS_SET_IMAGE |
                VKD3D_BINDLESS_SET_MUTABLE_TYPED | VKD3D_BINDLESS_SET_MUTABLE |
                extra_bindings;

        if (!uses_raw_typed_split)
        {
            flags |= VKD3D_BINDLESS_SET_CBV | VKD3D_BINDLESS_SET_MUTABLE_RAW;
            if (bindless_state->flags & VKD3D_BINDLESS_RAW_SSBO)
                flags |= VKD3D_BINDLESS_SET_RAW_SSBO;
        }

        /* Ensure that the descriptor size matches the other set, since we'll be overlaying them
         * on the same memory. */
        if (bindless_state->flags & VKD3D_BINDLESS_MUTABLE_EMBEDDED)
            flags |= VKD3D_BINDLESS_SET_MUTABLE_RAW;

        /* If we can, prefer to use one universal descriptor type which works for any descriptor.
         * The exception is SSBOs since we need to workaround buggy applications which create typed buffers,
         * but assume they can be read as untyped buffers. Move CBVs to the SSBO set as well if we go that route,
         * since it works around similar app bugs.
         * If we opt-in to it, we can move everything into the mutable set. */
        if (FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device, flags,
                VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)))
            goto fail;

        /* We never use CBV in second set unless SSBO does as well. */
        if (uses_raw_typed_split)
        {
            bool use_mutable = (bindless_state->flags & VKD3D_BINDLESS_MUTABLE_EMBEDDED) ||
                    !(bindless_state->flags & VKD3D_BINDLESS_CBV_AS_SSBO);

            flags = VKD3D_BINDLESS_SET_UAV |
                    VKD3D_BINDLESS_SET_SRV |
                    VKD3D_BINDLESS_SET_RAW_SSBO |
                    VKD3D_BINDLESS_SET_CBV;

            if (use_mutable)
                flags |= VKD3D_BINDLESS_SET_MUTABLE | VKD3D_BINDLESS_SET_MUTABLE_RAW;

            /* Ensure that the descriptor size matches the other set, since we'll be overlaying them
             * on the same memory. */
            if (bindless_state->flags & VKD3D_BINDLESS_MUTABLE_EMBEDDED)
                flags |= VKD3D_BINDLESS_SET_MUTABLE_TYPED;

            if (FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
                    flags, use_mutable ? VK_DESCRIPTOR_TYPE_MUTABLE_EXT : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)))
                goto fail;
        }
    }
    else
    {
        if (FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
                VKD3D_BINDLESS_SET_CBV | extra_bindings,
                vkd3d_bindless_state_get_cbv_descriptor_type(bindless_state),
                vkd3d_bindless_state_get_cbv_descriptor_type(bindless_state))))
            goto fail;

        if (FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
                VKD3D_BINDLESS_SET_SRV | VKD3D_BINDLESS_SET_BUFFER,
                VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)) ||
            FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
                VKD3D_BINDLESS_SET_SRV | VKD3D_BINDLESS_SET_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)))
            goto fail;

        if (FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
                VKD3D_BINDLESS_SET_UAV | VKD3D_BINDLESS_SET_BUFFER,
                VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)) ||
            FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
                VKD3D_BINDLESS_SET_UAV | VKD3D_BINDLESS_SET_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)))
            goto fail;

        if (bindless_state->flags & VKD3D_BINDLESS_RAW_SSBO)
        {
            if (FAILED(hr = vkd3d_bindless_state_add_binding(bindless_state, device,
                    VKD3D_BINDLESS_SET_UAV | VKD3D_BINDLESS_SET_SRV | VKD3D_BINDLESS_SET_RAW_SSBO,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)))
                goto fail;
        }
    }

    if (d3d12_device_uses_descriptor_buffers(device))
    {
        vkd3d_bindless_state_init_null_descriptor_payloads(bindless_state, device);
        /* cbv_srv_uav_size is computed while setting up null payload. */
        bindless_state->descriptor_buffer_sampler_size =
                device->device_info.descriptor_buffer_properties.samplerDescriptorSize;
        bindless_state->descriptor_buffer_cbv_srv_uav_size_log2 =
                vkd3d_log2i(bindless_state->descriptor_buffer_cbv_srv_uav_size);
        bindless_state->descriptor_buffer_sampler_size_log2 =
                vkd3d_log2i(bindless_state->descriptor_buffer_sampler_size);
        bindless_state->descriptor_buffer_packed_raw_buffer_offset =
                vkd3d_bindless_embedded_mutable_raw_buffer_offset(device);
        bindless_state->descriptor_buffer_packed_metadata_offset =
                vkd3d_bindless_embedded_mutable_packed_metadata_offset(device);
    }

    return S_OK;

fail:
    vkd3d_bindless_state_cleanup(bindless_state, device);
    return hr;
}
void vkd3d_bindless_state::cleanup(struct d3d12_device *device) {

}
bool vkd3d_bindless_state::find_binding(uint32_t flags, struct vkd3d_shader_descriptor_binding *binding) const {

}
struct vkd3d_bindless_state::vkd3d_descriptor_binding find_set(uint32_t flags) const {

}
uint32_t vkd3d_bindless_state::find_set_info_index( uint32_t flags) const {

}






static HRESULT d3d12_root_signature_init_local(struct d3d12_root_signature *root_signature,
        struct d3d12_device *device, const D3D12_ROOT_SIGNATURE_DESC2 *desc)
{
    /* Local root signatures map to the ShaderRecordBufferKHR. */
    struct vkd3d_descriptor_set_context context;
    struct d3d12_root_signature_info info;
    HRESULT hr;

    memset(&context, 0, sizeof(context));

    if (FAILED(hr = d3d12_root_signature_info_from_desc(&info, device, desc)))
        return hr;

#define D3D12_MAX_SHADER_RECORD_SIZE 4096
    if (info.cost * 4 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES > D3D12_MAX_SHADER_RECORD_SIZE)
    {
        ERR("Local root signature is too large.\n");
        hr = E_INVALIDARG;
        goto fail;
    }

    root_signature->binding_count = info.binding_count;
    root_signature->parameter_count = info.parameter_count;
    root_signature->static_sampler_count = desc->NumStaticSamplers;

    hr = E_OUTOFMEMORY;
    if (!(root_signature->parameters = vkd3d_calloc(root_signature->parameter_count,
            sizeof(*root_signature->parameters))))
        return hr;
    if (!(root_signature->bindings = vkd3d_calloc(root_signature->binding_count,
            sizeof(*root_signature->bindings))))
        return hr;
    root_signature->root_constant_count = info.root_constant_count;
    if (!(root_signature->root_constants = vkd3d_calloc(root_signature->root_constant_count,
            sizeof(*root_signature->root_constants))))
        return hr;
    if (!(root_signature->static_samplers = vkd3d_calloc(root_signature->static_sampler_count,
            sizeof(*root_signature->static_samplers))))
        return hr;
    if (!(root_signature->static_samplers_desc = vkd3d_calloc(root_signature->static_sampler_count,
            sizeof(*root_signature->static_samplers_desc))))
        return hr;

    if (FAILED(hr = d3d12_root_signature_init_local_static_samplers(root_signature, desc)))
        return hr;

    d3d12_root_signature_init_extra_bindings(root_signature, &info);

    if (FAILED(hr = d3d12_root_signature_init_shader_record_constants(root_signature, desc, &info)))
        return hr;
    if (FAILED(hr = d3d12_root_signature_init_shader_record_descriptors(root_signature, desc, &info, &context)))
        return hr;
    if (FAILED(hr = d3d12_root_signature_init_root_descriptor_tables(root_signature, desc, &info, &context)))
        return hr;

    if (FAILED(hr = vkd3d_private_store_init(&root_signature->private_store)))
        goto fail;

    return S_OK;

fail:
    return hr;
}



static HRESULT d3d12_root_signature_init_static_samplers(struct d3d12_root_signature *root_signature,
        const D3D12_ROOT_SIGNATURE_DESC2 *desc, struct vkd3d_descriptor_set_context *context,
        VkDescriptorSetLayout *vk_set_layout)
{
    VkDescriptorSetLayoutBinding *vk_binding_info, *vk_binding;
    struct vkd3d_shader_resource_binding *binding;
    unsigned int i;
    HRESULT hr;

    if (!desc->NumStaticSamplers)
        return S_OK;

    if (!(vk_binding_info = vkd3d_malloc(desc->NumStaticSamplers * sizeof(*vk_binding_info))))
        return E_OUTOFMEMORY;

    for (i = 0; i < desc->NumStaticSamplers; ++i)
    {
        const D3D12_STATIC_SAMPLER_DESC1 *s = &desc->pStaticSamplers[i];

        if (FAILED(hr = vkd3d_sampler_state_create_static_sampler(&root_signature->device->sampler_state,
                root_signature->device, s, &root_signature->static_samplers[i])))
            goto cleanup;

        vk_binding = &vk_binding_info[i];
        vk_binding->binding = context->vk_binding;
        vk_binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        vk_binding->descriptorCount = 1;
        vk_binding->stageFlags = vkd3d_vk_stage_flags_from_visibility(s->ShaderVisibility);
        vk_binding->pImmutableSamplers = &root_signature->static_samplers[i];

        binding = &root_signature->bindings[context->binding_index];
        binding->type = VKD3D_SHADER_DESCRIPTOR_TYPE_SAMPLER;
        binding->register_space = s->RegisterSpace;
        binding->register_index = s->ShaderRegister;
        binding->register_count = 1;
        binding->descriptor_table = 0;  /* ignored */
        binding->descriptor_offset = 0; /* ignored */
        binding->shader_visibility = vkd3d_shader_visibility_from_d3d12(s->ShaderVisibility);
        binding->flags = VKD3D_SHADER_BINDING_FLAG_IMAGE;
        binding->binding.binding = context->vk_binding;
        binding->binding.set = context->vk_set;

        context->binding_index += 1;
        context->vk_binding += 1;
    }

    if (FAILED(hr = vkd3d_create_descriptor_set_layout(root_signature->device, 0,
            desc->NumStaticSamplers, vk_binding_info,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT |
                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT,
            &root_signature->vk_sampler_descriptor_layout)))
        goto cleanup;

    /* With descriptor buffers we can implicitly bind immutable samplers, and no descriptors are necessary. */
    if (!d3d12_device_uses_descriptor_buffers(root_signature->device))
    {
        hr = vkd3d_sampler_state_allocate_descriptor_set(&root_signature->device->sampler_state,
                root_signature->device, root_signature->vk_sampler_descriptor_layout,
                &root_signature->vk_sampler_set, &root_signature->vk_sampler_pool);
    }
    else
        hr = S_OK;

cleanup:
    vkd3d_free(vk_binding_info);
    return hr;
}


static HRESULT d3d12_root_signature_init_push_constants(struct d3d12_root_signature *root_signature,
        const D3D12_ROOT_SIGNATURE_DESC2 *desc, const struct d3d12_root_signature_info *info,
        struct VkPushConstantRange *push_constant_range)
{
    unsigned int i, j;

    /* Stages set later. */
    push_constant_range->stageFlags = 0;
    push_constant_range->offset = 0;
    push_constant_range->size = 0;

    /* Put root descriptor VAs at the start to avoid alignment issues */
    for (i = 0; i < desc->NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1 *p = &desc->pParameters[i];

        if (d3d12_root_signature_parameter_is_raw_va(root_signature, p->ParameterType))
        {
            push_constant_range->stageFlags |= vkd3d_vk_stage_flags_from_visibility(p->ShaderVisibility);
            push_constant_range->size += sizeof(VkDeviceSize);
        }
    }

    /* Append actual root constants */
    for (i = 0, j = 0; i < desc->NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1 *p = &desc->pParameters[i];

        if (p->ParameterType != D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
            continue;

        root_signature->root_constant_mask |= 1ull << i;

        root_signature->parameters[i].parameter_type = p->ParameterType;
        root_signature->parameters[i].constant.constant_index = push_constant_range->size / sizeof(uint32_t);
        root_signature->parameters[i].constant.constant_count = p->Constants.Num32BitValues;

        root_signature->root_constants[j].register_space = p->Constants.RegisterSpace;
        root_signature->root_constants[j].register_index = p->Constants.ShaderRegister;
        root_signature->root_constants[j].shader_visibility = vkd3d_shader_visibility_from_d3d12(p->ShaderVisibility);
        root_signature->root_constants[j].offset = push_constant_range->size;
        root_signature->root_constants[j].size = p->Constants.Num32BitValues * sizeof(uint32_t);

        push_constant_range->stageFlags |= vkd3d_vk_stage_flags_from_visibility(p->ShaderVisibility);
        push_constant_range->size += p->Constants.Num32BitValues * sizeof(uint32_t);

        ++j;
    }

    /* Append one 32-bit push constant for each descriptor table offset */
    if (root_signature->device->bindless_state.flags)
    {
        root_signature->descriptor_table_offset = push_constant_range->size;

        for (i = 0; i < desc->NumParameters; ++i)
        {
            const D3D12_ROOT_PARAMETER1 *p = &desc->pParameters[i];

            if (p->ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
                continue;

            root_signature->descriptor_table_count += 1;

            push_constant_range->stageFlags |= vkd3d_vk_stage_flags_from_visibility(p->ShaderVisibility);
            push_constant_range->size += sizeof(uint32_t);
        }
    }

    return S_OK;
}

static void d3d12_root_signature_add_common_flags(struct d3d12_root_signature *root_signature,
        uint32_t common_flags)
{
    root_signature->graphics.flags |= common_flags;
    root_signature->mesh.flags |= common_flags;
    root_signature->compute.flags |= common_flags;
    root_signature->raygen.flags |= common_flags;
}


static HRESULT d3d12_root_signature_init_root_descriptor_tables(struct d3d12_root_signature *root_signature,
        const D3D12_ROOT_SIGNATURE_DESC2 *desc, const struct d3d12_root_signature_info *info,
        struct vkd3d_descriptor_set_context *context)
{
    struct vkd3d_bindless_state *bindless_state = &root_signature->device->bindless_state;
    struct vkd3d_shader_resource_binding binding;
    struct vkd3d_shader_descriptor_table *table;
    unsigned int i, j, t, range_count;
    uint32_t range_descriptor_offset;
    bool local_root_signature;

    local_root_signature = !!(desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    //if (d3d12_root_signature_may_require_global_heap_binding() ||
    //        (desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED))
    //    d3d12_root_signature_init_cbv_srv_uav_heap_bindings(root_signature, context);
    //if (desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED)
    //    d3d12_root_signature_init_sampler_heap_bindings(root_signature, context);

    for (i = 0, t = 0; i < desc->NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1 *p = &desc->pParameters[i];
        if (p->ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            continue;

        if (!local_root_signature)
            root_signature->descriptor_table_mask |= 1ull << i;

        table = &root_signature->parameters[i].descriptor_table;
        range_count = p->DescriptorTable.NumDescriptorRanges;
        range_descriptor_offset = 0;

        root_signature->parameters[i].parameter_type = p->ParameterType;

        if (local_root_signature)
            table->table_index = i;
        else
            table->table_index = t++;

        table->binding_count = 0;
        table->first_binding = &root_signature->bindings[context->binding_index];

        for (j = 0; j < range_count; ++j)
        {
            const D3D12_DESCRIPTOR_RANGE1 *range = &p->DescriptorTable.pDescriptorRanges[j];
            enum vkd3d_bindless_set_flag range_flag = vkd3d_bindless_set_flag_from_descriptor_range_type(range->RangeType);

            if (range->OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
                range_descriptor_offset = range->OffsetInDescriptorsFromTableStart;

            binding.type = vkd3d_descriptor_type_from_d3d12_range_type(range->RangeType);
            binding.register_space = range->RegisterSpace;
            binding.register_index = range->BaseShaderRegister;
            binding.register_count = range->NumDescriptors;
            binding.descriptor_table = table->table_index;
            binding.descriptor_offset = range_descriptor_offset;
            binding.shader_visibility = vkd3d_shader_visibility_from_d3d12(p->ShaderVisibility);

            switch (range->RangeType)
            {
                case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                    if (vkd3d_bindless_state_find_binding(bindless_state, range_flag, &binding.binding))
                    {
                        binding.flags = VKD3D_SHADER_BINDING_FLAG_BINDLESS | VKD3D_SHADER_BINDING_FLAG_IMAGE;
                        table->first_binding[table->binding_count++] = binding;
                    }
                    break;
                case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                    if (vkd3d_bindless_state_find_binding(bindless_state, range_flag, &binding.binding))
                    {
                        binding.flags = VKD3D_SHADER_BINDING_FLAG_BINDLESS | VKD3D_SHADER_BINDING_FLAG_BUFFER;
                        table->first_binding[table->binding_count++] = binding;
                    }
                    break;
                case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                    d3d12_root_signature_init_srv_uav_binding(root_signature, context, range->RangeType,
                            &binding, table->first_binding, &table->binding_count);
                    break;
                default:
                    FIXME("Unhandled descriptor range type %u.\n", range->RangeType);
            }

            range_descriptor_offset = binding.descriptor_offset + binding.register_count;
        }

        context->binding_index += table->binding_count;
    }

    return S_OK;
}


static void d3d12_root_signature_init_extra_bindings(struct d3d12_root_signature *root_signature,
        const struct d3d12_root_signature_info *info)
{
    vkd3d_bindless_state_find_binding(&root_signature->device->bindless_state,
            VKD3D_BINDLESS_SET_EXTRA_RAW_VA_AUX_BUFFER,
            &root_signature->raw_va_aux_buffer_binding);

    if (info->has_ssbo_offset_buffer || info->has_typed_offset_buffer)
    {
        if (info->has_ssbo_offset_buffer)
            d3d12_root_signature_add_common_flags(root_signature, VKD3D_ROOT_SIGNATURE_USE_SSBO_OFFSET_BUFFER);
        if (info->has_typed_offset_buffer)
            d3d12_root_signature_add_common_flags(root_signature, VKD3D_ROOT_SIGNATURE_USE_TYPED_OFFSET_BUFFER);

        vkd3d_bindless_state_find_binding(&root_signature->device->bindless_state,
                VKD3D_BINDLESS_SET_EXTRA_OFFSET_BUFFER,
                &root_signature->offset_buffer_binding);
    }

#ifdef VKD3D_ENABLE_DESCRIPTOR_QA
    if (vkd3d_descriptor_debug_active_descriptor_qa_checks())
    {
        vkd3d_bindless_state_find_binding(&root_signature->device->bindless_state,
                VKD3D_BINDLESS_SET_EXTRA_FEEDBACK_CONTROL_INFO_BUFFER,
                &root_signature->descriptor_qa_control_binding);
        vkd3d_bindless_state_find_binding(&root_signature->device->bindless_state,
                VKD3D_BINDLESS_SET_EXTRA_FEEDBACK_PAYLOAD_INFO_BUFFER,
                &root_signature->descriptor_qa_payload_binding);
    }
#endif
}



static HRESULT d3d12_root_signature_init_root_descriptors(struct d3d12_root_signature *root_signature,
        const D3D12_ROOT_SIGNATURE_DESC2 *desc, struct d3d12_root_signature_info *info,
        const VkPushConstantRange *push_constant_range, struct vkd3d_descriptor_set_context *context,
        VkDescriptorSetLayout *vk_set_layout)
{
    VkDescriptorSetLayoutBinding *vk_binding, *vk_binding_info = NULL;
    struct vkd3d_descriptor_hoist_desc *hoist_desc;
    struct vkd3d_shader_resource_binding *binding;
    struct vkd3d_shader_root_parameter *param;
    uint32_t raw_va_root_descriptor_count = 0;
    unsigned int hoisted_parameter_index;
    const D3D12_DESCRIPTOR_RANGE1 *range;
    unsigned int i, j, k;
    HRESULT hr = S_OK;
    uint32_t or_flags;

    or_flags = root_signature->graphics.flags |
            root_signature->compute.flags |
            root_signature->raygen.flags |
            root_signature->mesh.flags;

    if (info->push_descriptor_count || (or_flags & VKD3D_ROOT_SIGNATURE_USE_PUSH_CONSTANT_UNIFORM_BLOCK))
    {
        if (!(vk_binding_info = vkd3d_malloc(sizeof(*vk_binding_info) * (info->push_descriptor_count + 1))))
            return E_OUTOFMEMORY;
    }
    else if (!(root_signature->device->bindless_state.flags &
            (VKD3D_RAW_VA_ROOT_DESCRIPTOR_CBV | VKD3D_RAW_VA_ROOT_DESCRIPTOR_SRV_UAV)))
    {
        return S_OK;
    }

    hoisted_parameter_index = desc->NumParameters;

    for (i = 0, j = 0; i < desc->NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1 *p = &desc->pParameters[i];
        bool raw_va;

        if (!(desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE) &&
                p->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            unsigned int range_descriptor_offset = 0;
            for (k = 0; k < p->DescriptorTable.NumDescriptorRanges && info->hoist_descriptor_count; k++)
            {
                range = &p->DescriptorTable.pDescriptorRanges[k];
                if (range->OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
                    range_descriptor_offset = range->OffsetInDescriptorsFromTableStart;

                if (d3d12_descriptor_range_can_hoist_cbv_descriptor(root_signature->device, range))
                {
                    vk_binding = &vk_binding_info[j++];
                    vk_binding->binding = context->vk_binding;

                    vk_binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    vk_binding->descriptorCount = 1;
                    vk_binding->stageFlags = vkd3d_vk_stage_flags_from_visibility(p->ShaderVisibility);
                    vk_binding->pImmutableSamplers = NULL;

                    root_signature->root_descriptor_push_mask |= 1ull << hoisted_parameter_index;
                    hoist_desc = &root_signature->hoist_info.desc[root_signature->hoist_info.num_desc];
                    hoist_desc->table_index = i;
                    hoist_desc->parameter_index = hoisted_parameter_index;
                    hoist_desc->table_offset = range_descriptor_offset;
                    root_signature->hoist_info.num_desc++;

                    binding = &root_signature->bindings[context->binding_index];
                    binding->type = vkd3d_descriptor_type_from_d3d12_range_type(range->RangeType);
                    binding->register_space = range->RegisterSpace;
                    binding->register_index = range->BaseShaderRegister;
                    binding->register_count = 1;
                    binding->descriptor_table = 0;  /* ignored */
                    binding->descriptor_offset = 0; /* ignored */
                    binding->shader_visibility = vkd3d_shader_visibility_from_d3d12(p->ShaderVisibility);
                    binding->flags = VKD3D_SHADER_BINDING_FLAG_BUFFER;
                    binding->binding.binding = context->vk_binding;
                    binding->binding.set = context->vk_set;

                    param = &root_signature->parameters[hoisted_parameter_index];
                    param->parameter_type = D3D12_ROOT_PARAMETER_TYPE_CBV;
                    param->descriptor.binding = binding;

                    context->binding_index += 1;
                    context->vk_binding += 1;
                    hoisted_parameter_index += 1;
                    info->hoist_descriptor_count -= 1;
                }

                range_descriptor_offset += range->NumDescriptors;
            }
        }

        if (p->ParameterType != D3D12_ROOT_PARAMETER_TYPE_CBV
                && p->ParameterType != D3D12_ROOT_PARAMETER_TYPE_SRV
                && p->ParameterType != D3D12_ROOT_PARAMETER_TYPE_UAV)
            continue;

        raw_va = d3d12_root_signature_parameter_is_raw_va(root_signature, p->ParameterType);

        if (!raw_va)
        {
            vk_binding = &vk_binding_info[j++];
            vk_binding->binding = context->vk_binding;
            vk_binding->descriptorType = vk_descriptor_type_from_d3d12_root_parameter(root_signature->device, p->ParameterType);
            vk_binding->descriptorCount = 1;
            vk_binding->stageFlags = vkd3d_vk_stage_flags_from_visibility(p->ShaderVisibility);
            vk_binding->pImmutableSamplers = NULL;
            root_signature->root_descriptor_push_mask |= 1ull << i;
        }
        else
            root_signature->root_descriptor_raw_va_mask |= 1ull << i;

        binding = &root_signature->bindings[context->binding_index];
        binding->type = vkd3d_descriptor_type_from_d3d12_root_parameter_type(p->ParameterType);
        binding->register_space = p->Descriptor.RegisterSpace;
        binding->register_index = p->Descriptor.ShaderRegister;
        binding->register_count = 1;
        binding->descriptor_table = 0;  /* ignored */
        binding->descriptor_offset = 0; /* ignored */
        binding->shader_visibility = vkd3d_shader_visibility_from_d3d12(p->ShaderVisibility);
        binding->flags = VKD3D_SHADER_BINDING_FLAG_BUFFER;
        binding->binding.binding = context->vk_binding;
        binding->binding.set = context->vk_set;

        if (raw_va)
            binding->flags |= VKD3D_SHADER_BINDING_FLAG_RAW_VA;
        else if (vk_binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            binding->flags |= VKD3D_SHADER_BINDING_FLAG_RAW_SSBO;

        param = &root_signature->parameters[i];
        param->parameter_type = p->ParameterType;
        param->descriptor.binding = binding;
        param->descriptor.raw_va_root_descriptor_index = raw_va_root_descriptor_count;

        context->binding_index += 1;

        if (raw_va)
            raw_va_root_descriptor_count += 1;
        else
            context->vk_binding += 1;
    }

    if (or_flags & VKD3D_ROOT_SIGNATURE_USE_PUSH_CONSTANT_UNIFORM_BLOCK)
    {
        vk_binding = &vk_binding_info[j++];
        vk_binding->binding = context->vk_binding;
        vk_binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vk_binding->descriptorCount = 1;
        vk_binding->stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding->pImmutableSamplers = NULL;

        root_signature->push_constant_ubo_binding.set = context->vk_set;
        root_signature->push_constant_ubo_binding.binding = context->vk_binding;

        context->vk_binding += 1;
    }

#ifdef VKD3D_ENABLE_DESCRIPTOR_QA
    if (vkd3d_descriptor_debug_active_instruction_qa_checks())
    {
        vk_binding = &vk_binding_info[j++];
        vk_binding->binding = context->vk_binding;
        vk_binding->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vk_binding->descriptorCount = 1;
        vk_binding->stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding->pImmutableSamplers = NULL;

        root_signature->descriptor_qa_control_binding.set = context->vk_set;
        root_signature->descriptor_qa_control_binding.binding = context->vk_binding;
        context->vk_binding += 1;

        vk_binding = &vk_binding_info[j++];
        vk_binding->binding = context->vk_binding;
        vk_binding->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vk_binding->descriptorCount = 1;
        vk_binding->stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding->pImmutableSamplers = NULL;

        root_signature->descriptor_qa_payload_binding.set = context->vk_set;
        root_signature->descriptor_qa_payload_binding.binding = context->vk_binding;
        context->vk_binding += 1;
    }
#endif

    /* This should never happen. Min requirement for push descriptors is 32 and we can always fit into that limit. */
    if (j > root_signature->device->device_info.push_descriptor_properties.maxPushDescriptors)
    {
        ERR("Number of descriptors %u exceeds push descriptor limit of %u.\n",
                j, root_signature->device->device_info.push_descriptor_properties.maxPushDescriptors);
        vkd3d_free(vk_binding_info);
        return E_OUTOFMEMORY;
    }

    if (j)
    {
        hr = vkd3d_create_descriptor_set_layout(root_signature->device,
                VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                j, vk_binding_info,
                VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
                vk_set_layout);
    }

    vkd3d_free(vk_binding_info);
    return hr;
}

static HRESULT d3d12_root_signature_init_global(struct d3d12_root_signature *root_signature,
        struct d3d12_device *device, const D3D12_ROOT_SIGNATURE_DESC2 *desc)
{
    const VkPhysicalDeviceProperties *vk_device_properties = &device->device_info.properties2.properties;
    const struct vkd3d_bindless_state *bindless_state = &device->bindless_state;
    struct vkd3d_descriptor_set_context context;
    VkShaderStageFlagBits mesh_shader_stages;
    VkPushConstantRange push_constant_range;
    struct d3d12_root_signature_info info;
    unsigned int i;
    HRESULT hr;

    memset(&context, 0, sizeof(context));

    if (desc->Flags & ~(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
            | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
            | D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE))
        FIXME("Ignoring root signature flags %#x.\n", desc->Flags);

    if (FAILED(hr = d3d12_root_signature_info_from_desc(&info, device, desc)))
        return hr;

    if (info.cost > D3D12_MAX_ROOT_COST)
    {
        WARN("Root signature cost %u exceeds maximum allowed cost.\n", info.cost);
        return E_INVALIDARG;
    }

    root_signature->binding_count = info.binding_count;
    root_signature->parameter_count = info.parameter_count;
    root_signature->static_sampler_count = desc->NumStaticSamplers;

    hr = E_OUTOFMEMORY;
    if (!(root_signature->parameters = vkd3d_calloc(root_signature->parameter_count,
            sizeof(*root_signature->parameters))))
        return hr;
    if (!(root_signature->bindings = vkd3d_calloc(root_signature->binding_count,
            sizeof(*root_signature->bindings))))
        return hr;
    root_signature->root_constant_count = info.root_constant_count;
    if (!(root_signature->root_constants = vkd3d_calloc(root_signature->root_constant_count,
            sizeof(*root_signature->root_constants))))
        return hr;
    if (!(root_signature->static_samplers = vkd3d_calloc(root_signature->static_sampler_count,
            sizeof(*root_signature->static_samplers))))
        return hr;

    for (i = 0; i < bindless_state->set_count; i++)
        root_signature->set_layouts[context.vk_set++] = bindless_state->set_info[i].vk_set_layout;

    if (FAILED(hr = d3d12_root_signature_init_static_samplers(root_signature, desc,
                &context, &root_signature->vk_sampler_descriptor_layout)))
        return hr;

    if (root_signature->vk_sampler_descriptor_layout)
    {
        assert(context.vk_set < VKD3D_MAX_DESCRIPTOR_SETS);
        root_signature->set_layouts[context.vk_set] = root_signature->vk_sampler_descriptor_layout;
        root_signature->sampler_descriptor_set = context.vk_set;

        context.vk_binding = 0;
        context.vk_set += 1;
    }

    if (FAILED(hr = d3d12_root_signature_init_push_constants(root_signature, desc, &info,
            &push_constant_range)))
        return hr;

    /* If we cannot contain the push constants, fall back to push UBO everywhere. */
    if (push_constant_range.size > vk_device_properties->limits.maxPushConstantsSize)
        d3d12_root_signature_add_common_flags(root_signature, VKD3D_ROOT_SIGNATURE_USE_PUSH_CONSTANT_UNIFORM_BLOCK);
    else if (push_constant_range.size && (device->bindless_state.flags & VKD3D_FORCE_COMPUTE_ROOT_PARAMETERS_PUSH_UBO))
        root_signature->compute.flags |= VKD3D_ROOT_SIGNATURE_USE_PUSH_CONSTANT_UNIFORM_BLOCK;

    d3d12_root_signature_init_extra_bindings(root_signature, &info);

    /* Individual pipeline types may opt-in or out-of using the push UBO descriptor set. */
    root_signature->graphics.num_set_layouts = context.vk_set;
    root_signature->mesh.num_set_layouts = context.vk_set;
    root_signature->compute.num_set_layouts = context.vk_set;
    root_signature->raygen.num_set_layouts = context.vk_set;

    if (FAILED(hr = d3d12_root_signature_init_root_descriptors(root_signature, desc,
                &info, &push_constant_range, &context,
                &root_signature->vk_root_descriptor_layout)))
        return hr;

    if (root_signature->vk_root_descriptor_layout)
    {
        assert(context.vk_set < VKD3D_MAX_DESCRIPTOR_SETS);
        root_signature->set_layouts[context.vk_set] = root_signature->vk_root_descriptor_layout;
        root_signature->root_descriptor_set = context.vk_set;

        context.vk_binding = 0;
        context.vk_set += 1;
    }

    if (FAILED(hr = d3d12_root_signature_init_root_descriptor_tables(root_signature, desc, &info, &context)))
        return hr;

    /* Select push UBO style or push constants on a per-pipeline type basis. */
    d3d12_root_signature_update_bind_point_layout(&root_signature->graphics,
            &push_constant_range, &context, &info);
    d3d12_root_signature_update_bind_point_layout(&root_signature->mesh,
            &push_constant_range, &context, &info);
    d3d12_root_signature_update_bind_point_layout(&root_signature->compute,
            &push_constant_range, &context, &info);
    d3d12_root_signature_update_bind_point_layout(&root_signature->raygen,
            &push_constant_range, &context, &info);

    /* If we need to use restricted entry_points in vkCmdPushConstants,
     * we are unfortunately required to do it like this
     * since stageFlags in vkCmdPushConstants must cover at least all entry_points in the layout.
     *
     * We can pick the appropriate layout to use in PSO creation.
     * In set_root_signature we can bind the appropriate layout as well.
     *
     * For graphics we can generally rely on visibility mask, but not so for compute and raygen,
     * since they use ALL visibility. */

    if (FAILED(hr = vkd3d_create_pipeline_layout_for_stage_mask(
            device, root_signature->graphics.num_set_layouts, root_signature->set_layouts,
            &root_signature->graphics.push_constant_range,
            VK_SHADER_STAGE_ALL_GRAPHICS, &root_signature->graphics)))
        return hr;

    if (device->device_info.mesh_shader_features.meshShader && device->device_info.mesh_shader_features.taskShader)
    {
        mesh_shader_stages = VK_SHADER_STAGE_MESH_BIT_EXT |
                VK_SHADER_STAGE_TASK_BIT_EXT |
                VK_SHADER_STAGE_FRAGMENT_BIT;

        if (FAILED(hr = vkd3d_create_pipeline_layout_for_stage_mask(
                device, root_signature->mesh.num_set_layouts, root_signature->set_layouts,
                &root_signature->mesh.push_constant_range,
                mesh_shader_stages, &root_signature->mesh)))
            return hr;
    }

    if (FAILED(hr = vkd3d_create_pipeline_layout_for_stage_mask(
            device, root_signature->compute.num_set_layouts, root_signature->set_layouts,
            &root_signature->compute.push_constant_range,
            VK_SHADER_STAGE_COMPUTE_BIT, &root_signature->compute)))
        return hr;

    if (d3d12_device_supports_ray_tracing_tier_1_0(device))
    {
        if (FAILED(hr = vkd3d_create_pipeline_layout_for_stage_mask(
                device, root_signature->raygen.num_set_layouts, root_signature->set_layouts,
                &root_signature->raygen.push_constant_range,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_MISS_BIT_KHR |
                VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
                VK_SHADER_STAGE_CALLABLE_BIT_KHR |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                VK_SHADER_STAGE_ANY_HIT_BIT_KHR, &root_signature->raygen)))
            return hr;
    }

    return S_OK;
}


static HRESULT d3d12_root_signature_init(struct d3d12_root_signature *root_signature,
        struct d3d12_device *device, const D3D12_ROOT_SIGNATURE_DESC2 *desc)
{
    HRESULT hr;

    memset(root_signature, 0, sizeof(*root_signature));
    root_signature->ID3D12RootSignature_iface.lpVtbl = &d3d12_root_signature_vtbl;
    root_signature->refcount = 1;
    root_signature->internal_refcount = 1;

    root_signature->d3d12_flags = desc->Flags;
    /* needed by some methods, increment ref count later */
    root_signature->device = device;

    if (desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
        hr = d3d12_root_signature_init_local(root_signature, device, desc);
    else
        hr = d3d12_root_signature_init_global(root_signature, device, desc);

    if (FAILED(hr))
        goto fail;

    if (FAILED(hr = vkd3d_private_store_init(&root_signature->private_store)))
        goto fail;

    if (SUCCEEDED(hr))
        d3d12_device_add_ref(root_signature->device);

    return S_OK;

fail:
    d3d12_root_signature_cleanup(root_signature, device);
    return hr;
}


static HRESULT d3d12_root_signature_create_from_blob(struct d3d12_device *device,
        const void *bytecode, size_t bytecode_length, bool raw_payload,
        struct d3d12_root_signature **root_signature)
{
    const struct vkd3d_shader_code dxbc = {bytecode, bytecode_length};
    union
    {
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12;
        struct vkd3d_versioned_root_signature_desc vkd3d;
    } root_signature_desc;
    vkd3d_shader_hash_t compatibility_hash;
    struct d3d12_root_signature *object;
    HRESULT hr;
    int ret;

    if (raw_payload)
    {
        if ((ret = vkd3d_shader_parse_root_signature_v_1_2_from_raw_payload(&dxbc, &root_signature_desc.vkd3d,
                &compatibility_hash)))
        {
            WARN("Failed to parse root signature, vkd3d result %d.\n", ret);
            return hresult_from_vkd3d_result(ret);
        }
    }
    else
    {
        if ((ret = vkd3d_shader_parse_root_signature_v_1_2(&dxbc, &root_signature_desc.vkd3d, &compatibility_hash)) < 0)
        {
            WARN("Failed to parse root signature, vkd3d result %d.\n", ret);
            return hresult_from_vkd3d_result(ret);
        }
    }

    if (!(object = vkd3d_malloc(sizeof(*object))))
    {
        vkd3d_shader_free_root_signature(&root_signature_desc.vkd3d);
        return E_OUTOFMEMORY;
    }

    hr = d3d12_root_signature_init(object, device, &root_signature_desc.d3d12.Desc_1_2);

    /* For pipeline libraries, (and later DXR to some degree), we need a way to
     * compare root signature objects. */
    object->pso_compatibility_hash = compatibility_hash;
    object->layout_compatibility_hash = vkd3d_root_signature_v_1_2_compute_layout_compat_hash(
            &root_signature_desc.vkd3d.v_1_2);

    vkd3d_shader_free_root_signature(&root_signature_desc.vkd3d);
    if (FAILED(hr))
    {
        vkd3d_free(object);
        return hr;
    }

    d3d_destruction_notifier_init(&object->destruction_notifier, (IUnknown*)&object->ID3D12RootSignature_iface);

    TRACE("Created root signature %p.\n", object);

    *root_signature = object;

    return S_OK;
}
