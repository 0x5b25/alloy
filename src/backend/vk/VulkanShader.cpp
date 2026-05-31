#include "VulkanShader.hpp"

#include "VulkanDevice.hpp"
#include "VkCommon.hpp"
#include "VulkanBindableResource.hpp"

#include <format>

namespace alloy::vk
{
    static VkDescriptorType DXILResKind2VkDescType(dxil_spv_resource_kind kind, bool writable) {
        
        VkDescriptorType vkType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        switch(kind) {
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_1D:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_2D:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_2DMS:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_3D:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_CUBE:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_1D_ARRAY:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_2D_ARRAY:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_2D_MS_ARRAY:
        case DXIL_SPV_RESOURCE_KIND_TEXTURE_CUBE_ARRAY:
            vkType = writable? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                             : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; 
            break;
    
        case DXIL_SPV_RESOURCE_KIND_TYPED_BUFFER:
        case DXIL_SPV_RESOURCE_KIND_RAW_BUFFER:
        case DXIL_SPV_RESOURCE_KIND_STRUCTURED_BUFFER:
            vkType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;

        default: 
            assert(false && "Unlikely alloy shader resource type");
        }

        return vkType;
    }

    static bool Str2Semantic(const char* str, VertexInputSemantic::Name& semantic){

        static const struct {
            const char* str;
            VertexInputSemantic::Name enumVal;
        } lut[] {
            {"BINORMAL",     VertexInputSemantic::Name::Binormal},
            {"BLENDINDICES", VertexInputSemantic::Name::BlendIndices},
            {"BLENDWEIGHT",  VertexInputSemantic::Name::BlendWeight},
            {"COLOR",        VertexInputSemantic::Name::Color},
            {"NORMAL",       VertexInputSemantic::Name::Normal},
            {"POSITION",     VertexInputSemantic::Name::Position},
            {"PSIZE",        VertexInputSemantic::Name::PointSize},
            {"TANGENT",      VertexInputSemantic::Name::Tangent},
            {"TEXCOORD",     VertexInputSemantic::Name::TextureCoordinate},
        };

        for(auto& entry : lut) {
            if(std::strcmp(entry.str, str) == 0) {
                semantic = entry.enumVal;
                return true;
            }
        }

        return false;
    }
    
    SPVRemapper::SPVRemapper(
        const VulkanResourceLayout* layout,
        const IAMappingInfo* iaMappings
    )
        : layout(layout)
        , iaMappings(iaMappings)
    { }
    
    bool SPVRemapper::FindVkBindingSet(
        VkDescriptorType type,
        uint32_t d3dRegSpace,
        uint32_t d3dRegIdx,
        uint32_t& vkSetOut,
        uint32_t& vkSlotOut
    ) {
        const auto& bindingRemappings = layout->GetResSetInfo();

        for(const auto& s : bindingRemappings) {
            if(s.type == type) {
                for(auto& b : s.bindings) {
                    if(b.regSpaceDesignated == d3dRegSpace &&
                        b.regIdxDesignated == d3dRegIdx) {
                        vkSetOut = b.bindSetAllocated;
                        vkSlotOut = b.bindSlotAllocated;
                        return true;
                    }
                }
            }
        }

        // Send to global heap if useGlobalHeaps == true;
        // otherwise this should result in error
        assert(layout->GetDesc().useGlobalHeaps
            && __FUNCTION__": Failed to find vulkan bindings");
        return false;
    }
    
