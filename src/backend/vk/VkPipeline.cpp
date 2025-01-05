#include "VkPipeline.hpp"

#include "VkCommon.hpp"
#include "VkPhyDev.hpp"
#include "shader_converter/vkd3d_shader.h"

#include <vector>
#include <volk.h>

static inline bool d3d12_device_uses_descriptor_buffers(const struct d3d12_device *device)
{
    return device->global_descriptor_buffer.resource.va != 0;
}

HRESULT vkd3d_create_descriptor_set_layout(struct d3d12_device *device,
        VkDescriptorSetLayoutCreateFlags flags, unsigned int binding_count,
        const VkDescriptorSetLayoutBinding *bindings,
        VkDescriptorSetLayoutCreateFlags descriptor_buffer_flags,
        VkDescriptorSetLayout *set_layout)
{
    const struct vkd3d_vk_device_procs *vk_procs = &device->vk_procs;
    VkDescriptorSetLayoutCreateInfo set_desc;
    VkResult vr;

    set_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_desc.pNext = NULL;
    set_desc.flags = flags;
    set_desc.bindingCount = binding_count;
    set_desc.pBindings = bindings;

    if (d3d12_device_uses_descriptor_buffers(device))
        set_desc.flags |= descriptor_buffer_flags;

    if ((vr = VK_CALL(vkCreateDescriptorSetLayout(device->vk_device, &set_desc, NULL, set_layout))) < 0)
    {
        WARN("Failed to create Vulkan descriptor set layout, vr %d.\n", vr);
        return hresult_from_vk_result(vr);
    }

    return S_OK;
}


static HRESULT d3d12_root_signature_init_local_static_samplers(struct d3d12_root_signature *root_signature,
        const D3D12_ROOT_SIGNATURE_DESC2 *desc)
{
    unsigned int i;
    HRESULT hr;

    if (!desc->NumStaticSamplers)
        return S_OK;

    for (i = 0; i < desc->NumStaticSamplers; i++)
    {
        const D3D12_STATIC_SAMPLER_DESC1 *s = &desc->pStaticSamplers[i];
        if (FAILED(hr = vkd3d_sampler_state_create_static_sampler(&root_signature->device->sampler_state,
                root_signature->device, s, &root_signature->static_samplers[i])))
            return hr;
    }

    /* Cannot assign bindings until we've seen all local root signatures which go into an RTPSO.
     * For now, just copy the static samplers. RTPSO creation will build appropriate bindings. */
    memcpy(root_signature->static_samplers_desc, desc->pStaticSamplers,
            sizeof(*root_signature->static_samplers_desc) * desc->NumStaticSamplers);

    return S_OK;
}


static HRESULT d3d12_root_signature_info_from_desc(struct d3d12_root_signature_info *info,
        struct d3d12_device *device, const D3D12_ROOT_SIGNATURE_DESC2 *desc)
{
    bool local_root_signature;
    unsigned int i, j;
    HRESULT hr;

    memset(info, 0, sizeof(*info));

