#include "VulkanShader.hpp"

#include "VulkanDevice.hpp"
#include "VkCommon.hpp"

#include <format>


namespace alloy::vk
{
    
    
        bool SPVRemapper::RemapSRV( const dxil_spv_d3d_binding& binding,
                              dxil_spv_srv_vulkan_binding& vk_binding
        ) {
            vk_binding.buffer_binding.bindless.use_heap = false;
			vk_binding.buffer_binding.set = binding.register_space;
			vk_binding.buffer_binding.binding = binding.register_index;
			//vk_binding.buffer_binding.descriptor_type = VulkanDescriptorType::Identity;
			vk_binding.offset_binding = {};
			return true;
        }

        bool SPVRemapper::RemapSampler( const dxil_spv_d3d_binding& binding,
                                  dxil_spv_vulkan_binding& vk_binding
        ) {
            vk_binding.bindless.use_heap = DXIL_SPV_FALSE;
		    vk_binding.set = binding.register_space;
		    vk_binding.binding = binding.register_index;
			return true;
        }

        bool SPVRemapper::RemapUAV ( const dxil_spv_uav_d3d_binding& binding,
                               dxil_spv_uav_vulkan_binding& vk_binding
        ) {
            vk_binding.buffer_binding.bindless.use_heap = DXIL_SPV_FALSE;
		    vk_binding.buffer_binding.set = binding.d3d_binding.register_space;
		    vk_binding.buffer_binding.binding = binding.d3d_binding.register_index;
			return true;
        }

        bool SPVRemapper::RemapCBV( const dxil_spv_d3d_binding& binding,
                              dxil_spv_cbv_vulkan_binding& vk_binding
        ) {
            vk_binding.vulkan.uniform_binding.bindless.use_heap = DXIL_SPV_FALSE;
            vk_binding.vulkan.uniform_binding.set = binding.register_space;
            vk_binding.vulkan.uniform_binding.binding = binding.register_index;
            return true;
        }
        
        bool SPVRemapper::RemapVertexInput( const dxil_spv_d3d_vertex_input& d3d_input,
                                      dxil_spv_vulkan_vertex_input& vk_input
        ) {

            vk_input.location = d3d_input.start_row;
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
    
    ShaderConverterResult DXIL2SPV(const std::span<uint8_t>& dxil,
                                   const ConverterCompilerArgs& compiler_args,
                                   SPVRemapper& remapper,
                                   SPIRVBlob& spirv) {
        
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
                break;
            }

            auto stage = dxil_spv_parsed_blob_get_shader_stage(blob.blob);
            //if (!dxil_match_shader_stage(stage, shader_interface_info->stage))
            if (!dxil_match_shader_stage(stage, compiler_args.shaderStage))
            {
                ret = VKD3D_ERROR_INVALID_ARGUMENT;
                break;
            }

            if (dxil_spv_create_converter(blob.blob, &converter.converter) != DXIL_SPV_SUCCESS)
            {
                ret = VKD3D_ERROR_INVALID_ARGUMENT;
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
                break;
            }

            //remap_userdata.shader_interface_info = shader_interface_info;
            //remap_userdata.shader_interface_local_info = NULL;
            //remap_userdata.num_root_descriptors = num_root_descriptors;

            
            dxil_spv_converter_set_entry_point(converter.converter, compiler_args.entryPoint.c_str());

            uint32_t root_constant_words = 0;
            uint32_t num_root_descriptors = 0; /*One for srv_uav_cbv_combined, one for samplers*/

            //dxil_spv_converter_set_root_constant_word_count(converter.converter, root_constant_words);
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
                break;
            }


		    dxil_spv_compiled_spirv compiled;
            if (dxil_spv_converter_get_compiled_spirv(converter.converter, &compiled) != DXIL_SPV_SUCCESS)
            {
                ret = VKD3D_ERROR_INVALID_ARGUMENT;
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
        const std::span<std::uint8_t>& il
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