    bool SPVRemapper::RemapSRV( 
        const dxil_spv_d3d_binding& binding,
        dxil_spv_srv_vulkan_binding& vk_binding
    ) {
        
        if(!layout) {
            assert(false && "Can't remap a shader using pipelines that have no resource layout");
            return false;
        }
            
        VkDescriptorType vkType = DXILResKind2VkDescType(binding.kind, false);
        if(vkType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            return false;

        if(vkType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            vk_binding.buffer_binding.descriptor_type 
                = DXIL_SPV_VULKAN_DESCRIPTOR_TYPE_SSBO;

        // T2 bindless
        if(layout->GetDesc().useGlobalHeaps) {
            vk_binding.buffer_binding.set = 0;
            vk_binding.buffer_binding.binding = 0;
            vk_binding.buffer_binding.bindless.use_heap = DXIL_SPV_TRUE;
            vk_binding.buffer_binding.bindless.heap_root_offset = 0;

            vk_binding.offset_binding = { };

            return true;
        }
        // Legacy bindful
        else {
            vk_binding.buffer_binding.bindless.use_heap = DXIL_SPV_FALSE;
            vk_binding.offset_binding = {};
            uint32_t slot, set;
            if(FindVkBindingSet(vkType,
                                binding.register_space,
                                binding.register_index,
                                set, slot)){
                vk_binding.buffer_binding.set = set;
                vk_binding.buffer_binding.binding = slot;
                //vk_binding.buffer_binding.descriptor_type = VulkanDescriptorType::Identity;
                

                return true;
            }
            return false;
        }
    }
    
    bool SPVRemapper::RemapUAV( 
        const dxil_spv_uav_d3d_binding& binding,
        dxil_spv_uav_vulkan_binding& vk_binding
    ) {
        if(!layout) {
            assert(false && "Can't remap a shader using pipelines that have no resource layout");
            return false;
        }
            
        VkDescriptorType vkType = DXILResKind2VkDescType(binding.d3d_binding.kind, true);
        if(vkType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            return false;

        if(vkType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            vk_binding.buffer_binding.descriptor_type 
                = DXIL_SPV_VULKAN_DESCRIPTOR_TYPE_SSBO;

        // T2 bindless
        if(layout->GetDesc().useGlobalHeaps) {
            vk_binding.buffer_binding.set = 0;
            vk_binding.buffer_binding.binding = 0;
            vk_binding.buffer_binding.bindless.use_heap = DXIL_SPV_TRUE;
            vk_binding.buffer_binding.bindless.heap_root_offset = 0;

            vk_binding.offset_binding = { };

            return true;
        }
        // Legacy bindful
        else {
            vk_binding.buffer_binding.bindless.use_heap = DXIL_SPV_FALSE;
            vk_binding.offset_binding = {};
            uint32_t slot, set;
            if(FindVkBindingSet(vkType,
                                binding.d3d_binding.register_space,
                                binding.d3d_binding.register_index,
                                set, slot)){
                vk_binding.buffer_binding.set = set;
                vk_binding.buffer_binding.binding = slot;
                //vk_binding.buffer_binding.descriptor_type = VulkanDescriptorType::Identity;
                

                return true;
            }
            return false;
        }
    }

    bool SPVRemapper::RemapSampler( const dxil_spv_d3d_binding& binding,
                                dxil_spv_vulkan_binding& vk_binding
    ) {
        if(!layout) {
            assert(false && "Can't remap a shader using pipelines that have no resource layout");
            return false;
        }
        // T2 bindless
        if(layout->GetDesc().useGlobalHeaps) {
            vk_binding.set = 1;
            vk_binding.binding = 0;
            vk_binding.bindless.use_heap = DXIL_SPV_TRUE;
            return true;
        }
        // Legacy bindful
        else {
            uint32_t slot, set;
            if(FindVkBindingSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                binding.register_space,
                                binding.register_index,
                                set, slot))
            {
                vk_binding.bindless.use_heap = DXIL_SPV_FALSE;
                vk_binding.set = set;
                vk_binding.binding = slot;
                return true;
            }
            return false;
        }
    }


    bool SPVRemapper::RemapCBV( const dxil_spv_d3d_binding& binding,
                            dxil_spv_cbv_vulkan_binding& vk_binding
    ) {
        // shouldn't trigger remapping when no layout provided
        if(!layout) {
            assert(false && "Can't remap a shader using pipelines that have no resource layout");
            return false;
        }

        // should only contain values
        assert(binding.kind == DXIL_SPV_RESOURCE_KIND_CONSTANT_BUFFER);
        // Lookup push constants first
        const auto& pushConstantRemappings = layout->GetPushConstants();

        /* Try to map to root constant -> push constant. */
        for (auto& push : pushConstantRemappings)
        {
            if (push.bindingSpace == binding.register_space &&
                push.bindingSlot == binding.register_index
                ///#TODO: Shader visibility
                //&& dxil_match_shader_visibility(push.shaderVisibility, binding.stage)
            ) {
                vk_binding = {};
                vk_binding.push_constant = DXIL_SPV_TRUE;
                vk_binding.vulkan.push_constant.offset_in_words = push.offsetInDwords;
                return DXIL_SPV_TRUE;
            }
        }

        vk_binding.push_constant = DXIL_SPV_FALSE;

        // T2 bindless
        if(layout->GetDesc().useGlobalHeaps) {
            vk_binding.vulkan.uniform_binding.set = 0;
            vk_binding.vulkan.uniform_binding.binding = 0;
            vk_binding.vulkan.uniform_binding.bindless.use_heap = DXIL_SPV_TRUE;

            return true;
        }
        // legacy bindful
        else {
            vk_binding.vulkan.uniform_binding.bindless.use_heap = DXIL_SPV_FALSE;

            uint32_t set, slot;
            if(FindVkBindingSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                binding.register_space,
                                binding.register_index,
                                set, slot))
            {
                vk_binding.vulkan.uniform_binding.set = set;
                vk_binding.vulkan.uniform_binding.binding = slot;
                return true;
            }
            return false;
        }
    }

        
    bool SPVRemapper::RemapVertexInput(
        const dxil_spv_d3d_vertex_input& d3dIn,
        dxil_spv_vulkan_vertex_input& vkOut
    ) {
        if(!iaMappings) {
            assert(false && "IA input isn't supported by this pipeline type!");
            return false;
        }
        VertexInputSemantic d3dSemantic {};
        VertexInputSemantic::Name d3dSemanticName;
        if(!Str2Semantic(d3dIn.semantic, d3dSemanticName))
            return false;

        d3dSemantic.name = d3dSemanticName;
        d3dSemantic.slot = d3dIn.semantic_index;

        auto findRes = iaMappings->find(d3dSemantic);
        if(findRes == iaMappings->end()) {
            assert(false && "IA input element not found");
            return false;
        }
        vkOut.location = findRes->second;

        return true;
    }

    
    bool SPVRemapper::RemapShaderStageInput( const dxil_spv_d3d_shader_stage_io& d3d_input,
                                    dxil_spv_vulkan_shader_stage_io& vulkan_variable
    ) {
        if(currentStage == IShader::Stage::Vertex) return true;
        //auto io_map = (const vkd3d_shader_stage_io_map *)userdata;
        //const vkd3d_shader_stage_io_entry *e;

        //if (!(e = io_map->Find(d3d_input->semantic, d3d_input->semantic_index)))

        
        std::string semanticName = d3d_input.semantic;
        
        auto key = std::format("{}_{}", semanticName, d3d_input.semantic_index);
        auto iter = shaderStageIoMap.find(key);
        
        if(iter == shaderStageIoMap.end())
        {
            //ERR("Undefined semantic %s (%u).\n", d3d_input->semantic, d3d_input->semantic_index);
            assert(false);
            return false;
        }

        vulkan_variable.location = iter->second.vk_location;
        vulkan_variable.component = iter->second.vk_component;
        vulkan_variable.flags = iter->second.vk_flags;
        return true;
    }

    bool SPVRemapper::CaptureShaderStageOutput( const dxil_spv_d3d_shader_stage_io& d3d_input,
                                    dxil_spv_vulkan_shader_stage_io& vulkan_variable
    ) {
        //auto io_map = (struct vkd3d_shader_stage_io_map *)userdata;
        //struct vkd3d_shader_stage_io_entry *e;

        std::string semanticName = d3d_input.semantic;

        auto key = std::format("{}_{}", semanticName, d3d_input.semantic_index);

        if(shaderStageIoMap.find(key) != shaderStageIoMap.end())
        //if (!(e = io_map->Append(d3d_input.semantic, d3d_input.semantic_index)))
        {
            //ERR("Duplicate semantic %s (%u).\n", d3d_input.semantic, d3d_input.semantic_index);

            assert(false);
            return false;
        }

        ShaderStageIOInfo e{
            .semanticName  = semanticName,
            .semanticIndex = d3d_input.semantic_index,
            .vk_location   = vulkan_variable.location,
            .vk_component  = vulkan_variable.component,
            .vk_flags      = vulkan_variable.flags
        };

        shaderStageIoMap.insert({key, e});

        return true;
    }


    dxil_spv_bool SPVRemapper_RemapSRV(void *userdata, 
                                    const dxil_spv_d3d_binding *binding,
                                    dxil_spv_srv_vulkan_binding *vk_binding
    ) {
        auto self = (SPVRemapper*)userdata;
        return self->RemapSRV(*binding, *vk_binding) ? DXIL_SPV_TRUE : DXIL_SPV_FALSE;
    }

    dxil_spv_bool SPVRemapper_RemapSampler(void *userdata,
                                        const dxil_spv_d3d_binding *binding,
                                        dxil_spv_vulkan_binding *vk_binding
    ) {
        auto self = (SPVRemapper*)userdata;
        return self->RemapSampler(*binding, *vk_binding) ? DXIL_SPV_TRUE : DXIL_SPV_FALSE;
    }

    dxil_spv_bool SPVRemapper_RemapUAV (void *userdata,
                                    const dxil_spv_uav_d3d_binding *binding,
                                    dxil_spv_uav_vulkan_binding *vk_binding
    ) {
        auto self = (SPVRemapper*)userdata;
        return self->RemapUAV(*binding, *vk_binding) ? DXIL_SPV_TRUE : DXIL_SPV_FALSE;
    }

    dxil_spv_bool SPVRemapper_RemapCBV(void *userdata,
                                    const dxil_spv_d3d_binding *binding,
                                    dxil_spv_cbv_vulkan_binding *vk_binding
    ) {
        auto self = (SPVRemapper*)userdata;
        return self->RemapCBV(*binding, *vk_binding) ? DXIL_SPV_TRUE : DXIL_SPV_FALSE;
    }

    dxil_spv_bool SPVRemapper_RemapVertexInput(void *userdata, 
                                            const dxil_spv_d3d_vertex_input *d3d_input,
                                            dxil_spv_vulkan_vertex_input *vk_input
    ) {
        auto self = (SPVRemapper*)userdata;
        return self->RemapVertexInput(*d3d_input, *vk_input) ? DXIL_SPV_TRUE : DXIL_SPV_FALSE;
    }
    dxil_spv_bool SPVRemapper_RemapShaderStageInput(void *userdata, const dxil_spv_d3d_shader_stage_io *d3d_input,
                                                            dxil_spv_vulkan_shader_stage_io *vulkan_variable)
    {
        auto self = (SPVRemapper*)userdata;
        return self->RemapShaderStageInput(*d3d_input, *vulkan_variable) ? DXIL_SPV_TRUE : DXIL_SPV_FALSE;
    }
    dxil_spv_bool SPVRemapper_CaptureShaderStageOutput(void *userdata, const dxil_spv_d3d_shader_stage_io *d3d_input,
                                                            dxil_spv_vulkan_shader_stage_io *vulkan_variable)
    {
        auto self = (SPVRemapper*)userdata;
        return self->CaptureShaderStageOutput(*d3d_input, *vulkan_variable) ? DXIL_SPV_TRUE : DXIL_SPV_FALSE;
    }

    struct ThreadAllocatorGuard {
        ThreadAllocatorGuard(){ dxil_spv_begin_thread_allocator_context(); }
        ~ThreadAllocatorGuard(){ dxil_spv_end_thread_allocator_context(); }
    };

    struct ParsedBlobGuard {
        dxil_spv_parsed_blob blob = nullptr;
        ParsedBlobGuard(){ }
        ~ParsedBlobGuard(){ if(blob) dxil_spv_parsed_blob_free(blob); }
    };

    struct ConverterGuard {
        dxil_spv_converter converter = nullptr;
        ConverterGuard(){ }
        ~ConverterGuard(){ if(converter) dxil_spv_converter_free(converter); }
    };

    
    static bool dxil_match_shader_stage(dxil_spv_shader_stage blob_stage, VkShaderStageFlagBits expected)
    {
        VkShaderStageFlagBits stage;

        switch (blob_stage)
        {
            case DXIL_SPV_STAGE_VERTEX: stage = VK_SHADER_STAGE_VERTEX_BIT; break;
            case DXIL_SPV_STAGE_HULL: stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; break;
            case DXIL_SPV_STAGE_DOMAIN: stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; break;
            case DXIL_SPV_STAGE_GEOMETRY: stage = VK_SHADER_STAGE_GEOMETRY_BIT; break;
            case DXIL_SPV_STAGE_PIXEL: stage = VK_SHADER_STAGE_FRAGMENT_BIT; break;
            case DXIL_SPV_STAGE_COMPUTE: stage = VK_SHADER_STAGE_COMPUTE_BIT; break;
            case DXIL_SPV_STAGE_AMPLIFICATION: stage = VK_SHADER_STAGE_TASK_BIT_EXT; break;
            case DXIL_SPV_STAGE_MESH: stage = VK_SHADER_STAGE_MESH_BIT_EXT; break;
            default: return false;
        }

        if (stage != expected)
        {
            //ERR("Expected VkShaderStage #%x, but got VkShaderStage #%x.\n", expected, stage);
            return false;
        }

        return true;
    }
    
    ShaderConverterResult DXIL2SPV(VulkanDevice& device,
                                   const std::span<uint8_t>& dxil,
                                   const ConverterCompilerArgs& compiler_args,
                                   SPVRemapper& remapper,
                                   SPIRVBlob& spirv) {
        
        const auto& devLimit = device.GetAdapter().GetAdapterInfo().limits;
        
        //dxil_spv_converter converter = nullptr;
        //dxil_spv_parsed_blob blob = nullptr;

        ShaderConverterResult ret = Success;
        ThreadAllocatorGuard{};//dxil_spv_begin_thread_allocator_context();
        ParsedBlobGuard blob {};
        ConverterGuard converter {};

        do {

            if (dxil_spv_parse_dxil_blob(dxil.data(), dxil.size(), &blob.blob) != DXIL_SPV_SUCCESS)
            {
                ret = VKD3D_ERROR_INVALID_SHADER;
                assert(false);
                break;
            }

            auto stage = dxil_spv_parsed_blob_get_shader_stage(blob.blob);
            //if (!dxil_match_shader_stage(stage, shader_interface_info->stage))
            if (!dxil_match_shader_stage(stage, compiler_args.shaderStage))
            {
                ret = VKD3D_ERROR_INVALID_ARGUMENT;
                assert(false);
                break;
            }

            if (dxil_spv_create_converter(blob.blob, &converter.converter) != DXIL_SPV_SUCCESS)
            {
                ret = VKD3D_ERROR_INVALID_ARGUMENT;
                assert(false);
                break;
            }

            dxil_spv_option_compute_shader_derivatives compute_shader_derivatives = {{ DXIL_SPV_OPTION_COMPUTE_SHADER_DERIVATIVES }};

            //else if (e == VKD3D_SHADER_TARGET_EXTENSION_COMPUTE_SHADER_DERIVATIVES_NV)
            //{
            //    compute_shader_derivatives.supports_nv = DXIL_SPV_TRUE;
            //}
            //else if (e == VKD3D_SHADER_TARGET_EXTENSION_COMPUTE_SHADER_DERIVATIVES_KHR)
            //{
            //    compute_shader_derivatives.supports_khr = DXIL_SPV_TRUE;
            //}

             /* For legacy reasons, COMPUTE_SHADER_DERIVATIVES_NV is default true in dxil-spirv,
             * so we have to override it to false as needed. */
            if (dxil_spv_converter_add_option(converter.converter, &compute_shader_derivatives.base) != DXIL_SPV_SUCCESS)
            {
                //ERR("dxil-spirv does not support COMPUTE_SHADER_DERIVATIVES.\n");
                ret = VKD3D_ERROR_NOT_IMPLEMENTED;
                assert(false);
                break;
            }

            dxil_spv_option_ssbo_alignment ssbo_alignment {
                .base = { DXIL_SPV_OPTION_SSBO_ALIGNMENT },
                .alignment = (uint32_t)devLimit.minStructuredBufferStride
            };

            if (dxil_spv_converter_add_option(converter.converter, &ssbo_alignment.base) != DXIL_SPV_SUCCESS)
            {
                //ERR("dxil-spirv does not support ssbo_alignment.\n");
                ret = VKD3D_ERROR_NOT_IMPLEMENTED;
                assert(false);
                break;
            }

            //remap_userdata.shader_interface_info = shader_interface_info;
            //remap_userdata.shader_interface_local_info = NULL;
            //remap_userdata.num_root_descriptors = num_root_descriptors;

            
            dxil_spv_converter_set_entry_point(converter.converter, compiler_args.entryPoint.c_str());

            uint32_t root_constant_words = compiler_args.root_constant_words;
            uint32_t num_root_descriptors = 0; /*One for srv_uav_cbv_combined, one for samplers*/

            dxil_spv_converter_set_root_constant_word_count(converter.converter, root_constant_words);
            //dxil_spv_converter_set_root_descriptor_count(converter.converter, num_root_descriptors);
            dxil_spv_converter_set_srv_remapper(converter.converter, SPVRemapper_RemapSRV, &remapper);
            dxil_spv_converter_set_sampler_remapper(converter.converter, SPVRemapper_RemapSampler, &remapper);
            dxil_spv_converter_set_uav_remapper(converter.converter, SPVRemapper_RemapUAV, &remapper);
            dxil_spv_converter_set_cbv_remapper(converter.converter, SPVRemapper_RemapCBV, &remapper);

            dxil_spv_converter_set_vertex_input_remapper(converter.converter, SPVRemapper_RemapVertexInput, &remapper);

            ///#TODO: support stream output?
            //if (shader_interface_info->xfb_info)
            //    dxil_spv_converter_set_stream_output_remapper(converter, dxil_output_remap, (void *)shader_interface_info->xfb_info);

            //if (shader_interface_info->stage_input_map)
                dxil_spv_converter_set_stage_input_remapper(converter.converter, SPVRemapper_RemapShaderStageInput, &remapper);

            //if (shader_interface_info->stage_output_map)
                dxil_spv_converter_set_stage_output_remapper(converter.converter, SPVRemapper_CaptureShaderStageOutput, &remapper);

            if (dxil_spv_converter_run(converter.converter) != DXIL_SPV_SUCCESS)
            {
                ret = VKD3D_ERROR_INVALID_ARGUMENT;
                assert(false);
                break;
            }


		    dxil_spv_compiled_spirv compiled;
            if (dxil_spv_converter_get_compiled_spirv(converter.converter, &compiled) != DXIL_SPV_SUCCESS)
            {
                ret = VKD3D_ERROR_INVALID_ARGUMENT;
                assert(false);
                break;
            }

            /* Late replacement for mesh shaders. */
            //if (shader_interface_info->stage == VK_SHADER_STAGE_MESH_BIT_EXT &&
            //        vkd3d_shader_replace(hash, &spirv->code, &spirv->size))
            //{
            //    spirv->meta.flags |= VKD3D_SHADER_META_FLAG_REPLACED;
            //}
            //else
            {
                spirv.code.resize(compiled.size);
                memcpy(spirv.code.data(), compiled.data, compiled.size);
            }

            dxil_spv_converter_get_compute_workgroup_dimensions(converter.converter,
                    &spirv.meta.cs_workgroup_size[0],
                    &spirv.meta.cs_workgroup_size[1],
                    &spirv.meta.cs_workgroup_size[2]);
            dxil_spv_converter_get_patch_vertex_count(converter.converter, &spirv.meta.patch_vertex_count);

            uint32_t wave_size_min, wave_size_max, wave_size_preferred;
            dxil_spv_converter_get_compute_wave_size_range(converter.converter,
                    &wave_size_min, &wave_size_max, &wave_size_preferred);

            /* Ensure that the maximum wave size is always valid */
            if (!wave_size_max)
                wave_size_max = wave_size_min;

            if (compiler_args.promote_wave_size_heuristics)
            {

                unsigned int heuristic_wave_size;
                dxil_spv_converter_get_compute_heuristic_max_wave_size(converter.converter, &heuristic_wave_size);
                //if (quirks & VKD3D_SHADER_QUIRK_FORCE_MAX_WAVE32)
                //    heuristic_wave_size = 32;

                if (heuristic_wave_size && !wave_size_min &&
                        compiler_args.max_subgroup_size > heuristic_wave_size &&
                        compiler_args.min_subgroup_size <= heuristic_wave_size)
                    wave_size_preferred = heuristic_wave_size;
            }

            spirv.meta.cs_wave_size_min = wave_size_min;
            spirv.meta.cs_wave_size_max = wave_size_max;
            spirv.meta.cs_wave_size_preferred = wave_size_preferred;

        } while(0);

        //Do we really need this?
        //vkd3d_shader_extract_feature_meta(spirv);

        //vkd3d_shader_dump_spirv_shader(hash, spirv);

    //end:
        //dxil_spv_converter_free(converter);
        //dxil_spv_parsed_blob_free(blob);
        //dxil_spv_end_thread_allocator_context();
        return ret;
    }


    

    VulkanShader::~VulkanShader(){
        //vkDestroyShaderModule(_Dev()->LogicalDev(), _shaderModule, nullptr);
    }

    common::sp<IShader> VulkanShader::Make(
        const common::sp<VulkanDevice>& dev,
        const IShader::Description& desc,
        const std::span<const std::uint8_t>& il
    ){
        //VkShaderModuleCreateInfo shaderModuleCI {};
        //shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        //shaderModuleCI.codeSize = il.size();
        //shaderModuleCI.pCode = (const uint32_t*)il.data();
        //VkShaderModule module;
        //VK_CHECK(vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &module));

        auto shader = new VulkanShader(dev, desc, il);
        //shader->_shaderModule = module;

        return common::sp<IShader>(shader);
    }

} // namespace alloy::vk