    local_root_signature = !!(desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    /* Need to emit bindings for the magic internal table binding. */
    if (d3d12_root_signature_may_require_global_heap_binding() ||
            (desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED))
    {
        d3d12_root_signature_info_count_srv_uav_table(info, device);
        d3d12_root_signature_info_count_srv_uav_table(info, device);
        d3d12_root_signature_info_count_cbv_table(info);
    }

    if (desc->Flags & D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED)
        d3d12_root_signature_info_count_sampler_table(info);

    for (i = 0; i < desc->NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1 *p = &desc->pParameters[i];

        switch (p->ParameterType)
        {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                for (j = 0; j < p->DescriptorTable.NumDescriptorRanges; ++j)
                    if (FAILED(hr = d3d12_root_signature_info_count_descriptors(info,
                            device, desc, &p->DescriptorTable.pDescriptorRanges[j])))
                        return hr;

                /* Local root signature directly affects memory layout. */
                if (local_root_signature)
                    info->cost = (info->cost + 1u) & ~1u;
                info->cost += local_root_signature ? 2 : 1;
                break;

            case D3D12_ROOT_PARAMETER_TYPE_CBV:

                /* Local root signature directly affects memory layout. */
                if (local_root_signature)
                    info->cost = (info->cost + 1u) & ~1u;
                else if (!(device->bindless_state.flags & VKD3D_RAW_VA_ROOT_DESCRIPTOR_CBV))
                    info->push_descriptor_count += 1;

                info->binding_count += 1;
                info->cost += 2;
                break;

            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
                /* Local root signature directly affects memory layout. */
                if (local_root_signature)
                    info->cost = (info->cost + 1u) & ~1u;
                else if (!(device->bindless_state.flags & VKD3D_RAW_VA_ROOT_DESCRIPTOR_SRV_UAV))
                    info->push_descriptor_count += 1;

                info->binding_count += 1;
                info->cost += 2;
                break;

            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
                info->root_constant_count += 1;
                info->cost += p->Constants.Num32BitValues;
                break;

            default:
                FIXME("Unhandled type %#x for parameter %u.\n", p->ParameterType, i);
                return E_NOTIMPL;
        }
    }

    if (!local_root_signature)
    {
        /* Make sure that we won't exceed device limits.
         * Minimum spec for push descriptors is 32 descriptors, which fits exactly what we need for D3D12.
         * Worst case scenarios:
         * - 32 root CBVs -> all 32 push descriptors are used. No push constants.
         * - Root constants > 128 bytes, 15 root CBVs. 1 push descriptor for push UBO. Can hoist 16 other descriptors.
         * Just base the amount of descriptors we can hoist on the root signature cost. This is simple and is trivially correct. */
        info->hoist_descriptor_count = min(info->hoist_descriptor_count, VKD3D_MAX_HOISTED_DESCRIPTORS);
        info->hoist_descriptor_count = min(info->hoist_descriptor_count, (D3D12_MAX_ROOT_COST - info->cost) / 2);

        info->push_descriptor_count += info->hoist_descriptor_count;
        info->binding_count += info->hoist_descriptor_count;
        info->binding_count += desc->NumStaticSamplers;

        if (vkd3d_descriptor_debug_active_instruction_qa_checks())
            info->push_descriptor_count += 2;
    }

    info->parameter_count = desc->NumParameters + info->hoist_descriptor_count;
    return S_OK;
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


void d3d12_pipeline_state_init_shader_interface(struct d3d12_pipeline_state *state,
        struct d3d12_device *device,
        VkShaderStageFlagBits stage,
        struct vkd3d_shader_interface_info *shader_interface)
{
    const struct d3d12_root_signature *root_signature = state->root_signature;
    memset(shader_interface, 0, sizeof(*shader_interface));
    shader_interface->flags = d3d12_root_signature_get_shader_interface_flags(root_signature, state->pipeline_type);
    shader_interface->min_ssbo_alignment = d3d12_device_get_ssbo_alignment(device);
    shader_interface->descriptor_tables.offset = root_signature->descriptor_table_offset;
    shader_interface->descriptor_tables.count = root_signature->descriptor_table_count;
    shader_interface->bindings = root_signature->bindings;
    shader_interface->binding_count = root_signature->binding_count;
    shader_interface->push_constant_buffers = root_signature->root_constants;
    shader_interface->push_constant_buffer_count = root_signature->root_constant_count;
    shader_interface->push_constant_ubo_binding = &root_signature->push_constant_ubo_binding;
    shader_interface->offset_buffer_binding = &root_signature->offset_buffer_binding;
    shader_interface->stage = stage;
    shader_interface->xfb_info = state->pipeline_type == VKD3D_PIPELINE_TYPE_GRAPHICS &&
            stage == state->graphics.cached_desc.xfb_stage ?
            state->graphics.cached_desc.xfb_info : NULL;
    shader_interface->descriptor_size_cbv_srv_uav = d3d12_device_get_descriptor_handle_increment_size(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    shader_interface->descriptor_size_sampler = d3d12_device_get_descriptor_handle_increment_size(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    if (stage == VK_SHADER_STAGE_MESH_BIT_EXT)
    {
        shader_interface->stage_output_map = &state->graphics.cached_desc.stage_io_map_ms_ps;
    }
    else if ((stage == VK_SHADER_STAGE_FRAGMENT_BIT) &&
            (state->graphics.stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT))
    {
        shader_interface->stage_input_map = &state->graphics.cached_desc.stage_io_map_ms_ps;
    }
}


void d3d12_pipeline_state_init_compile_arguments(struct d3d12_pipeline_state *state,
        struct d3d12_device *device, VkShaderStageFlagBits stage,
        struct vkd3d_shader_compile_arguments *compile_arguments)
{
    memset(compile_arguments, 0, sizeof(*compile_arguments));
    compile_arguments->target = VKD3D_SHADER_TARGET_SPIRV_VULKAN_1_0;
    compile_arguments->target_extensions = device->vk_info.shader_extensions;
    compile_arguments->min_subgroup_size = device->device_info.vulkan_1_3_properties.minSubgroupSize;
    compile_arguments->max_subgroup_size = device->device_info.vulkan_1_3_properties.maxSubgroupSize;
    compile_arguments->promote_wave_size_heuristics =
            d3d12_device_supports_required_subgroup_size_for_stage(device, stage);
    //compile_arguments->quirks = &vkd3d_shader_quirk_info;

    //if (vkd3d_config_flags & VKD3D_CONFIG_FLAG_DRIVER_VERSION_SENSITIVE_SHADERS)
    //{
    //    compile_arguments->driver_id = device->device_info.vulkan_1_2_properties.driverID;
    //    compile_arguments->driver_version = device->device_info.properties2.properties.driverVersion;
    //}

    if (stage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        /* Options which are exclusive to PS. Especially output swizzles must only be used in PS. */
        compile_arguments->parameters = state->graphics.cached_desc.ps_shader_parameters;
        compile_arguments->dual_source_blending = state->graphics.cached_desc.is_dual_source_blending;
        compile_arguments->output_swizzles = state->graphics.cached_desc.ps_output_swizzle;
        compile_arguments->output_swizzle_count = state->graphics.rt_count;
    }
}

VkPipeline vkd3d_vertex_input_pipeline_create(struct d3d12_device *device,
        const struct vkd3d_vertex_input_pipeline_desc *desc)
{
    VkGraphicsPipelineLibraryCreateInfoEXT library_create_info;
    struct vkd3d_vertex_input_pipeline_desc desc_copy = *desc;
    VkGraphicsPipelineCreateInfo create_info;
    VkPipeline vk_pipeline;
    VkResult vr;

    vkd3d_vertex_input_pipeline_desc_prepare(&desc_copy);

    memset(&library_create_info, 0, sizeof(library_create_info));
    library_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
    library_create_info.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.pNext = &library_create_info;
    create_info.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
    create_info.pInputAssemblyState = &desc_copy.ia_info;
    create_info.pVertexInputState = &desc_copy.vi_info;
    create_info.pDynamicState = &desc_copy.dy_info;
    create_info.basePipelineIndex = -1;

    if (d3d12_device_uses_descriptor_buffers(device))
        create_info.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateGraphicsPipelines(device->vk_device,
            VK_NULL_HANDLE, 1, &create_info, NULL, &vk_pipeline));
        
    //{
    //    ERR("Failed to create vertex input pipeline, vr %d.\n", vr);
    //    return VK_NULL_HANDLE;
    //}

    return vk_pipeline;
}

VkPipeline d3d12_device_get_or_create_vertex_input_pipeline(struct d3d12_device *device,
        const struct vkd3d_vertex_input_pipeline_desc *desc)
{
    struct vkd3d_vertex_input_pipeline pipeline, *entry;

    memset(&pipeline, 0, sizeof(pipeline));

    //rwlock_lock_read(&device->vertex_input_lock);
    entry = (void*)hash_map_find(&device->vertex_input_pipelines, desc);

    if (entry)
        pipeline.vk_pipeline = entry->vk_pipeline;

    //rwlock_unlock_read(&device->vertex_input_lock);

    if (!pipeline.vk_pipeline)
    {
        //rwlock_lock_write(&device->vertex_input_lock);
        pipeline.desc = *desc;

        entry = (void*)hash_map_insert(&device->vertex_input_pipelines, desc, &pipeline.entry);

        if (!entry->vk_pipeline)
            entry->vk_pipeline = vkd3d_vertex_input_pipeline_create(device, desc);

        pipeline.vk_pipeline = entry->vk_pipeline;
        //rwlock_unlock_write(&device->vertex_input_lock);
    }

    return pipeline.vk_pipeline;
}


VkPipeline vkd3d_fragment_output_pipeline_create(struct d3d12_device *device,
        const struct vkd3d_fragment_output_pipeline_desc *desc)
{
    struct vkd3d_fragment_output_pipeline_desc desc_copy = *desc;
    VkGraphicsPipelineLibraryCreateInfoEXT library_create_info;
    VkGraphicsPipelineCreateInfo create_info;
    VkPipeline vk_pipeline;
    VkResult vr;

    vkd3d_fragment_output_pipeline_desc_prepare(&desc_copy);

    memset(&library_create_info, 0, sizeof(library_create_info));
    library_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
    library_create_info.pNext = &desc_copy.rt_info;
    library_create_info.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.pNext = &library_create_info;
    create_info.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
    create_info.pColorBlendState = &desc_copy.cb_info;
    create_info.pMultisampleState = &desc_copy.ms_info;
    create_info.pDynamicState = &desc_copy.dy_info;
    create_info.basePipelineIndex = -1;

    if (d3d12_device_uses_descriptor_buffers(device))
        create_info.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateGraphicsPipelines(device->vk_device,
            VK_NULL_HANDLE, 1, &create_info, NULL, &vk_pipeline));
    //{
    //    ERR("Failed to create fragment output pipeline, vr %d.\n", vr);
    //    return VK_NULL_HANDLE;
    //}

    return vk_pipeline;
}


VkPipeline d3d12_device_get_or_create_fragment_output_pipeline(struct d3d12_device *device,
        const struct vkd3d_fragment_output_pipeline_desc *desc)
{
    struct vkd3d_fragment_output_pipeline pipeline, *entry;

    memset(&pipeline, 0, sizeof(pipeline));

    //rwlock_lock_read(&device->fragment_output_lock);
    entry = (void*)hash_map_find(&device->fragment_output_pipelines, desc);

    if (entry)
        pipeline.vk_pipeline = entry->vk_pipeline;

    //rwlock_unlock_read(&device->fragment_output_lock);

    if (!pipeline.vk_pipeline)
    {
        //rwlock_lock_write(&device->fragment_output_lock);
        pipeline.desc = *desc;

        entry = (void*)hash_map_insert(&device->fragment_output_pipelines, desc, &pipeline.entry);

        if (!entry->vk_pipeline)
            entry->vk_pipeline = vkd3d_fragment_output_pipeline_create(device, desc);

        pipeline.vk_pipeline = entry->vk_pipeline;
        //rwlock_unlock_write(&device->fragment_output_lock);
    }

    return pipeline.vk_pipeline;
}


void vkd3d_init_dynamic_state_array(std::vector<VkDynamicState>& dynamic_states, uint32_t dynamic_state_flags)
{
    static const struct
    {
        enum vkd3d_dynamic_state_flag flag;
        VkDynamicState vk_state;
    }
    vkd3d_dynamic_state_list[] =
    {
        { VKD3D_DYNAMIC_STATE_VIEWPORT,              VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT },
        { VKD3D_DYNAMIC_STATE_SCISSOR,               VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT },
        { VKD3D_DYNAMIC_STATE_BLEND_CONSTANTS,       VK_DYNAMIC_STATE_BLEND_CONSTANTS },
        { VKD3D_DYNAMIC_STATE_STENCIL_REFERENCE,     VK_DYNAMIC_STATE_STENCIL_REFERENCE },
        { VKD3D_DYNAMIC_STATE_DEPTH_BOUNDS,          VK_DYNAMIC_STATE_DEPTH_BOUNDS },
        { VKD3D_DYNAMIC_STATE_TOPOLOGY,              VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT },
        { VKD3D_DYNAMIC_STATE_VERTEX_BUFFER_STRIDE,  VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT },
        { VKD3D_DYNAMIC_STATE_FRAGMENT_SHADING_RATE, VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR },
        { VKD3D_DYNAMIC_STATE_PRIMITIVE_RESTART,     VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT },
        { VKD3D_DYNAMIC_STATE_PATCH_CONTROL_POINTS,  VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT },
        { VKD3D_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,    VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE },
        { VKD3D_DYNAMIC_STATE_STENCIL_WRITE_MASK,    VK_DYNAMIC_STATE_STENCIL_WRITE_MASK },
        { VKD3D_DYNAMIC_STATE_DEPTH_BIAS,            VK_DYNAMIC_STATE_DEPTH_BIAS },
        { VKD3D_DYNAMIC_STATE_DEPTH_BIAS,            VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE },
        { VKD3D_DYNAMIC_STATE_RASTERIZATION_SAMPLES, VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT },
    };

    for (auto& e : vkd3d_dynamic_state_list)
    {
        if (dynamic_state_flags & e.flag)
            dynamic_states.push_back( e.vk_state);
    }
}


static bool vkd3d_topology_type_can_restart(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
{
    return type == D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ||
           type == D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
}

static bool vkd3d_topology_can_restart(VkPrimitiveTopology topology)
{
    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        return false;

    default:
        return true;
    }
}


void vkd3d_vertex_input_pipeline_desc_init(struct vkd3d_vertex_input_pipeline_desc *desc,
        struct d3d12_pipeline_state *state, const struct vkd3d_pipeline_key *key, uint32_t dynamic_state_flags)
{
    struct d3d12_graphics_pipeline_state *graphics = &state->graphics;

    /* Mesh shader pipelines do not use vertex input state */
    assert(!(graphics->stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT));

    /* Do not set up pointers here as they would complicate hash table lookup */
    memset(desc, 0, sizeof(*desc));

    memcpy(desc->vi_divisors, graphics->instance_divisors, graphics->instance_divisor_count * sizeof(*desc->vi_divisors));
    desc->vi_divisor_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
    desc->vi_divisor_info.vertexBindingDivisorCount = graphics->instance_divisor_count;

    memcpy(desc->vi_bindings, graphics->attribute_bindings, graphics->attribute_binding_count * sizeof(*desc->vi_bindings));
    memcpy(desc->vi_attributes, graphics->attributes, graphics->attribute_count * sizeof(*desc->vi_attributes));
    desc->vi_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    desc->vi_info.vertexBindingDescriptionCount = graphics->attribute_binding_count;
    desc->vi_info.vertexAttributeDescriptionCount = graphics->attribute_count;

    desc->ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    desc->ia_info.topology = key && !key->dynamic_topology
            ? vk_topology_from_d3d12_topology(key->topology)
            : vk_topology_from_d3d12_topology_type(graphics->primitive_topology_type, !!graphics->index_buffer_strip_cut_value);
    desc->ia_info.primitiveRestartEnable = graphics->index_buffer_strip_cut_value && (key && !key->dynamic_topology
            ? vkd3d_topology_can_restart(desc->ia_info.topology)
            : vkd3d_topology_type_can_restart(graphics->primitive_topology_type));

    desc->dy_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    constexpr auto VKD3D_VERTEX_INPUT_DYNAMIC_STATE_MASK = (VKD3D_DYNAMIC_STATE_TOPOLOGY |
                                                            VKD3D_DYNAMIC_STATE_VERTEX_BUFFER_STRIDE |
                                                            VKD3D_DYNAMIC_STATE_PRIMITIVE_RESTART);

    vkd3d_init_dynamic_state_array(desc->dy_states,
            dynamic_state_flags & VKD3D_VERTEX_INPUT_DYNAMIC_STATE_MASK);
}

void vkd3d_vertex_input_pipeline_desc_prepare(struct vkd3d_vertex_input_pipeline_desc *desc)
{
    if (desc->vi_divisor_info.vertexBindingDivisorCount)
    {
        desc->vi_divisor_info.pVertexBindingDivisors = desc->vi_divisors;
        desc->vi_info.pNext = &desc->vi_divisor_info;
    }

    if (desc->vi_info.vertexAttributeDescriptionCount)
        desc->vi_info.pVertexAttributeDescriptions = desc->vi_attributes;

    if (desc->vi_info.vertexBindingDescriptionCount)
        desc->vi_info.pVertexBindingDescriptions = desc->vi_bindings;

    desc->dy_info.dynamicStateCount = desc->dy_states.size();
    if (desc->dy_info.dynamicStateCount)
        desc->dy_info.pDynamicStates = desc->dy_states.data();
}



void vkd3d_fragment_output_pipeline_desc_init(struct vkd3d_fragment_output_pipeline_desc *desc,
        struct d3d12_pipeline_state *state, const struct vkd3d_format *dsv_format, uint32_t dynamic_state_flags)
{
    struct d3d12_graphics_pipeline_state *graphics = &state->graphics;
    unsigned int i;

    memset(desc, 0, sizeof(*desc));

    memcpy(desc->cb_attachments, graphics->blend_attachments, graphics->rt_count * sizeof(*desc->cb_attachments));

    desc->cb_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    desc->cb_info.logicOpEnable = graphics->blend_desc.logicOpEnable;
    desc->cb_info.logicOp = graphics->blend_desc.logicOp;
    desc->cb_info.attachmentCount = graphics->rt_count;

    desc->ms_sample_mask = graphics->sample_mask;

    desc->ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    desc->ms_info.rasterizationSamples = graphics->ms_desc.rasterizationSamples;
    desc->ms_info.sampleShadingEnable = graphics->ms_desc.sampleShadingEnable;
    desc->ms_info.minSampleShading = graphics->ms_desc.minSampleShading;
    desc->ms_info.alphaToCoverageEnable = graphics->ms_desc.alphaToCoverageEnable;
    desc->ms_info.alphaToOneEnable = graphics->ms_desc.alphaToOneEnable;

    for (i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        desc->rt_formats[i] = graphics->rtv_active_mask & (1u << i)
            ? graphics->rtv_formats[i] : VK_FORMAT_UNDEFINED;
    }

    desc->rt_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    desc->rt_info.colorAttachmentCount = graphics->rt_count;
    /* From spec:  If depthAttachmentFormat is not VK_FORMAT_UNDEFINED, it must be a format that includes a depth aspect. */
    desc->rt_info.depthAttachmentFormat = dsv_format && (dsv_format->vk_aspect_mask & VK_IMAGE_ASPECT_DEPTH_BIT) ? dsv_format->vk_format : VK_FORMAT_UNDEFINED;
    /* From spec:  If stencilAttachmentFormat is not VK_FORMAT_UNDEFINED, it must be a format that includes a stencil aspect. */
    desc->rt_info.stencilAttachmentFormat = dsv_format && (dsv_format->vk_aspect_mask & VK_IMAGE_ASPECT_STENCIL_BIT) ? dsv_format->vk_format : VK_FORMAT_UNDEFINED;

    desc->dy_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    desc->dy_info.dynamicStateCount = vkd3d_init_dynamic_state_array(desc->dy_states,
            dynamic_state_flags & VKD3D_FRAGMENT_OUTPUT_DYNAMIC_STATE_MASK);
}

void vkd3d_fragment_output_pipeline_desc_prepare(struct vkd3d_fragment_output_pipeline_desc *desc)
{
    if (desc->cb_info.attachmentCount)
        desc->cb_info.pAttachments = desc->cb_attachments;

    desc->ms_info.pSampleMask = &desc->ms_sample_mask;

    if (desc->rt_info.colorAttachmentCount)
        desc->rt_info.pColorAttachmentFormats = desc->rt_formats;

    if (desc->dy_info.dynamicStateCount)
        desc->dy_info.pDynamicStates = desc->dy_states;
}


static bool vk_blend_attachment_needs_blend_constants(const VkPipelineColorBlendAttachmentState *attachment) {
 
    auto blend_factor_needs_blend_constants = [](VkBlendFactor blend_factor)
    {
        return blend_factor == VK_BLEND_FACTOR_CONSTANT_COLOR ||
                blend_factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR ||
                blend_factor == VK_BLEND_FACTOR_CONSTANT_ALPHA ||
                blend_factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    };

    return attachment->blendEnable && (
            blend_factor_needs_blend_constants(attachment->srcColorBlendFactor) ||
            blend_factor_needs_blend_constants(attachment->dstColorBlendFactor) ||
            blend_factor_needs_blend_constants(attachment->srcAlphaBlendFactor) ||
            blend_factor_needs_blend_constants(attachment->dstAlphaBlendFactor));
}

bool d3d12_device_supports_variable_shading_rate_tier_1(struct d3d12_device *device)
{
    const struct vkd3d_physical_device_info *info = &device->device_info;

    return info->fragment_shading_rate_features.pipelineFragmentShadingRate &&
            (device->vk_info.device_limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_2_BIT);
}

static bool d3d12_graphics_pipeline_needs_dynamic_rasterization_samples(const struct d3d12_graphics_pipeline_state *graphics)
{
    /* Ignore the case where the pipeline is compiled for a single sample since Vulkan drivers are robust against that. */
    if (graphics->rs_desc.rasterizerDiscardEnable ||
            (graphics->ms_desc.rasterizationSamples == VK_SAMPLE_COUNT_1_BIT 
            /*&& !(vkd3d_config_flags & VKD3D_CONFIG_FLAG_FORCE_DYNAMIC_MSAA)*/))
        return false;

    return graphics->rtv_active_mask || graphics->dsv_format ||
            (graphics->null_attachment_mask & dsv_attachment_mask(graphics));
}

uint32_t d3d12_graphics_pipeline_state_get_dynamic_state_flags(struct d3d12_pipeline_state *state,
        const struct vkd3d_pipeline_key *key)
{
    struct d3d12_graphics_pipeline_state *graphics = &state->graphics;
    bool is_mesh_pipeline, is_tess_pipeline;
    uint32_t dynamic_state_flags;
    unsigned int i;

    dynamic_state_flags = graphics->explicit_dynamic_states;

    is_mesh_pipeline = !!(graphics->stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT);
    is_tess_pipeline = !!(graphics->stage_flags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

    /* Enable dynamic states as necessary */
    dynamic_state_flags |= VKD3D_DYNAMIC_STATE_VIEWPORT | VKD3D_DYNAMIC_STATE_SCISSOR;

    if (graphics->attribute_binding_count && !is_mesh_pipeline)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_VERTEX_BUFFER_STRIDE;

    if (is_tess_pipeline && state->device->device_info.extended_dynamic_state2_features.extendedDynamicState2PatchControlPoints)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_PATCH_CONTROL_POINTS;

    if ((!key || key->dynamic_topology) && !is_mesh_pipeline && !is_tess_pipeline)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_TOPOLOGY;

    if (graphics->ds_desc.stencilTestEnable)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_STENCIL_REFERENCE;

    if (graphics->ds_desc.depthBoundsTestEnable)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_DEPTH_BOUNDS;

    /* If the DSV is read-only for a plane, writes are dynamically disabled. */
    if (graphics->ds_desc.depthTestEnable && graphics->ds_desc.depthWriteEnable)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;

    if (graphics->ds_desc.stencilTestEnable && (graphics->ds_desc.front.writeMask | graphics->ds_desc.back.writeMask))
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_STENCIL_WRITE_MASK;

    for (i = 0; i < graphics->rt_count; i++)
    {
        if (vk_blend_attachment_needs_blend_constants(&graphics->blend_attachments[i]))
            dynamic_state_flags |= VKD3D_DYNAMIC_STATE_BLEND_CONSTANTS;
    }

    /* We always need to enable fragment shading rate dynamic state when rasterizing.
     * D3D12 has no information about this ahead of time for a pipeline
     * unlike Vulkan.
     * Target Independent Rasterization (ForcedSampleCount) is not supported when this is used
     * so we don't need to worry about side effects when there are no render targets. */
    if (d3d12_device_supports_variable_shading_rate_tier_1(state->device) && graphics->rt_count)
    {
        /* If sample rate shading, ROVs are used, or depth stencil export is used force default VRS state.
         * Do this by not enabling the dynamic state.
         * This forces default static pipeline state to be used instead, which is what we want. */
        const uint32_t disable_flags =
                VKD3D_SHADER_META_FLAG_USES_SAMPLE_RATE_SHADING |
                VKD3D_SHADER_META_FLAG_USES_DEPTH_STENCIL_WRITE |
                VKD3D_SHADER_META_FLAG_USES_RASTERIZER_ORDERED_VIEWS;
        bool allow_vrs_combiners = true;

        for (i = 0; allow_vrs_combiners && i < graphics->stage_count; i++)
            if (graphics->code[i].meta.flags & disable_flags)
                allow_vrs_combiners = false;

        if (allow_vrs_combiners)
            dynamic_state_flags |= VKD3D_DYNAMIC_STATE_FRAGMENT_SHADING_RATE;
    }

    if (graphics->index_buffer_strip_cut_value && !is_mesh_pipeline)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_PRIMITIVE_RESTART;

    /* Enable dynamic sample count for multisampled pipelines so we can work around
     * bugs where the app may render to a single sampled render target. */
    if (d3d12_graphics_pipeline_needs_dynamic_rasterization_samples(&state->graphics) &&
            state->device->device_info.extended_dynamic_state3_features.extendedDynamicState3RasterizationSamples)
        dynamic_state_flags |= VKD3D_DYNAMIC_STATE_RASTERIZATION_SAMPLES;

    return dynamic_state_flags;
}

static uint32_t d3d12_graphics_pipeline_state_init_dynamic_state(struct d3d12_pipeline_state *state,
        VkPipelineDynamicStateCreateInfo *dynamic_desc, std::vector<VkDynamicState>& dynamic_state_buffer,
        const struct vkd3d_pipeline_key *key)
{
    uint32_t dynamic_state_flags = d3d12_graphics_pipeline_state_get_dynamic_state_flags(state, key);
    vkd3d_init_dynamic_state_array(dynamic_state_buffer, dynamic_state_flags);

    dynamic_desc->sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_desc->pNext = NULL;
    dynamic_desc->flags = 0;
    dynamic_desc->dynamicStateCount = dynamic_state_buffer.size();
    dynamic_desc->pDynamicStates = dynamic_desc->dynamicStateCount ? dynamic_state_buffer.data() : nullptr;
    return dynamic_state_flags;
}


static VkResult d3d12_pipeline_state_link_pipeline_variant(struct d3d12_pipeline_state *state,
        const struct vkd3d_pipeline_key *key, const struct vkd3d_format *dsv_format, VkPipelineCache vk_cache,
        uint32_t dynamic_state_flags, VkPipeline *vk_pipeline)
{
    const struct vkd3d_vk_device_procs *vk_procs = &state->device->vk_procs;
    struct d3d12_graphics_pipeline_state *graphics = &state->graphics;
    struct vkd3d_fragment_output_pipeline_desc fragment_output_desc;
    struct vkd3d_vertex_input_pipeline_desc vertex_input_desc;
    VkPipelineLibraryCreateInfoKHR library_info;
    VkGraphicsPipelineCreateInfo create_info;
    VkPipeline vk_libraries[3];
    uint32_t library_count = 0;
    VkResult vr;

    vk_libraries[library_count++] = graphics->library;

    if ((!(graphics->stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT)) &&
            (!(graphics->library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)))
    {
        vkd3d_vertex_input_pipeline_desc_init(&vertex_input_desc, state, key, dynamic_state_flags);
        vk_libraries[library_count++] = d3d12_device_get_or_create_vertex_input_pipeline(state->device, &vertex_input_desc);
    }

    if (!(graphics->library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT))
    {
        vkd3d_fragment_output_pipeline_desc_init(&fragment_output_desc, state, dsv_format, dynamic_state_flags);
        vk_libraries[library_count++] = d3d12_device_get_or_create_fragment_output_pipeline(state->device, &fragment_output_desc);
    }

    memset(&library_info, 0, sizeof(library_info));
    library_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
    library_info.libraryCount = library_count;
    library_info.pLibraries = vk_libraries;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.pNext = &library_info;
    create_info.flags = graphics->library_create_flags;
    create_info.layout = graphics->pipeline_layout;
    create_info.basePipelineIndex = -1;

    if (d3d12_device_uses_descriptor_buffers(state->device))
        create_info.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    if (graphics->disable_optimization)
        create_info.flags |= VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;

    /* Only use LINK_TIME_OPTIMIZATION for the primary pipeline for now,
     * accept a small runtime perf hit on subsequent compiles in order
     * to avoid stutter. */
    if (!key)
        create_info.flags |= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;

    vr = VK_CALL(vkCreateGraphicsPipelines(state->device->vk_device,
            vk_cache, 1, &create_info, NULL, vk_pipeline));

    if (vr != VK_SUCCESS && vr != VK_PIPELINE_COMPILE_REQUIRED)
    {
        ERR("Failed to link pipeline variant, vr %d.\n", vr);
        d3d12_pipeline_state_log_graphics_state(state);
    }

    return vr;
}

VkPipeline d3d12_pipeline_state_create_pipeline_variant(struct d3d12_pipeline_state *state,
        const struct vkd3d_pipeline_key *key, const struct vkd3d_format *dsv_format, VkPipelineCache vk_cache,
        VkGraphicsPipelineLibraryFlagsEXT library_flags, uint32_t *dynamic_state_flags)
{
    const struct vkd3d_vk_device_procs *vk_procs = &state->device->vk_procs;
    std::vector<VkDynamicState> dynamic_state_buffer;
    struct d3d12_graphics_pipeline_state *graphics = &state->graphics;
    VkPipelineCreationFeedbackEXT feedbacks[VKD3D_MAX_SHADER_STAGES];
    struct vkd3d_fragment_output_pipeline_desc fragment_output_desc;
    VkPipelineShaderStageCreateInfo stages[VKD3D_MAX_SHADER_STAGES];
    VkGraphicsPipelineLibraryCreateInfoEXT library_create_info;
    struct vkd3d_vertex_input_pipeline_desc vertex_input_desc;
    VkPipelineTessellationStateCreateInfo tessellation_info;
    bool has_vertex_input_state, has_fragment_output_state;
    VkPipelineCreationFeedbackCreateInfoEXT feedback_info;
    VkPipelineMultisampleStateCreateInfo multisample_info;
    VkPipelineDynamicStateCreateInfo dynamic_create_info;
    struct d3d12_device *device = state->device;
    VkGraphicsPipelineCreateInfo pipeline_desc;
    VkPipelineViewportStateCreateInfo vp_desc;
    VkPipelineCreationFeedbackEXT feedback;
    uint32_t stages_module_dup_mask = 0;
    VkPipeline vk_pipeline;
    unsigned int i;
    VkResult vr;
    HRESULT hr;

    *dynamic_state_flags = d3d12_graphics_pipeline_state_init_dynamic_state(state, &dynamic_create_info,
            dynamic_state_buffer, key);

    if (!library_flags && graphics->library)
    {
        if (d3d12_pipeline_state_link_pipeline_variant(state, key, dsv_format,
                vk_cache, *dynamic_state_flags, &vk_pipeline) == VK_SUCCESS)
            return vk_pipeline;
    }

    has_vertex_input_state = !(graphics->stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT) &&
            (!library_flags || (library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT));

    has_fragment_output_state = !library_flags || (library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);

    if (!(graphics->stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT))
    {
        vkd3d_vertex_input_pipeline_desc_init(&vertex_input_desc, state, key, *dynamic_state_flags);
        vkd3d_vertex_input_pipeline_desc_prepare(&vertex_input_desc);

        tessellation_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        tessellation_info.pNext = NULL;
        tessellation_info.flags = 0;
        tessellation_info.patchControlPoints = key && !key->dynamic_topology ?
                max(key->topology - D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1, 1) :
                graphics->patch_vertex_count;
    }

    vp_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_desc.pNext = NULL;
    vp_desc.flags = 0;
    vp_desc.viewportCount = 0;
    vp_desc.pViewports = NULL;
    vp_desc.scissorCount = 0;
    vp_desc.pScissors = NULL;

    vkd3d_fragment_output_pipeline_desc_init(&fragment_output_desc, state, dsv_format, *dynamic_state_flags);
    vkd3d_fragment_output_pipeline_desc_prepare(&fragment_output_desc);

    memset(&pipeline_desc, 0, sizeof(pipeline_desc));
    pipeline_desc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_desc.pNext = &fragment_output_desc.rt_info;
    pipeline_desc.stageCount = graphics->stage_count;
    pipeline_desc.pStages = graphics->stages;
    pipeline_desc.pViewportState = &vp_desc;
    pipeline_desc.pRasterizationState = &graphics->rs_desc;
    pipeline_desc.pDepthStencilState = &graphics->ds_desc;
    pipeline_desc.pDynamicState = &dynamic_create_info;
    pipeline_desc.layout = graphics->pipeline_layout;
    pipeline_desc.basePipelineIndex = -1;

    if (d3d12_device_supports_variable_shading_rate_tier_2(device))
        pipeline_desc.flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

    if (graphics->stage_flags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        pipeline_desc.pTessellationState = &tessellation_info;

    if (has_vertex_input_state)
    {
        pipeline_desc.pVertexInputState = &vertex_input_desc.vi_info;
        pipeline_desc.pInputAssemblyState = &vertex_input_desc.ia_info;
    }

    if (has_fragment_output_state || graphics->ms_desc.sampleShadingEnable)
    {
        multisample_info = graphics->ms_desc;

        if (key && key->rasterization_samples)
            multisample_info.rasterizationSamples = key->rasterization_samples;

        pipeline_desc.pMultisampleState = &multisample_info;
    }

    if (has_fragment_output_state)
        pipeline_desc.pColorBlendState = &fragment_output_desc.cb_info;

    if (library_flags)
    {
        TRACE("Compiling pipeline library for %p with flags %#x.\n", state, library_flags);

        library_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
        /* Explicit cast to silence a constness warning, this seems to be a Vulkan header bug */
        library_create_info.pNext = (void*)pipeline_desc.pNext;
        library_create_info.flags = library_flags;

        pipeline_desc.pNext = &library_create_info;
        pipeline_desc.flags |= VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
                VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

        graphics->library_flags = library_flags;
    }

    /* A workaround for SottR, which creates pipelines with DSV_UNKNOWN, but still insists on using a depth buffer.
     * If we notice that the base pipeline's DSV format does not match the dynamic DSV format, we fall-back to create a new render pass. */
    if (d3d12_graphics_pipeline_state_has_unknown_dsv_format_with_test(graphics) && dsv_format)
        TRACE("Compiling %p with fallback DSV format %#x.\n", state, dsv_format->vk_format);

    /* FIXME: This gets modified on late recompilation, could there be thread safety issues here?
     * For GENERAL depth-stencil, this mask should not matter at all, but there might be edge cases for tracked DSV. */
    graphics->dsv_plane_optimal_mask = d3d12_graphics_pipeline_state_get_plane_optimal_mask(graphics, dsv_format);

    if (key)
    {
        /* Need a reader lock here since a concurrent vkd3d_late_compile_shader_stages can
         * touch the VkShaderModule and/or code[] array. When late compile has been called once,
         * we will always have a concrete VkShaderModule to work with. */
        rwlock_lock_read(&state->lock);

        /* In a fallback pipeline, we might have to re-create shader modules.
         * This can happen from multiple threads, so need temporary pStages array. */
        memcpy(stages, graphics->stages, graphics->stage_count * sizeof(stages[0]));

        for (i = 0; i < graphics->stage_count; i++)
        {
            if (stages[i].module == VK_NULL_HANDLE && graphics->code[i].code)
            {
                if (FAILED(hr = d3d12_pipeline_state_create_shader_module(device,
                        &stages[i].module, &graphics->code[i])))
                {
                    /* This is kind of fatal and should only happen for out-of-memory. */
                    ERR("Unexpected failure (hr %x) in creating fallback SPIR-V module.\n", hr);
                    rwlock_unlock_read(&state->lock);
                    vk_pipeline = VK_NULL_HANDLE;
                    goto err;
                }

                pipeline_desc.pStages = stages;
                /* Remember to free this module. */
                stages_module_dup_mask |= 1u << i;
            }
        }

        rwlock_unlock_read(&state->lock);
    }

    /* If we're using identifiers, set the appropriate flag. */
    for (i = 0; i < graphics->stage_count; i++)
        if (pipeline_desc.pStages[i].module == VK_NULL_HANDLE)
            pipeline_desc.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;

    if (d3d12_device_uses_descriptor_buffers(device))
        pipeline_desc.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    if (graphics->disable_optimization)
        pipeline_desc.flags |= VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;

    TRACE("Calling vkCreateGraphicsPipelines.\n");

    if (vkd3d_config_flags & VKD3D_CONFIG_FLAG_PIPELINE_LIBRARY_LOG)
    {
        feedback_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT;
        feedback_info.pNext = pipeline_desc.pNext;
        feedback_info.pPipelineStageCreationFeedbacks = feedbacks;
        feedback_info.pipelineStageCreationFeedbackCount = pipeline_desc.stageCount;
        feedback_info.pPipelineCreationFeedback = &feedback;
        pipeline_desc.pNext = &feedback_info;
    }
    else
        feedback_info.pipelineStageCreationFeedbackCount = 0;

    vr = VK_CALL(vkCreateGraphicsPipelines(device->vk_device, vk_cache, 1, &pipeline_desc, NULL, &vk_pipeline));

    if (vkd3d_config_flags & VKD3D_CONFIG_FLAG_PIPELINE_LIBRARY_LOG)
    {
        if (pipeline_desc.flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT)
        {
            if (vr == VK_SUCCESS)
                INFO("[IDENTIFIER] Successfully created graphics pipeline from identifier.\n");
            else if (vr == VK_PIPELINE_COMPILE_REQUIRED)
                INFO("[IDENTIFIER] Failed to create graphics pipeline from identifier, falling back ...\n");
        }
        else
            INFO("[IDENTIFIER] No graphics identifier\n");
    }

    if (vr == VK_PIPELINE_COMPILE_REQUIRED)
    {
        if (FAILED(hr = vkd3d_late_compile_shader_stages(state)))
        {
            ERR("Late compilation of SPIR-V failed.\n");
            vk_pipeline = VK_NULL_HANDLE;
            goto err;
        }

        pipeline_desc.flags &= ~VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        /* Internal modules are known to be non-null now. */
        pipeline_desc.pStages = state->graphics.stages;
        vr = VK_CALL(vkCreateGraphicsPipelines(device->vk_device, vk_cache, 1, &pipeline_desc, NULL, &vk_pipeline));
    }

    TRACE("Completed vkCreateGraphicsPipelines.\n");

    if (vr < 0)
    {
        ERR("Failed to create Vulkan graphics pipeline, vr %d.\n", vr);
        d3d12_pipeline_state_log_graphics_state(state);

        vk_pipeline = VK_NULL_HANDLE;
        goto err;
    }

    if (feedback_info.pipelineStageCreationFeedbackCount)
        vkd3d_report_pipeline_creation_feedback_results(&feedback_info);

    if (library_flags)
        graphics->library_create_flags = pipeline_desc.flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;

err:
    /* Clean up any temporary SPIR-V modules we created. */
    while (stages_module_dup_mask)
    {
        i = vkd3d_bitmask_iter32(&stages_module_dup_mask);
        VK_CALL(vkDestroyShaderModule(device->vk_device, stages[i].module, NULL));
    }

    return vk_pipeline;
}




static HRESULT d3d12_pipeline_state_init_static_pipeline(struct d3d12_pipeline_state *state,
        const struct d3d12_pipeline_state_desc *desc)
{
    struct d3d12_graphics_pipeline_state *graphics = &state->graphics;
    bool can_compile_pipeline_early, has_gpl, create_library = false;
    VkGraphicsPipelineLibraryFlagsEXT library_flags = 0;
    unsigned int i;

    for (i = 0; i < graphics->stage_count; i++)
        if (graphics->code[i].meta.flags & VKD3D_SHADER_META_FLAG_DISABLE_OPTIMIZATIONS)
            graphics->disable_optimization = true;

    has_gpl = state->device->device_info.graphics_pipeline_library_features.graphicsPipelineLibrary;

    library_flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
            VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
            VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
            VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

    if (d3d12_graphics_pipeline_state_has_unknown_dsv_format_with_test(graphics))
    {
        library_flags &= ~VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
        create_library = true;
    }

    if (graphics->stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT)
    {
        can_compile_pipeline_early = true;

        library_flags &= ~VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
        graphics->pipeline_layout = state->root_signature->mesh.vk_pipeline_layout;
    }
    else
    {
        /* Defer compilation if tessellation is enabled but the patch vertex count is not known */
        bool has_tess = !!(graphics->stage_flags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

        can_compile_pipeline_early = !has_tess || graphics->patch_vertex_count != 0 ||
                state->device->device_info.extended_dynamic_state2_features.extendedDynamicState2PatchControlPoints;

        if (desc->primitive_topology_type == D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED)
        {
            library_flags &= ~VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
            create_library = true;

            can_compile_pipeline_early = false;
        }

        /* In case of tessellation shaders, we may have to recompile the pipeline with a
         * different patch vertex count, which is part of pre-rasterization state. Do not
         * create a pipeline library if dynamic patch control points are unsupported. */
        if (has_tess && !state->device->device_info.extended_dynamic_state2_features.extendedDynamicState2PatchControlPoints)
            create_library = false;

        graphics->pipeline_layout = state->root_signature->graphics.vk_pipeline_layout;
    }

    graphics->pipeline = VK_NULL_HANDLE;
    graphics->library = VK_NULL_HANDLE;
    graphics->library_flags = 0;
    graphics->library_create_flags = 0;

    if (create_library && has_gpl)
    {
        if (!(graphics->library = d3d12_pipeline_state_create_pipeline_variant(state, NULL, graphics->dsv_format,
                state->vk_pso_cache, library_flags, &graphics->pipeline_dynamic_states)))
            return E_OUTOFMEMORY;
    }

    if (can_compile_pipeline_early)
    {
        if (!(graphics->pipeline = d3d12_pipeline_state_create_pipeline_variant(state, NULL, graphics->dsv_format,
                state->vk_pso_cache, 0, &graphics->pipeline_dynamic_states)))
            return E_OUTOFMEMORY;
    }
    else
    {
        graphics->dsv_plane_optimal_mask = d3d12_graphics_pipeline_state_get_plane_optimal_mask(graphics, NULL);
    }

    return S_OK;
}





static HRESULT d3d12_pipeline_state_init_compute(struct d3d12_pipeline_state *state,
        struct d3d12_device *device, const struct d3d12_pipeline_state_desc *desc,
        const struct d3d12_cached_pipeline_state *cached_pso)
{
    const struct vkd3d_vk_device_procs *vk_procs = &device->vk_procs;
    struct vkd3d_shader_interface_info shader_interface;
    HRESULT hr;

    state->pipeline_type = VKD3D_PIPELINE_TYPE_COMPUTE;
    d3d12_pipeline_state_init_shader_interface(state, device,
            VK_SHADER_STAGE_COMPUTE_BIT, &shader_interface);
    shader_interface.flags |= vkd3d_descriptor_debug_get_shader_interface_flags(
            device->descriptor_qa_global_info, desc->cs.pShaderBytecode, desc->cs.BytecodeLength);

    vkd3d_load_spirv_from_cached_state(device, cached_pso,
            VK_SHADER_STAGE_COMPUTE_BIT, &state->compute.code,
            &state->compute.identifier_create_info);

    hr = vkd3d_create_compute_pipeline(state, device, &desc->cs);

    if (FAILED(hr))
    {
        WARN("Failed to create Vulkan compute pipeline, hr %#x.\n", hr);
        return hr;
    }

    if (FAILED(hr = vkd3d_private_store_init(&state->private_store)))
    {
        VK_CALL(vkDestroyPipeline(device->vk_device, state->compute.vk_pipeline, NULL));
        return hr;
    }

    d3d_destruction_notifier_init(&state->destruction_notifier, (IUnknown*)&state->ID3D12PipelineState_iface);
    d3d12_device_add_ref(state->device = device);

    return S_OK;
}



HRESULT d3d12_pipeline_state_create(struct d3d12_device *device, VkPipelineBindPoint bind_point,
        const struct d3d12_pipeline_state_desc *desc, struct d3d12_pipeline_state **state)
{
    const struct vkd3d_vk_device_procs *vk_procs = &device->vk_procs;
    const struct d3d12_cached_pipeline_state *desc_cached_pso;
    struct d3d12_cached_pipeline_state cached_pso;
    auto *object = new d3d12_pipeline_state{};
    HRESULT hr;

    memset(object, 0, sizeof(*object));

    if (!desc->root_signature)
    {
        if (FAILED(hr = d3d12_pipeline_create_private_root_signature(device,
                bind_point, desc, &object->root_signature)))
        {
            ERR("No root signature for pipeline.\n");
            vkd3d_free(object);
            return hr;
        }
        object->root_signature_compat_hash_is_dxbc_derived = true;
    }
    else
    {
        object->root_signature = impl_from_ID3D12RootSignature(desc->root_signature);
        /* Hold a private reference on this root signature in case we have to create fallback PSOs. */
        d3d12_root_signature_inc_ref(object->root_signature);
    }

    


    hr = S_OK;

    if (!(vkd3d_config_flags & VKD3D_CONFIG_FLAG_GLOBAL_PIPELINE_CACHE))
        if (FAILED(hr = vkd3d_create_pipeline_cache_from_d3d12_desc(device, desc_cached_pso, &object->vk_pso_cache)))
            ERR("Failed to create pipeline cache, hr %d.\n", hr);

    /* By using our own VkPipelineCache, drivers will generally not cache pipelines internally in memory.
     * For games that spam an extreme number of pipelines only to serialize them to pipeline libraries and then
     * release the pipeline state, we will run into memory issues on memory constrained systems since a driver might
     * be tempted to keep several gigabytes of PSO binaries live in memory.
     * A workaround (pilfered from Fossilize) is to create our own pipeline cache and destroy it.
     * Ideally there would be a flag to disable in-memory caching (but retain on-disk cache),
     * but that's extremely specific, so do what we gotta do. */

    if (SUCCEEDED(hr))
    {
        switch (bind_point)
        {
            case VK_PIPELINE_BIND_POINT_COMPUTE:
                hr = d3d12_pipeline_state_init_compute(object, device, desc, desc_cached_pso);
                break;

            case VK_PIPELINE_BIND_POINT_GRAPHICS:
                /* Creating a graphics PSO is more involved ... */
                hr = d3d12_pipeline_state_init_graphics_create_info(object, device, desc);
                if (SUCCEEDED(hr))
                    hr = d3d12_pipeline_state_init_graphics_spirv(object, desc, desc_cached_pso);
                if (SUCCEEDED(hr))
                    hr = d3d12_pipeline_state_init_static_pipeline(object, desc);
                if (SUCCEEDED(hr))
                    hr = d3d12_pipeline_state_finish_graphics(object);
                break;

            default:
                ERR("Invalid pipeline type %u.\n", bind_point);
                hr = E_INVALIDARG;
        }
    }

    if (FAILED(hr))
    {
        if (object->root_signature)
            d3d12_root_signature_dec_ref(object->root_signature);
        d3d12_pipeline_state_free_spirv_code(object);
        d3d12_pipeline_state_free_spirv_code_debug(object);
        d3d12_pipeline_state_destroy_shader_modules(object, device);
        if (object->pipeline_type == VKD3D_PIPELINE_TYPE_GRAPHICS || object->pipeline_type == VKD3D_PIPELINE_TYPE_MESH_GRAPHICS)
            d3d12_pipeline_state_free_cached_desc(&object->graphics.cached_desc);
        VK_CALL(vkDestroyPipelineCache(device->vk_device, object->vk_pso_cache, NULL));
        rwlock_destroy(&object->lock);

        vkd3d_free(object);
        return hr;
    }

    /* The strategy here is that we need to keep the SPIR-V alive somehow.
     * If we don't need to serialize SPIR-V from the PSO, then we don't need to keep the code alive as pointer/size pairs.
     * The scenarios for this case is:
     * - When we choose to not serialize SPIR-V at all with VKD3D_CONFIG
     * - PSO was loaded from a cached blob. It's extremely unlikely that anyone is going to try
     *   serializing that PSO again, so there should be no need to keep it alive.
     * - We are using a disk cache with SHADER_IDENTIFIER support.
     *   In this case, we'll never store the SPIR-V itself, but the identifier, so we don't need to keep the code around.
     *
     * The worst that would happen is a performance loss should that entry be reloaded later.
     * For graphics pipelines, we have to keep VkShaderModules around in case we need fallback pipelines.
     * If we keep the SPIR-V around in memory, we can always create shader modules on-demand in case we
     * need to actually create fallback pipelines. This avoids unnecessary memory bloat. */
    if (desc_cached_pso->blob.CachedBlobSizeInBytes ||
            (device->disk_cache.library && (device->disk_cache.library->flags & VKD3D_PIPELINE_LIBRARY_FLAG_SHADER_IDENTIFIER)) ||
            (vkd3d_config_flags & VKD3D_CONFIG_FLAG_PIPELINE_LIBRARY_NO_SERIALIZE_SPIRV))
        d3d12_pipeline_state_free_spirv_code(object);
    else
        d3d12_pipeline_state_destroy_shader_modules(object, device);

    /* If it is impossible for us to recompile this shader, we can free VkShaderModules. Saves a lot of memory.
     * If we are required to be able to serialize the SPIR-V, it will live as host pointers, not VkShaderModule. */
    if (object->pso_is_fully_dynamic)
        d3d12_pipeline_state_destroy_shader_modules(object, device);

    /* We don't expect to serialize the PSO blob if we loaded it from cache.
     * Free the cache now to save on memory. */
    if (desc_cached_pso->blob.CachedBlobSizeInBytes)
    {
        VK_CALL(vkDestroyPipelineCache(device->vk_device, object->vk_pso_cache, NULL));
        object->vk_pso_cache = VK_NULL_HANDLE;

        /* Set this explicitly so we avoid attempting to touch code[i] when serializing the PSO blob.
         * We are at risk of compiling code on the fly in some upcoming situations. */
        object->pso_is_loaded_from_cached_blob = true;
    }
    else if (device->disk_cache.library)
    {
        /* We compiled this PSO without any cache (internal or app-provided),
         * so we should serialize this to internal disk cache.
         * Pushes work to disk$ thread. */
        vkd3d_pipeline_library_store_pipeline_to_disk_cache(&device->disk_cache, object);
    }

    TRACE("Created pipeline state %p.\n", object);

    *state = object;
    return S_OK;
}

