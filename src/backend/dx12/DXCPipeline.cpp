#include "DXCPipeline.hpp"

//3rd-party headers

//veldrid public headers

//standard library headers
#include <string>
#include <cassert>

//backend specific headers

//platform specific headers

//Local headers
#include "DXCDevice.hpp"
#include "D3DTypeCvt.hpp"

#include "spirv/nir_spirv.h"
#include "nir.h"
#include "nir_xfb_info.h"

#include "dxil_nir.h"
#include "nir_to_dxil.h"
#include "dxil_spirv_nir.h"
#include "spirv_to_dxil.h"
#include "dxil_validator.h"


using Microsoft::WRL::ComPtr;


namespace Veldrid::VK::priv
{
   struct dzn_nir_options {
      enum dxil_spirv_yz_flip_mode yz_flip_mode;
      uint16_t y_flip_mask, z_flip_mask;
      bool force_sample_rate_shading;
      bool lower_view_index;
      bool lower_view_index_to_rt_layer;
      enum pipe_format *vi_conversions;
      const nir_shader_compiler_options *nir_opts;
      enum gl_subgroup_size subgroup_size;
   };

   gl_shader_stage VdToMesaShaderStage(const Shader::Description::Stage& stage){

      if(stage.vertex) return gl_shader_stage::MESA_SHADER_VERTEX;
      if(stage.geometry) return gl_shader_stage::MESA_SHADER_GEOMETRY;
      if(stage.tessellationControl) return gl_shader_stage::MESA_SHADER_TESS_CTRL;
      if(stage.tessellationEvaluation) return gl_shader_stage::MESA_SHADER_TESS_EVAL;
      if(stage.fragment) return gl_shader_stage::MESA_SHADER_FRAGMENT;
      if(stage.compute) return gl_shader_stage::MESA_SHADER_COMPUTE;


      return MESA_SHADER_NONE;
   }


struct nir_spirv_specialization*
vk_spec_info_to_nir_spirv(const VkSpecializationInfo *spec_info,
                          uint32_t *out_num_spec_entries)
{
   if (spec_info == NULL || spec_info->mapEntryCount == 0)
      return NULL;

   uint32_t num_spec_entries = spec_info->mapEntryCount;
   struct nir_spirv_specialization *spec_entries =
      calloc(num_spec_entries, sizeof(*spec_entries));

   for (uint32_t i = 0; i < num_spec_entries; i++) {
      VkSpecializationMapEntry entry = spec_info->pMapEntries[i];
      const void *data = (uint8_t *)spec_info->pData + entry.offset;
      assert((uint8_t *)data + entry.size <=
             (uint8_t *)spec_info->pData + spec_info->dataSize);

      spec_entries[i].id = spec_info->pMapEntries[i].constantID;
      switch (entry.size) {
      case 8:
         spec_entries[i].value.u64 = *(const uint64_t *)data;
         break;
      case 4:
         spec_entries[i].value.u32 = *(const uint32_t *)data;
         break;
      case 2:
         spec_entries[i].value.u16 = *(const uint16_t *)data;
         break;
      case 1:
         spec_entries[i].value.u8 = *(const uint8_t *)data;
         break;
      case 0:
      default:
         /* The Vulkan spec says:
          *
          *    "For a constantID specialization constant declared in a
          *    shader, size must match the byte size of the constantID. If
          *    the specialization constant is of type boolean, size must be
          *    the byte size of VkBool32."
          *
          * Therefore, since only scalars can be decorated as
          * specialization constants, we can assume that if it doesn't have
          * a size of 1, 2, 4, or 8, any use in a shader would be invalid
          * usage.  The spec further says:
          *
          *    "If a constantID value is not a specialization constant ID
          *    used in the shader, that map entry does not affect the
          *    behavior of the pipeline."
          *
          * so we should ignore any invalid specialization constants rather
          * than crash or error out when we see one.
          */
         break;
      }
   }

   *out_num_spec_entries = num_spec_entries;
   return spec_entries;
}

#define SPIR_V_MAGIC_NUMBER 0x07230203

uint32_t
vk_spirv_version(const uint32_t *spirv_data, size_t spirv_size_B)
{
   assert(spirv_size_B >= 8);
   assert(spirv_data[0] == SPIR_V_MAGIC_NUMBER);
   return spirv_data[1];
}

static void
spirv_nir_debug(void *private_data,
                enum nir_spirv_debug_level level,
                size_t spirv_offset,
                const char *message)
{
   const struct vk_object_base *log_obj = private_data;

   switch (level) {
   case NIR_SPIRV_DEBUG_LEVEL_INFO:
      //vk_logi(VK_LOG_OBJS(log_obj), "SPIR-V offset %lu: %s",
      //        (unsigned long) spirv_offset, message);
      break;
   case NIR_SPIRV_DEBUG_LEVEL_WARNING:
      vk_logw(VK_LOG_OBJS(log_obj), "SPIR-V offset %lu: %s",
              (unsigned long) spirv_offset, message);
      break;
   case NIR_SPIRV_DEBUG_LEVEL_ERROR:
      vk_loge(VK_LOG_OBJS(log_obj), "SPIR-V offset %lu: %s",
              (unsigned long) spirv_offset, message);
      break;
   default:
      break;
   }
}


static bool
is_not_xfb_output(nir_variable *var, void *data)
{
   if (var->data.mode != nir_var_shader_out)
      return true;

   return !var->data.explicit_xfb_buffer &&
          !var->data.explicit_xfb_stride;
}

nir_shader *
vk_spirv_to_nir(struct vk_device *device,
                const uint32_t *spirv_data, size_t spirv_size_B,
                gl_shader_stage stage, const char *entrypoint_name,
                enum gl_subgroup_size subgroup_size,
                const VkSpecializationInfo *spec_info,
                const struct spirv_to_nir_options *spirv_options,
                const struct nir_shader_compiler_options *nir_options,
                void *mem_ctx)
{
   assert(spirv_size_B >= 4 && spirv_size_B % 4 == 0);
   assert(spirv_data[0] == SPIR_V_MAGIC_NUMBER);

   struct spirv_to_nir_options spirv_options_local = *spirv_options;
   spirv_options_local.debug.func = spirv_nir_debug;
   spirv_options_local.debug.private_data = (void *)device;
   spirv_options_local.subgroup_size = subgroup_size;

   uint32_t num_spec_entries = 0;
   struct nir_spirv_specialization *spec_entries =
      vk_spec_info_to_nir_spirv(spec_info, &num_spec_entries);

   nir_shader *nir = spirv_to_nir(spirv_data, spirv_size_B / 4,
                                  spec_entries, num_spec_entries,
                                  stage, entrypoint_name,
                                  &spirv_options_local, nir_options);
   free(spec_entries);

   if (nir == NULL)
      return NULL;

   assert(nir->info.stage == stage);
   nir_validate_shader(nir, "after spirv_to_nir");
   nir_validate_ssa_dominance(nir, "after spirv_to_nir");
   if (mem_ctx != NULL)
      ralloc_steal(mem_ctx, nir);

   /* We have to lower away local constant initializers right before we
    * inline functions.  That way they get properly initialized at the top
    * of the function and not at the top of its caller.
    */
   NIR_PASS_V(nir, nir_lower_variable_initializers, nir_var_function_temp);
   NIR_PASS_V(nir, nir_lower_returns);
   NIR_PASS_V(nir, nir_inline_functions);
   NIR_PASS_V(nir, nir_copy_prop);
   NIR_PASS_V(nir, nir_opt_deref);

   /* Pick off the single entrypoint that we want */
   nir_remove_non_entrypoints(nir);

   /* Now that we've deleted all but the main function, we can go ahead and
    * lower the rest of the constant initializers.  We do this here so that
    * nir_remove_dead_variables and split_per_member_structs below see the
    * corresponding stores.
    */
   NIR_PASS_V(nir, nir_lower_variable_initializers, (nir_variable_mode)~0);

   /* Split member structs.  We do this before lower_io_to_temporaries so that
    * it doesn't lower system values to temporaries by accident.
    */
   NIR_PASS_V(nir, nir_split_var_copies);
   NIR_PASS_V(nir, nir_split_per_member_structs);

   nir_remove_dead_variables_options dead_vars_opts {};
   {
      dead_vars_opts.can_remove_var = &is_not_xfb_output;
   };
   NIR_PASS_V(nir, nir_remove_dead_variables,
              nir_var_shader_in | nir_var_shader_out | nir_var_system_value |
              nir_var_shader_call_data | nir_var_ray_hit_attrib,
              &dead_vars_opts);

   /* This needs to happen after remove_dead_vars because GLSLang likes to
    * insert dead clip/cull vars and we don't want to clip/cull based on
    * uninitialized garbage.
    */
   NIR_PASS_V(nir, nir_lower_clip_cull_distance_arrays);

   if (nir->info.stage == MESA_SHADER_VERTEX ||
       nir->info.stage == MESA_SHADER_TESS_EVAL ||
       nir->info.stage == MESA_SHADER_GEOMETRY)
      NIR_PASS_V(nir, nir_shader_gather_xfb_info);

   NIR_PASS_V(nir, nir_propagate_invariant, false);

   return nir;
}


VkResult
vk_pipeline_shader_stage_to_nir(struct vk_device *device,
                                const VkPipelineShaderStageCreateInfo *info,
                                const struct spirv_to_nir_options *spirv_options,
                                const struct nir_shader_compiler_options *nir_options,
                                void *mem_ctx, nir_shader **nir_out)
{
   VK_FROM_HANDLE(vk_shader_module, module, info->module);
   const gl_shader_stage stage = vk_to_mesa_shader_stage(info->stage);

   assert(info->sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);

   if (module != NULL && module->nir != NULL) {
      assert(module->nir->info.stage == stage);
      assert(exec_list_length(&module->nir->functions) == 1);
      ASSERTED const char *nir_name =
         nir_shader_get_entrypoint(module->nir)->function->name;
      assert(strcmp(nir_name, info->pName) == 0);

      nir_validate_shader(module->nir, "internal shader");

      nir_shader *clone = nir_shader_clone(mem_ctx, module->nir);
      if (clone == NULL)
         return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

      assert(clone->options == NULL || clone->options == nir_options);
      clone->options = nir_options;

      *nir_out = clone;
      return VK_SUCCESS;
   }

   const uint32_t *spirv_data;
   uint32_t spirv_size;
   if (module != NULL) {
      spirv_data = (uint32_t *)module->data;
      spirv_size = module->size;
   } else {
      const VkShaderModuleCreateInfo *minfo =
         vk_find_struct_const(info->pNext, SHADER_MODULE_CREATE_INFO);
      if (unlikely(minfo == NULL)) {
         return vk_errorf(device, VK_ERROR_UNKNOWN,
                          "No shader module provided");
      }
      spirv_data = minfo->pCode;
      spirv_size = minfo->codeSize;
   }

   enum gl_subgroup_size subgroup_size;
   uint32_t req_subgroup_size = get_required_subgroup_size(info);
   if (req_subgroup_size > 0) {
      assert(util_is_power_of_two_nonzero(req_subgroup_size));
      assert(req_subgroup_size >= 8 && req_subgroup_size <= 128);
      subgroup_size = req_subgroup_size;
   } else if (info->flags & VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT ||
              vk_spirv_version(spirv_data, spirv_size) >= 0x10600) {
      /* Starting with SPIR-V 1.6, varying subgroup size the default */
      subgroup_size = SUBGROUP_SIZE_VARYING;
   } else if (info->flags & VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT) {
      assert(stage == MESA_SHADER_COMPUTE);
      subgroup_size = SUBGROUP_SIZE_FULL_SUBGROUPS;
   } else {
      subgroup_size = SUBGROUP_SIZE_API_CONSTANT;
   }

   nir_shader *nir = vk_spirv_to_nir(device, spirv_data, spirv_size, stage,
                                     info->pName, subgroup_size,
                                     info->pSpecializationInfo,
                                     spirv_options, nir_options, mem_ctx);
   if (nir == NULL)
      return vk_errorf(device, VK_ERROR_UNKNOWN, "spirv_to_nir failed");

   *nir_out = nir;

   return VK_SUCCESS;
}


VkResult
vk_shader_module_to_nir(struct vk_device *device,
                        const struct vk_shader_module *mod,
                        gl_shader_stage stage,
                        const char *entrypoint_name,
                        const VkSpecializationInfo *spec_info,
                        const struct spirv_to_nir_options *spirv_options,
                        const nir_shader_compiler_options *nir_options,
                        void *mem_ctx, nir_shader **nir_out)
{
   const VkPipelineShaderStageCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = mesa_to_vk_shader_stage(stage),
      .module = vk_shader_module_to_handle((struct vk_shader_module *)mod),
      .pName = entrypoint_name,
      .pSpecializationInfo = spec_info,
   };
   return vk_pipeline_shader_stage_to_nir(device, &info,
                                          spirv_options, nir_options,
                                          mem_ctx, nir_out);
}

static VkResult
dzn_pipeline_get_nir_shader(struct dzn_device *device,
                            const struct dzn_pipeline_layout *layout,
                            struct vk_pipeline_cache *cache,
                            const uint8_t *hash,
                            const VkPipelineShaderStageCreateInfo *stage_info,
                            gl_shader_stage stage,
                            const struct dzn_nir_options *options,
                            nir_shader **nir)
{
   if (cache) {
      *nir = vk_pipeline_cache_lookup_nir(cache, hash, SHA1_DIGEST_LENGTH,
                                          options->nir_opts, NULL, NULL);
       if (*nir)
          return VK_SUCCESS;
   }

   struct dzn_physical_device *pdev =
      container_of(device->vk.physical, struct dzn_physical_device, vk);
   VK_FROM_HANDLE(vk_shader_module, module, stage_info->module);
   const struct spirv_to_nir_options *spirv_opts = dxil_spirv_nir_get_spirv_options();

   VkResult result =
      vk_shader_module_to_nir(&device->vk, module, stage,
                              stage_info->pName, stage_info->pSpecializationInfo,
                              spirv_opts, options->nir_opts, NULL, nir);
   if (result != VK_SUCCESS)
      return result;

   struct dxil_spirv_runtime_conf conf = {
      .runtime_data_cbv = {
         .register_space = DZN_REGISTER_SPACE_SYSVALS,
         .base_shader_register = 0,
      },
      .push_constant_cbv = {
         .register_space = DZN_REGISTER_SPACE_PUSH_CONSTANT,
         .base_shader_register = 0,
      },
      .zero_based_vertex_instance_id = false,
      .zero_based_compute_workgroup_id = false,
      .yz_flip = {
         .mode = options->yz_flip_mode,
         .y_mask = options->y_flip_mask,
         .z_mask = options->z_flip_mask,
      },
      .declared_read_only_images_as_srvs = !device->bindless,
      .inferred_read_only_images_as_srvs = !device->bindless,
      .force_sample_rate_shading = options->force_sample_rate_shading,
      .lower_view_index = options->lower_view_index,
      .lower_view_index_to_rt_layer = options->lower_view_index_to_rt_layer,
      .shader_model_max = dzn_get_shader_model(pdev),
   };

   bool requires_runtime_data;
   dxil_spirv_nir_passes(*nir, &conf, &requires_runtime_data);

   if (stage == MESA_SHADER_VERTEX) {
      bool needs_conv = false;
      for (uint32_t i = 0; i < MAX_VERTEX_GENERIC_ATTRIBS; i++) {
         if (options->vi_conversions[i] != PIPE_FORMAT_NONE)
            needs_conv = true;
      }

      if (needs_conv)
         NIR_PASS_V(*nir, dxil_nir_lower_vs_vertex_conversion, options->vi_conversions);
   }
   (*nir)->info.subgroup_size = options->subgroup_size;

   if (cache)
      vk_pipeline_cache_add_nir(cache, hash, SHA1_DIGEST_LENGTH, *nir);

   return VK_SUCCESS;
}




static bool
adjust_var_bindings(nir_shader *shader,
                    struct dzn_device *device,
                    const struct dzn_pipeline_layout *layout,
                    uint8_t *bindings_hash)
{
   uint32_t modes = nir_var_image | nir_var_uniform | nir_var_mem_ubo | nir_var_mem_ssbo;
   struct mesa_sha1 bindings_hash_ctx;

   if (bindings_hash)
      _mesa_sha1_init(&bindings_hash_ctx);

   nir_foreach_variable_with_modes(var, shader, modes) {
      if (var->data.mode == nir_var_uniform) {
         const struct glsl_type *type = glsl_without_array(var->type);

         if (!glsl_type_is_sampler(type) && !glsl_type_is_texture(type))
            continue;
      }

      unsigned s = var->data.descriptor_set, b = var->data.binding;

      if (s >= layout->set_count)
         continue;

      assert(b < layout->binding_translation[s].binding_count);
      if (!device->bindless)
         var->data.binding = layout->binding_translation[s].base_reg[b];

      if (bindings_hash) {
         _mesa_sha1_update(&bindings_hash_ctx, &s, sizeof(s));
         _mesa_sha1_update(&bindings_hash_ctx, &b, sizeof(b));
         _mesa_sha1_update(&bindings_hash_ctx, &var->data.binding, sizeof(var->data.binding));
      }
   }

   if (bindings_hash)
      _mesa_sha1_final(&bindings_hash_ctx, bindings_hash);

   if (device->bindless) {
      struct dxil_spirv_nir_lower_bindless_options options = {
         .dynamic_buffer_binding = layout->dynamic_buffer_count ? layout->set_count : ~0,
         .num_descriptor_sets = layout->set_count,
         .callback_context = (void *)layout,
         .remap_binding = adjust_to_bindless_cb
      };
      bool ret = dxil_spirv_nir_lower_bindless(shader, &options);
      /* We skipped remapping variable bindings in the hashing loop, but if there's static
       * samplers still declared, we need to remap those now. */
      nir_foreach_variable_with_modes(var, shader, nir_var_uniform) {
         assert(glsl_type_is_sampler(glsl_without_array(var->type)));
         var->data.binding = layout->binding_translation[var->data.descriptor_set].base_reg[var->data.binding];
      }
      return ret;
   } else {
      return nir_shader_instructions_pass(shader, adjust_resource_index_binding,
                                          nir_metadata_all, (void *)layout);
   }
}


static VkResult
dzn_pipeline_compile_shader(struct dzn_device *device,
                            nir_shader *nir,
                            uint32_t input_clip_size,
                            D3D12_SHADER_BYTECODE *slot)
{
   struct dzn_instance *instance =
      container_of(device->vk.physical->instance, struct dzn_instance, vk);
   struct dzn_physical_device *pdev =
      container_of(device->vk.physical, struct dzn_physical_device, vk);
   struct nir_to_dxil_options opts = {
      .environment = DXIL_ENVIRONMENT_VULKAN,
      .lower_int16 = !pdev->options4.Native16BitShaderOpsSupported &&
      /* Don't lower 16-bit types if they can only come from min-precision */
         (device->vk.enabled_extensions.KHR_shader_float16_int8 ||
          device->vk.enabled_features.shaderFloat16 ||
          device->vk.enabled_features.shaderInt16),
      .shader_model_max = dzn_get_shader_model(pdev),
      .input_clip_size = input_clip_size,
#ifdef _WIN32
      .validator_version_max = dxil_get_validator_version(instance->dxil_validator),
#endif
   };
   struct blob dxil_blob;
   VkResult result = VK_SUCCESS;

   if (instance->debug_flags & DZN_DEBUG_NIR)
      nir_print_shader(nir, stderr);

   if (nir_to_dxil(nir, &opts, NULL, &dxil_blob)) {
      blob_finish_get_buffer(&dxil_blob, (void **)&slot->pShaderBytecode,
                             (size_t *)&slot->BytecodeLength);
   } else {
      result = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   if (dxil_blob.allocated)
      blob_finish(&dxil_blob);

   if (result != VK_SUCCESS)
      return result;

#ifdef _WIN32
   char *err;
   bool res = dxil_validate_module(instance->dxil_validator,
                                   (void *)slot->pShaderBytecode,
                                   slot->BytecodeLength, &err);

   if (instance->debug_flags & DZN_DEBUG_DXIL) {
      char *disasm = dxil_disasm_module(instance->dxil_validator,
                                        (void *)slot->pShaderBytecode,
                                        slot->BytecodeLength);
      if (disasm) {
         fprintf(stderr,
                 "== BEGIN SHADER ============================================\n"
                 "%s\n"
                 "== END SHADER ==============================================\n",
                  disasm);
         ralloc_free(disasm);
      }
   }

   if (!res) {
      if (err) {
         mesa_loge(
               "== VALIDATION ERROR =============================================\n"
               "%s\n"
               "== END ==========================================================\n",
               err);
         ralloc_free(err);
      }
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
   }
#endif

   return VK_SUCCESS;
}



bool
dzn_graphics_pipeline_compile_shaders(struct dzn_device *device,
                                      struct dzn_graphics_pipeline *pipeline,
                                      struct vk_pipeline_cache *cache,
                                      const struct dzn_pipeline_layout *layout,
                                      D3D12_PIPELINE_STATE_STREAM_DESC *out,
                                      D3D12_INPUT_ELEMENT_DESC *attribs,
                                      enum pipe_format *vi_conversions,
                                      //const VkGraphicsPipelineCreateInfo *info
                                      const GraphicsPipelineDescription &desc
                                      )
{
   struct dzn_physical_device *pdev =
      container_of(device->vk.physical, struct dzn_physical_device, vk);
   const VkPipelineViewportStateCreateInfo *vp_info =
      info->pRasterizationState->rasterizerDiscardEnable ?
      NULL : info->pViewportState;
   struct {
      //const VkPipelineShaderStageCreateInfo *info;
      const Veldrid::Shader::Description* info;
      uint8_t spirv_hash[SHA1_DIGEST_LENGTH];
      uint8_t dxil_hash[SHA1_DIGEST_LENGTH];
      uint8_t nir_hash[SHA1_DIGEST_LENGTH];
      uint8_t link_hashes[SHA1_DIGEST_LENGTH][2];
   } stages[MESA_VULKAN_SHADER_STAGES] = { 0 };
   const uint8_t *dxil_hashes[MESA_VULKAN_SHADER_STAGES] = { 0 };
   uint8_t attribs_hash[SHA1_DIGEST_LENGTH];
   uint8_t pipeline_hash[SHA1_DIGEST_LENGTH];
   gl_shader_stage last_raster_stage = MESA_SHADER_NONE;
   uint32_t active_stage_mask = 0;
   
   /* First step: collect stage info in a table indexed by gl_shader_stage
    * so we can iterate over stages in pipeline order or reverse pipeline
    * order.
    */
   for(auto i = 0; i < desc.shaderSet.shaders.size(); i++) {
   //for (uint32_t i = 0; i < info->stageCount; i++) {
      auto& shader = desc.shaderSet.shaders[i];
      auto& shaderDesc = shader->GetDesc();

      gl_shader_stage stage = VdToMesaShaderStage(shaderDesc.stage);

      assert(stage <= MESA_SHADER_FRAGMENT);

      if ((stage == MESA_SHADER_VERTEX ||
           stage == MESA_SHADER_TESS_EVAL ||
           stage == MESA_SHADER_GEOMETRY) &&
          last_raster_stage < stage)
         last_raster_stage = stage;

      //if (stage == MESA_SHADER_FRAGMENT &&
      //    info->pRasterizationState &&
      //    (info->pRasterizationState->rasterizerDiscardEnable ||
      //     info->pRasterizationState->cullMode == VK_CULL_MODE_FRONT_AND_BACK)) {
      //   /* Disable rasterization (AKA leave fragment shader NULL) when
      //    * front+back culling or discard is set.
      //    */
      //   continue;
      //}

      stages[stage].info =  &shaderDesc; //&info->pStages[i];
      active_stage_mask |= BITFIELD_BIT(stage);
   }

   pipeline->use_gs_for_polygon_mode_point =
      info->pRasterizationState &&
      info->pRasterizationState->polygonMode == VK_POLYGON_MODE_POINT &&
      !(active_stage_mask & (1 << MESA_SHADER_GEOMETRY));
   if (pipeline->use_gs_for_polygon_mode_point)
      last_raster_stage = MESA_SHADER_GEOMETRY;

   enum dxil_spirv_yz_flip_mode yz_flip_mode = DXIL_SPIRV_YZ_FLIP_NONE;
   uint16_t y_flip_mask = 0, z_flip_mask = 0;
   bool lower_view_index =
      !pipeline->multiview.native_view_instancing &&
      pipeline->multiview.view_mask > 1;

   if (pipeline->vp.dynamic) {
      yz_flip_mode = DXIL_SPIRV_YZ_FLIP_CONDITIONAL;
   } else if (vp_info) {
      for (uint32_t i = 0; vp_info->pViewports && i < vp_info->viewportCount; i++) {
         if (vp_info->pViewports[i].height > 0)
            y_flip_mask |= BITFIELD_BIT(i);

         if (vp_info->pViewports[i].minDepth > vp_info->pViewports[i].maxDepth)
            z_flip_mask |= BITFIELD_BIT(i);
      }

      if (y_flip_mask && z_flip_mask)
         yz_flip_mode = DXIL_SPIRV_YZ_FLIP_UNCONDITIONAL;
      else if (z_flip_mask)
         yz_flip_mode = DXIL_SPIRV_Z_FLIP_UNCONDITIONAL;
      else if (y_flip_mask)
         yz_flip_mode = DXIL_SPIRV_Y_FLIP_UNCONDITIONAL;
   }

   bool force_sample_rate_shading =
      !info->pRasterizationState->rasterizerDiscardEnable &&
      info->pMultisampleState &&
      info->pMultisampleState->sampleShadingEnable;

   //No caching for now.
   //if (cache) {
   //   dzn_graphics_pipeline_hash_attribs(attribs, vi_conversions, attribs_hash);
   //
   //   struct mesa_sha1 pipeline_hash_ctx;
   //
   //   _mesa_sha1_init(&pipeline_hash_ctx);
   //   _mesa_sha1_update(&pipeline_hash_ctx, &device->bindless, sizeof(device->bindless));
   //   _mesa_sha1_update(&pipeline_hash_ctx, attribs_hash, sizeof(attribs_hash));
   //   _mesa_sha1_update(&pipeline_hash_ctx, &yz_flip_mode, sizeof(yz_flip_mode));
   //   _mesa_sha1_update(&pipeline_hash_ctx, &y_flip_mask, sizeof(y_flip_mask));
   //   _mesa_sha1_update(&pipeline_hash_ctx, &z_flip_mask, sizeof(z_flip_mask));
   //   _mesa_sha1_update(&pipeline_hash_ctx, &force_sample_rate_shading, sizeof(force_sample_rate_shading));
   //   _mesa_sha1_update(&pipeline_hash_ctx, &lower_view_index, sizeof(lower_view_index));
   //   _mesa_sha1_update(&pipeline_hash_ctx, &pipeline->use_gs_for_polygon_mode_point, sizeof(pipeline->use_gs_for_polygon_mode_point));
   //
   //   u_foreach_bit(stage, active_stage_mask) {
   //      const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *subgroup_size =
   //         (const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *)
   //         vk_find_struct_const(stages[stage].info->pNext, PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO);
   //      enum gl_subgroup_size subgroup_enum = subgroup_size && subgroup_size->requiredSubgroupSize >= 8 ?
   //         subgroup_size->requiredSubgroupSize : SUBGROUP_SIZE_FULL_SUBGROUPS;
   //
   //      vk_pipeline_hash_shader_stage(stages[stage].info, NULL, stages[stage].spirv_hash);
   //      _mesa_sha1_update(&pipeline_hash_ctx, &subgroup_enum, sizeof(subgroup_enum));
   //      _mesa_sha1_update(&pipeline_hash_ctx, stages[stage].spirv_hash, sizeof(stages[stage].spirv_hash));
   //      _mesa_sha1_update(&pipeline_hash_ctx, layout->stages[stage].hash, sizeof(layout->stages[stage].hash));
   //   }
   //   _mesa_sha1_final(&pipeline_hash_ctx, pipeline_hash);
   //
   //   bool cache_hit;
   //   ret = dzn_pipeline_cache_lookup_gfx_pipeline(pipeline, cache, pipeline_hash,
   //                                                &cache_hit);
   //   if (ret != VK_SUCCESS)
   //      return ret;
   //
   //   if (cache_hit)
   //      return VK_SUCCESS;
   //}

   /* Second step: get NIR shaders for all stages. */
   nir_shader_compiler_options nir_opts;
   unsigned supported_bit_sizes = (pdev->options4.Native16BitShaderOpsSupported ? 16 : 0) | 32 | 64;
   dxil_get_nir_compiler_options(&nir_opts, dzn_get_shader_model(pdev), supported_bit_sizes, supported_bit_sizes);
   nir_opts.lower_base_vertex = true;
   u_foreach_bit(stage, active_stage_mask) {
      //struct mesa_sha1 nir_hash_ctx;

      const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *subgroup_size =
         (const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *)
         vk_find_struct_const(stages[stage].info->pNext, PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO);
      enum gl_subgroup_size subgroup_enum = subgroup_size && subgroup_size->requiredSubgroupSize >= 8 ?
         subgroup_size->requiredSubgroupSize : SUBGROUP_SIZE_FULL_SUBGROUPS;

      //if (cache) {
      //   _mesa_sha1_init(&nir_hash_ctx);
      //   _mesa_sha1_update(&nir_hash_ctx, &device->bindless, sizeof(device->bindless));
      //   if (stage != MESA_SHADER_FRAGMENT) {
      //      _mesa_sha1_update(&nir_hash_ctx, &lower_view_index, sizeof(lower_view_index));
      //      _mesa_sha1_update(&nir_hash_ctx, &force_sample_rate_shading, sizeof(force_sample_rate_shading));
      //   }
      //   if (stage == MESA_SHADER_VERTEX)
      //      _mesa_sha1_update(&nir_hash_ctx, attribs_hash, sizeof(attribs_hash));
      //   if (stage == last_raster_stage) {
      //      _mesa_sha1_update(&nir_hash_ctx, &yz_flip_mode, sizeof(yz_flip_mode));
      //      _mesa_sha1_update(&nir_hash_ctx, &y_flip_mask, sizeof(y_flip_mask));
      //      _mesa_sha1_update(&nir_hash_ctx, &z_flip_mask, sizeof(z_flip_mask));
      //      _mesa_sha1_update(&nir_hash_ctx, &lower_view_index, sizeof(lower_view_index));
      //   }
      //   _mesa_sha1_update(&nir_hash_ctx, &subgroup_enum, sizeof(subgroup_enum));
      //   _mesa_sha1_update(&nir_hash_ctx, stages[stage].spirv_hash, sizeof(stages[stage].spirv_hash));
      //   _mesa_sha1_final(&nir_hash_ctx, stages[stage].nir_hash);
      //}

      struct dzn_nir_options options = {
         .yz_flip_mode = stage == last_raster_stage ? yz_flip_mode : DXIL_SPIRV_YZ_FLIP_NONE,
         .y_flip_mask = y_flip_mask,
         .z_flip_mask = z_flip_mask,
         .force_sample_rate_shading = stage == MESA_SHADER_FRAGMENT ? force_sample_rate_shading : false,
         .lower_view_index = lower_view_index,
         .lower_view_index_to_rt_layer = stage == last_raster_stage ? lower_view_index : false,
         .vi_conversions = vi_conversions,
         .nir_opts = &nir_opts,
         .subgroup_size = subgroup_enum,
      };

      ret = dzn_pipeline_get_nir_shader(device, layout,
                                        cache, stages[stage].nir_hash,
                                        stages[stage].info, stage,
                                        &options,
                                        &pipeline->templates.shaders[stage].nir);
      if (ret != VK_SUCCESS)
         return ret;
   }

   if (pipeline->use_gs_for_polygon_mode_point) {
      /* TODO: Cache; handle TES */
      struct dzn_nir_point_gs_info gs_info = {
         .cull_mode = info->pRasterizationState->cullMode,
         .front_ccw = info->pRasterizationState->frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE,
         .depth_bias = info->pRasterizationState->depthBiasEnable,
         .depth_bias_dynamic = pipeline->zsa.dynamic_depth_bias,
         .ds_fmt = pipeline->zsa.ds_fmt,
         .constant_depth_bias = info->pRasterizationState->depthBiasConstantFactor,
         .slope_scaled_depth_bias = info->pRasterizationState->depthBiasSlopeFactor,
         .depth_bias_clamp = info->pRasterizationState->depthBiasClamp,
         .runtime_data_cbv = {
            .register_space = DZN_REGISTER_SPACE_SYSVALS,
            .base_shader_register = 0,
         }
      };
      pipeline->templates.shaders[MESA_SHADER_GEOMETRY].nir =
         dzn_nir_polygon_point_mode_gs(pipeline->templates.shaders[MESA_SHADER_VERTEX].nir,
                                       &gs_info);

      struct dxil_spirv_runtime_conf conf = {
         .runtime_data_cbv = {
            .register_space = DZN_REGISTER_SPACE_SYSVALS,
            .base_shader_register = 0,
         },
         .yz_flip = {
            .mode = yz_flip_mode,
            .y_mask = y_flip_mask,
            .z_mask = z_flip_mask,
         },
      };

      bool requires_runtime_data;
      NIR_PASS_V(pipeline->templates.shaders[MESA_SHADER_GEOMETRY].nir, dxil_spirv_nir_lower_yz_flip,
                 &conf, &requires_runtime_data);

      active_stage_mask |= (1 << MESA_SHADER_GEOMETRY);
      memcpy(stages[MESA_SHADER_GEOMETRY].spirv_hash, stages[MESA_SHADER_VERTEX].spirv_hash, SHA1_DIGEST_LENGTH);

      if ((active_stage_mask & (1 << MESA_SHADER_FRAGMENT)) &&
         BITSET_TEST(pipeline->templates.shaders[MESA_SHADER_FRAGMENT].nir->info.system_values_read, SYSTEM_VALUE_FRONT_FACE))
         NIR_PASS_V(pipeline->templates.shaders[MESA_SHADER_FRAGMENT].nir, dxil_nir_forward_front_face);
   }

   /* Third step: link those NIR shaders. We iterate in reverse order
    * so we can eliminate outputs that are never read by the next stage.
    */
   uint32_t link_mask = active_stage_mask;
   while (link_mask != 0) {
      gl_shader_stage stage = util_last_bit(link_mask) - 1;
      link_mask &= ~BITFIELD_BIT(stage);
      gl_shader_stage prev_stage = util_last_bit(link_mask) - 1;

      struct dxil_spirv_runtime_conf conf = {
         .runtime_data_cbv = {
            .register_space = DZN_REGISTER_SPACE_SYSVALS,
            .base_shader_register = 0,
      }};

      assert(pipeline->templates.shaders[stage].nir);
      bool requires_runtime_data;
      dxil_spirv_nir_link(pipeline->templates.shaders[stage].nir,
                          prev_stage != MESA_SHADER_NONE ?
                          pipeline->templates.shaders[prev_stage].nir : NULL,
                          &conf, &requires_runtime_data);

      if (prev_stage != MESA_SHADER_NONE) {
         memcpy(stages[stage].link_hashes[0], stages[prev_stage].spirv_hash, SHA1_DIGEST_LENGTH);
         memcpy(stages[prev_stage].link_hashes[1], stages[stage].spirv_hash, SHA1_DIGEST_LENGTH);
      }
   }

   u_foreach_bit(stage, active_stage_mask) {
      uint8_t bindings_hash[SHA1_DIGEST_LENGTH];

      NIR_PASS_V(pipeline->templates.shaders[stage].nir, adjust_var_bindings, device, layout,
                 cache ? bindings_hash : NULL);

      if (cache) {
         struct mesa_sha1 dxil_hash_ctx;

         _mesa_sha1_init(&dxil_hash_ctx);
         _mesa_sha1_update(&dxil_hash_ctx, stages[stage].nir_hash, sizeof(stages[stage].nir_hash));
         _mesa_sha1_update(&dxil_hash_ctx, stages[stage].spirv_hash, sizeof(stages[stage].spirv_hash));
         _mesa_sha1_update(&dxil_hash_ctx, stages[stage].link_hashes[0], sizeof(stages[stage].link_hashes[0]));
         _mesa_sha1_update(&dxil_hash_ctx, stages[stage].link_hashes[1], sizeof(stages[stage].link_hashes[1]));
         _mesa_sha1_update(&dxil_hash_ctx, bindings_hash, sizeof(bindings_hash));
         _mesa_sha1_final(&dxil_hash_ctx, stages[stage].dxil_hash);
         dxil_hashes[stage] = stages[stage].dxil_hash;

         gl_shader_stage cached_stage;
         D3D12_SHADER_BYTECODE bc;
         ret = dzn_pipeline_cache_lookup_dxil_shader(cache, stages[stage].dxil_hash, &cached_stage, &bc);
         if (ret != VK_SUCCESS)
            return ret;

         if (cached_stage != MESA_SHADER_NONE) {
            assert(cached_stage == stage);
            D3D12_SHADER_BYTECODE *slot =
               dzn_pipeline_get_gfx_shader_slot(out, stage);
            *slot = bc;
            pipeline->templates.shaders[stage].bc = slot;
         }
      }
   }

   uint32_t vert_input_count = 0;
   if (pipeline->templates.shaders[MESA_SHADER_VERTEX].nir) {
      /* Now, declare one D3D12_INPUT_ELEMENT_DESC per VS input variable, so
       * we can handle location overlaps properly.
       */
      nir_foreach_shader_in_variable(var, pipeline->templates.shaders[MESA_SHADER_VERTEX].nir) {
         assert(var->data.location >= VERT_ATTRIB_GENERIC0);
         unsigned loc = var->data.location - VERT_ATTRIB_GENERIC0;
         assert(vert_input_count < D3D12_VS_INPUT_REGISTER_COUNT);
         assert(loc < MAX_VERTEX_GENERIC_ATTRIBS);

         pipeline->templates.inputs[vert_input_count] = attribs[loc];
         pipeline->templates.inputs[vert_input_count].SemanticIndex = vert_input_count;
         var->data.driver_location = vert_input_count++;
      }

      if (vert_input_count > 0) {
         d3d12_gfx_pipeline_state_stream_new_desc(out, INPUT_LAYOUT, D3D12_INPUT_LAYOUT_DESC, desc);
         desc->pInputElementDescs = pipeline->templates.inputs;
         desc->NumElements = vert_input_count;
      }
   }

   /* Last step: translate NIR shaders into DXIL modules */
   u_foreach_bit(stage, active_stage_mask) {
      gl_shader_stage prev_stage =
         util_last_bit(active_stage_mask & BITFIELD_MASK(stage)) - 1;
      uint32_t prev_stage_output_clip_size = 0;
      if (stage == MESA_SHADER_FRAGMENT) {
         /* Disable rasterization if the last geometry stage doesn't
          * write the position.
          */
         if (prev_stage == MESA_SHADER_NONE ||
             !(pipeline->templates.shaders[prev_stage].nir->info.outputs_written & VARYING_BIT_POS)) {
            pipeline->rast_disabled_from_missing_position = true;
            /* Clear a cache hit if there was one. */
            pipeline->templates.shaders[stage].bc = NULL;
            continue;
         }
      } else if (prev_stage != MESA_SHADER_NONE) {
         prev_stage_output_clip_size = pipeline->templates.shaders[prev_stage].nir->info.clip_distance_array_size;
      }

      /* Cache hit, we can skip the compilation. */
      if (pipeline->templates.shaders[stage].bc)
         continue;

      D3D12_SHADER_BYTECODE *slot =
         dzn_pipeline_get_gfx_shader_slot(out, stage);
         /*
         //Where is compute stage??
      static D3D12_SHADER_BYTECODE *
      dzn_pipeline_get_gfx_shader_slot(D3D12_PIPELINE_STATE_STREAM_DESC *stream,
                                       gl_shader_stage in)
      {
         switch (in) {
         case MESA_SHADER_VERTEX: {
            d3d12_gfx_pipeline_state_stream_new_desc(stream, VS, D3D12_SHADER_BYTECODE, desc);
            return desc;
         }
         case MESA_SHADER_TESS_CTRL: {
            d3d12_gfx_pipeline_state_stream_new_desc(stream, DS, D3D12_SHADER_BYTECODE, desc);
            return desc;
         }
         case MESA_SHADER_TESS_EVAL: {
            d3d12_gfx_pipeline_state_stream_new_desc(stream, HS, D3D12_SHADER_BYTECODE, desc);
            return desc;
         }
         case MESA_SHADER_GEOMETRY: {
            d3d12_gfx_pipeline_state_stream_new_desc(stream, GS, D3D12_SHADER_BYTECODE, desc);
            return desc;
         }
         case MESA_SHADER_FRAGMENT: {
            d3d12_gfx_pipeline_state_stream_new_desc(stream, PS, D3D12_SHADER_BYTECODE, desc);
            return desc;
         }
         default: unreachable("Unsupported stage");
         }
      }
         
         */

      ret = dzn_pipeline_compile_shader(device, pipeline->templates.shaders[stage].nir, prev_stage_output_clip_size, slot);
      if (ret != VK_SUCCESS)
         return ret;

      pipeline->templates.shaders[stage].bc = slot;

      //if (cache)
      //   dzn_pipeline_cache_add_dxil_shader(cache, stages[stage].dxil_hash, stage, slot);
   }

   //if (cache)
   //   dzn_pipeline_cache_add_gfx_pipeline(pipeline, cache, vert_input_count, pipeline_hash,
   //                                       dxil_hashes);

   return VK_SUCCESS;
}


    
static VkResult
dzn_graphics_pipeline_create(struct dzn_device *device,
                             VkPipelineCache cache,
                             const VkGraphicsPipelineCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator,
                             VkPipeline *out)
{
   struct dzn_physical_device *pdev =
      container_of(device->vk.physical, struct dzn_physical_device, vk);
   const VkPipelineRenderingCreateInfo *ri = (const VkPipelineRenderingCreateInfo *)
      vk_find_struct_const(pCreateInfo, PIPELINE_RENDERING_CREATE_INFO);
   VK_FROM_HANDLE(vk_pipeline_cache, pcache, cache);
   VK_FROM_HANDLE(vk_render_pass, pass, pCreateInfo->renderPass);
   VK_FROM_HANDLE(dzn_pipeline_layout, layout, pCreateInfo->layout);
   uint32_t color_count = 0;
   VkFormat color_fmts[MAX_RTS] = { 0 };
   VkFormat zs_fmt = VK_FORMAT_UNDEFINED;
   VkResult ret;
   HRESULT hres = 0;
   D3D12_VIEW_INSTANCE_LOCATION vi_locs[D3D12_MAX_VIEW_INSTANCE_COUNT];

   struct dzn_graphics_pipeline *pipeline =
      vk_zalloc2(&device->vk.alloc, pAllocator, sizeof(*pipeline), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!pipeline)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   D3D12_PIPELINE_STATE_STREAM_DESC *stream_desc = &pipeline->templates.stream_desc;
   stream_desc->pPipelineStateSubobjectStream = pipeline->templates.stream_buf;

   dzn_pipeline_init(&pipeline->base, device,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     layout, stream_desc);
   D3D12_INPUT_ELEMENT_DESC attribs[MAX_VERTEX_GENERIC_ATTRIBS] = { 0 };
   enum pipe_format vi_conversions[MAX_VERTEX_GENERIC_ATTRIBS] = { 0 };

   ret = dzn_graphics_pipeline_translate_vi(pipeline, pCreateInfo,
                                            attribs, vi_conversions);
   if (ret != VK_SUCCESS)
      goto out;

   d3d12_gfx_pipeline_state_stream_new_desc(stream_desc, FLAGS, D3D12_PIPELINE_STATE_FLAGS, flags);
   *flags = D3D12_PIPELINE_STATE_FLAG_NONE;

   if (pCreateInfo->pDynamicState) {
      for (uint32_t i = 0; i < pCreateInfo->pDynamicState->dynamicStateCount; i++) {
         switch (pCreateInfo->pDynamicState->pDynamicStates[i]) {
         case VK_DYNAMIC_STATE_VIEWPORT:
            pipeline->vp.dynamic = true;
            break;
         case VK_DYNAMIC_STATE_SCISSOR:
            pipeline->scissor.dynamic = true;
            break;
         case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
            pipeline->zsa.stencil_test.dynamic_ref = true;
            break;
         case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
            pipeline->zsa.stencil_test.dynamic_compare_mask = true;
            ret = dzn_graphics_pipeline_prepare_for_variants(device, pipeline);
            if (ret)
               goto out;
            break;
         case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
            pipeline->zsa.stencil_test.dynamic_write_mask = true;
            ret = dzn_graphics_pipeline_prepare_for_variants(device, pipeline);
            if (ret)
               goto out;
            break;
         case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
            pipeline->blend.dynamic_constants = true;
            break;
         case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
            pipeline->zsa.depth_bounds.dynamic = true;
            break;
         case VK_DYNAMIC_STATE_DEPTH_BIAS:
            pipeline->zsa.dynamic_depth_bias = true;
            if (pdev->options16.DynamicDepthBiasSupported) {
               *flags |= D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS;
            } else {
               ret = dzn_graphics_pipeline_prepare_for_variants(device, pipeline);
               if (ret)
                  goto out;
            }
            break;
         case VK_DYNAMIC_STATE_LINE_WIDTH:
            /* Nothing to do since we just support lineWidth = 1. */
            break;
         default: unreachable("Unsupported dynamic state");
         }
      }
   }

   ret = dzn_graphics_pipeline_translate_ia(device, pipeline, stream_desc, pCreateInfo);
   if (ret)
      goto out;

   dzn_graphics_pipeline_translate_rast(device, pipeline, stream_desc, pCreateInfo);
   dzn_graphics_pipeline_translate_ms(pipeline, stream_desc, pCreateInfo);
   dzn_graphics_pipeline_translate_zsa(device, pipeline, stream_desc, pCreateInfo);
   dzn_graphics_pipeline_translate_blend(pipeline, stream_desc, pCreateInfo);

   unsigned view_mask = 0;
   if (pass) {
      const struct vk_subpass *subpass = &pass->subpasses[pCreateInfo->subpass];
      color_count = subpass->color_count;
      for (uint32_t i = 0; i < subpass->color_count; i++) {
         uint32_t idx = subpass->color_attachments[i].attachment;

         if (idx == VK_ATTACHMENT_UNUSED) continue;

         const struct vk_render_pass_attachment *attachment =
            &pass->attachments[idx];

         color_fmts[i] = attachment->format;
      }

      if (subpass->depth_stencil_attachment &&
          subpass->depth_stencil_attachment->attachment != VK_ATTACHMENT_UNUSED) {
         const struct vk_render_pass_attachment *attachment =
            &pass->attachments[subpass->depth_stencil_attachment->attachment];

         zs_fmt = attachment->format;
      }

      view_mask = subpass->view_mask;
   } else if (ri) {
      color_count = ri->colorAttachmentCount;
      memcpy(color_fmts, ri->pColorAttachmentFormats,
             sizeof(color_fmts[0]) * color_count);
      if (ri->depthAttachmentFormat != VK_FORMAT_UNDEFINED)
         zs_fmt = ri->depthAttachmentFormat;
      else if (ri->stencilAttachmentFormat != VK_FORMAT_UNDEFINED)
         zs_fmt = ri->stencilAttachmentFormat;

      view_mask = ri->viewMask;
   }

   if (color_count > 0) {
      d3d12_gfx_pipeline_state_stream_new_desc(stream_desc, RENDER_TARGET_FORMATS, struct D3D12_RT_FORMAT_ARRAY, rts);
      rts->NumRenderTargets = color_count;
      for (uint32_t i = 0; i < color_count; i++) {
         rts->RTFormats[i] =
            dzn_image_get_dxgi_format(pdev, color_fmts[i],
                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                      VK_IMAGE_ASPECT_COLOR_BIT);
      }
   }

   if (zs_fmt != VK_FORMAT_UNDEFINED) {
      d3d12_gfx_pipeline_state_stream_new_desc(stream_desc, DEPTH_STENCIL_FORMAT, DXGI_FORMAT, ds_fmt);
      *ds_fmt =
         dzn_image_get_dxgi_format(pdev, zs_fmt,
                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                   VK_IMAGE_ASPECT_DEPTH_BIT |
                                   VK_IMAGE_ASPECT_STENCIL_BIT);
      pipeline->zsa.ds_fmt = *ds_fmt;
   }

   pipeline->multiview.view_mask = MAX2(view_mask, 1);
   if (view_mask != 0 && /* Is multiview */
       view_mask != 1 && /* Is non-trivially multiview */
       (view_mask & ~((1 << D3D12_MAX_VIEW_INSTANCE_COUNT) - 1)) == 0 && /* Uses only views 0 thru 3 */
       pdev->options3.ViewInstancingTier > D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED /* Actually supported */) {
      d3d12_gfx_pipeline_state_stream_new_desc(stream_desc, VIEW_INSTANCING, D3D12_VIEW_INSTANCING_DESC, vi);
      vi->pViewInstanceLocations = vi_locs;
      for (uint32_t i = 0; i < D3D12_MAX_VIEW_INSTANCE_COUNT; ++i) {
         vi_locs[i].RenderTargetArrayIndex = i;
         vi_locs[i].ViewportArrayIndex = 0;
         if (view_mask & (1 << i))
            vi->ViewInstanceCount = i + 1;
      }
      vi->Flags = D3D12_VIEW_INSTANCING_FLAG_ENABLE_VIEW_INSTANCE_MASKING;
      pipeline->multiview.native_view_instancing = true;
   }

   ret = dzn_graphics_pipeline_compile_shaders(device, pipeline, pcache,
                                               layout, stream_desc,
                                               attribs, vi_conversions,
                                               pCreateInfo);
   if (ret != VK_SUCCESS)
      goto out;

   /* If we have no position output from a pre-rasterizer stage, we need to make sure that
    * depth is disabled, to fully disable the rasterizer. We can only know this after compiling
    * or loading the shaders.
    */
   if (pipeline->rast_disabled_from_missing_position) {
      if (pdev->options14.IndependentFrontAndBackStencilRefMaskSupported) {
         D3D12_DEPTH_STENCIL_DESC2 *ds = dzn_graphics_pipeline_get_desc(pipeline, pipeline->templates.stream_buf, ds);
         if (ds)
            ds->DepthEnable = ds->StencilEnable = false;
      } else {
         D3D12_DEPTH_STENCIL_DESC1 *ds = dzn_graphics_pipeline_get_desc(pipeline, pipeline->templates.stream_buf, ds);
         if (ds)
            ds->DepthEnable = ds->StencilEnable = false;
      }
   }

   if (!pipeline->variants) {
      hres = ID3D12Device4_CreatePipelineState(device->dev, stream_desc,
                                               &IID_ID3D12PipelineState,
                                               (void **)&pipeline->base.state);
      if (FAILED(hres)) {
         ret = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
         goto out;
      }

      dzn_graphics_pipeline_cleanup_dxil_shaders(pipeline);
   }

   dzn_graphics_pipeline_cleanup_nir_shaders(pipeline);
   ret = VK_SUCCESS;

out:
   if (ret != VK_SUCCESS)
      dzn_graphics_pipeline_destroy(pipeline, pAllocator);
   else
      *out = dzn_graphics_pipeline_to_handle(pipeline);

   return ret;
}

} // namespace Veldrid::VK::priv



namespace Veldrid
{
    DXCDevice* DXCPipelineBase::_Dev() {return static_cast<DXCDevice*>(dev.get());}

    sp<Pipeline> DXCGraphicsPipeline::Make(const sp<DXCDevice> &dev, const GraphicsPipelineDescription &desc) {
        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        /*
        typedef struct D3D12_GRAPHICS_PIPELINE_STATE_DESC
        {
            ID3D12RootSignature *pRootSignature;
            D3D12_SHADER_BYTECODE VS;
            D3D12_SHADER_BYTECODE PS;
            D3D12_SHADER_BYTECODE DS;
            D3D12_SHADER_BYTECODE HS;
            D3D12_SHADER_BYTECODE GS;
            D3D12_STREAM_OUTPUT_DESC StreamOutput;
            * D3D12_BLEND_DESC BlendState;
            * UINT SampleMask;
            * D3D12_RASTERIZER_DESC RasterizerState;
            * D3D12_DEPTH_STENCIL_DESC DepthStencilState;
            * D3D12_INPUT_LAYOUT_DESC InputLayout;
            D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
            * D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
            * UINT NumRenderTargets;
            * DXGI_FORMAT RTVFormats[ 8 ];
            * DXGI_FORMAT DSVFormat;
            * DXGI_SAMPLE_DESC SampleDesc;
            UINT NodeMask;
            D3D12_CACHED_PIPELINE_STATE CachedPSO;
            D3D12_PIPELINE_STATE_FLAGS Flags;
        } 	D3D12_GRAPHICS_PIPELINE_STATE_DESC;
        */

        // Describe and create the graphics pipeline state object (PSO).
        std::vector<sp<RefCntBase>> refCnts;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc {};
        //psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        //psoDesc.pRootSignature = m_rootSignature.Get();
        //psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        //psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        //psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        //psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        //psoDesc.DepthStencilState.DepthEnable = FALSE;
        //psoDesc.DepthStencilState.StencilEnable = FALSE;
        //psoDesc.SampleMask = UINT_MAX;
        //psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        //psoDesc.NumRenderTargets = 1;
        //psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        //psoDesc.SampleDesc.Count = 1;
        

        /*
        // Blend State
        VkPipelineColorBlendStateCreateInfo blendStateCI{};
        blendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        auto attachmentsCount = desc.blendState.attachments.size();
        std::vector<VkPipelineColorBlendAttachmentState> attachments(attachmentsCount);
        for (int i = 0; i < attachmentsCount; i++)
        {
            auto vdDesc = desc.blendState.attachments[i];
            auto& attachmentState = attachments[i];
            attachmentState.srcColorBlendFactor = VdToVkBlendFactor(vdDesc.sourceAlphaFactor);
            attachmentState.dstColorBlendFactor = VdToVkBlendFactor(vdDesc.destinationColorFactor);
            attachmentState.colorBlendOp = VdToVkBlendOp(vdDesc.colorFunction);
            attachmentState.srcAlphaBlendFactor = VdToVkBlendFactor(vdDesc.sourceAlphaFactor);
            attachmentState.dstAlphaBlendFactor = VdToVkBlendFactor(vdDesc.destinationAlphaFactor);
            attachmentState.alphaBlendOp = VdToVkBlendOp(vdDesc.alphaFunction);
            attachmentState.colorWriteMask = VdToVkColorWriteMask(vdDesc.colorWriteMask);
            attachmentState.blendEnable = vdDesc.blendEnabled;
        }
        
        blendStateCI.attachmentCount = attachmentsCount;
        blendStateCI.pAttachments = attachments.data();
        auto& blendFactor = desc.blendState.blendConstant;
        blendStateCI.blendConstants[0] = blendFactor.r;
        blendStateCI.blendConstants[1] = blendFactor.g;
        blendStateCI.blendConstants[2] = blendFactor.b;
        blendStateCI.blendConstants[3] = blendFactor.a;

        pipelineCI.pColorBlendState = &blendStateCI;
        */
        {
            //For BlendFactor::BlendFactor: 
            //The blend factor is the blend factor set with 
            //ID3D12GraphicsCommandList::OMSetBlendFactor. No pre-blend operation.

            /*void OMSetBlendFactor(
              [in, optional] const FLOAT [4] BlendFactor
            );*/
            auto& blendFactor = desc.blendState.blendConstant;
        //blendStateCI.blendConstants[0] = blendFactor.r;
        //blendStateCI.blendConstants[1] = blendFactor.g;
        //blendStateCI.blendConstants[2] = blendFactor.b;
        //blendStateCI.blendConstants[3] = blendFactor.a;

            auto& blendState = psoDesc.BlendState;
            // BOOL AlphaToCoverageEnable;
            //BOOL IndependentBlendEnable;

            auto attachmentsCount = desc.blendState.attachments.size();

            assert(attachmentsCount <= 8);

            for (int i = 0; i < attachmentsCount; i++)
            {
                auto vdDesc = desc.blendState.attachments[i];
                auto& attachmentState = blendState.RenderTarget[i];

                /*typedef struct D3D12_RENDER_TARGET_BLEND_DESC
                    {
                    BOOL BlendEnable;
                    BOOL LogicOpEnable;
                    D3D12_BLEND SrcBlend;
                    D3D12_BLEND DestBlend;
                    D3D12_BLEND_OP BlendOp;
                    D3D12_BLEND SrcBlendAlpha;
                    D3D12_BLEND DestBlendAlpha;
                    D3D12_BLEND_OP BlendOpAlpha;
                    D3D12_LOGIC_OP LogicOp;
                    UINT8 RenderTargetWriteMask;
                    } 	D3D12_RENDER_TARGET_BLEND_DESC;
                    
                    typedef 
                    enum D3D12_BLEND
                        {
                            D3D12_BLEND_ZERO	= 1,
                            D3D12_BLEND_ONE	= 2,
                            D3D12_BLEND_SRC_COLOR	= 3,
                            D3D12_BLEND_INV_SRC_COLOR	= 4,
                            D3D12_BLEND_SRC_ALPHA	= 5,
                            D3D12_BLEND_INV_SRC_ALPHA	= 6,
                            D3D12_BLEND_DEST_ALPHA	= 7,
                            D3D12_BLEND_INV_DEST_ALPHA	= 8,
                            D3D12_BLEND_DEST_COLOR	= 9,
                            D3D12_BLEND_INV_DEST_COLOR	= 10,
                            D3D12_BLEND_SRC_ALPHA_SAT	= 11,
                            D3D12_BLEND_BLEND_FACTOR	= 14,
                            D3D12_BLEND_INV_BLEND_FACTOR	= 15,
                            D3D12_BLEND_SRC1_COLOR	= 16,
                            D3D12_BLEND_INV_SRC1_COLOR	= 17,
                            D3D12_BLEND_SRC1_ALPHA	= 18,
                            D3D12_BLEND_INV_SRC1_ALPHA	= 19
                        } 	D3D12_BLEND;

                    typedef 
                    enum D3D12_BLEND_OP
                        {
                            D3D12_BLEND_OP_ADD	= 1,
                            D3D12_BLEND_OP_SUBTRACT	= 2,
                            D3D12_BLEND_OP_REV_SUBTRACT	= 3,
                            D3D12_BLEND_OP_MIN	= 4,
                            D3D12_BLEND_OP_MAX	= 5
                        } 	D3D12_BLEND_OP;

                    typedef 
                    enum D3D12_COLOR_WRITE_ENABLE
                        {
                            D3D12_COLOR_WRITE_ENABLE_RED	= 1,
                            D3D12_COLOR_WRITE_ENABLE_GREEN	= 2,
                            D3D12_COLOR_WRITE_ENABLE_BLUE	= 4,
                            D3D12_COLOR_WRITE_ENABLE_ALPHA	= 8,
                            D3D12_COLOR_WRITE_ENABLE_ALL	= ( ( ( D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN )  | D3D12_COLOR_WRITE_ENABLE_BLUE )  | D3D12_COLOR_WRITE_ENABLE_ALPHA ) 
                        } 	D3D12_COLOR_WRITE_ENABLE;

                    
                    
                    */

                attachmentState.SrcBlend = VdToD3DBlendFactor(vdDesc.sourceAlphaFactor);
                attachmentState.DestBlend = VdToD3DBlendFactor(vdDesc.destinationColorFactor);
                attachmentState.BlendOp = VdToD3DBlendOp(vdDesc.colorFunction);
                attachmentState.SrcBlendAlpha = VdToD3DBlendFactor(vdDesc.sourceAlphaFactor);
                attachmentState.DestBlendAlpha = VdToD3DBlendFactor(vdDesc.destinationAlphaFactor);
                attachmentState.BlendOpAlpha = VdToD3DBlendOp(vdDesc.alphaFunction);
                attachmentState.RenderTargetWriteMask = VdToD3DColorWriteMask(vdDesc.colorWriteMask);
                attachmentState.BlendEnable = vdDesc.blendEnabled;
            }

            blendState.AlphaToCoverageEnable = desc.blendState.alphaToCoverageEnabled;
       
        }
        /*        
        // Rasterizer State
        auto& rsDesc = desc.rasterizerState;
        VkPipelineRasterizationStateCreateInfo rsCI{};
        rsCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rsCI.cullMode = VdToVkCullMode(rsDesc.cullMode);
        rsCI.polygonMode = VdToVkPolygonMode(rsDesc.fillMode);
        rsCI.depthClampEnable = !rsDesc.depthClipEnabled;
        rsCI.frontFace = rsDesc.frontFace == RasterizerStateDescription::FrontFace::Clockwise
            ? VkFrontFace::VK_FRONT_FACE_CLOCKWISE 
            : VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsCI.lineWidth = 1.f;
        pipelineCI.pRasterizationState = &rsCI;
*/
        {
            auto& rsDesc = desc.rasterizerState;
            auto& rsCI = psoDesc.RasterizerState;
            /*
            typedef struct D3D12_RASTERIZER_DESC
            {
            D3D12_FILL_MODE FillMode;
            D3D12_CULL_MODE CullMode;
            BOOL FrontCounterClockwise;
            INT DepthBias;
            FLOAT DepthBiasClamp;
            FLOAT SlopeScaledDepthBias;
            BOOL DepthClipEnable;
            BOOL MultisampleEnable;
            BOOL AntialiasedLineEnable;
            UINT ForcedSampleCount;
            D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
            } 	D3D12_RASTERIZER_DESC;*/

            rsCI.CullMode = VdToD3DCullMode(rsDesc.cullMode);
            rsCI.FillMode = VdToD3DPolygonMode(rsDesc.fillMode);
            rsCI.DepthClipEnable = rsDesc.depthClipEnabled;
            rsCI.FrontCounterClockwise = rsDesc.frontFace == RasterizerStateDescription::FrontFace::Clockwise;

        }
            /*
        // Dynamic State
        VkPipelineDynamicStateCreateInfo dynamicStateCI{};
        dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        VkDynamicState dynamicStates[2];
        dynamicStates[0] = VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT;
        dynamicStates[1] = VkDynamicState::VK_DYNAMIC_STATE_SCISSOR;
        dynamicStateCI.dynamicStateCount = 2;
        dynamicStateCI.pDynamicStates = dynamicStates;

        pipelineCI.pDynamicState = &dynamicStateCI;

        // Depth Stencil State
        auto& vdDssDesc = desc.depthStencilState;
        VkPipelineDepthStencilStateCreateInfo dssCI{};
        dssCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dssCI.depthWriteEnable = vdDssDesc.depthWriteEnabled;
        dssCI.depthTestEnable = vdDssDesc.depthTestEnabled;
        dssCI.depthCompareOp = VdToVkCompareOp(vdDssDesc.depthComparison);
        dssCI.stencilTestEnable = vdDssDesc.stencilTestEnabled;

        dssCI.front.failOp = VdToVkStencilOp(vdDssDesc.stencilFront.fail);
        dssCI.front.passOp = VdToVkStencilOp(vdDssDesc.stencilFront.pass);
        dssCI.front.depthFailOp = VdToVkStencilOp(vdDssDesc.stencilFront.depthFail);
        dssCI.front.compareOp = VdToVkCompareOp(vdDssDesc.stencilFront.comparison);
        dssCI.front.compareMask = vdDssDesc.stencilReadMask;
        dssCI.front.writeMask = vdDssDesc.stencilWriteMask;
        dssCI.front.reference = vdDssDesc.stencilReference;

        dssCI.back.failOp = VdToVkStencilOp(vdDssDesc.stencilBack.fail);
        dssCI.back.passOp = VdToVkStencilOp(vdDssDesc.stencilBack.pass);
        dssCI.back.depthFailOp = VdToVkStencilOp(vdDssDesc.stencilBack.depthFail);
        dssCI.back.compareOp = VdToVkCompareOp(vdDssDesc.stencilBack.comparison);
        dssCI.back.compareMask = vdDssDesc.stencilReadMask;
        dssCI.back.writeMask = vdDssDesc.stencilWriteMask;
        dssCI.back.reference = vdDssDesc.stencilReference;

        pipelineCI.pDepthStencilState = &dssCI;*/
        {
            auto& vdDssDesc = desc.depthStencilState;
            auto& dssState = psoDesc.DepthStencilState;
            

            dssState.DepthEnable = vdDssDesc.depthTestEnabled;
            dssState.DepthFunc = VdToD3DCompareOp(vdDssDesc.depthComparison);
            dssState.DepthWriteMask = vdDssDesc.depthWriteEnabled 
                        ? D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ALL
                        : D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ZERO;

            dssState.StencilEnable = vdDssDesc.stencilTestEnabled;
            dssState.StencilReadMask = vdDssDesc.stencilReadMask;
            dssState.StencilWriteMask = vdDssDesc.stencilWriteMask;

            
            dssState.FrontFace.StencilFailOp = VdToD3DStencilOp(vdDssDesc.stencilFront.fail);
            dssState.FrontFace.StencilPassOp = VdToD3DStencilOp(vdDssDesc.stencilFront.pass);
            dssState.FrontFace.StencilDepthFailOp = VdToD3DStencilOp(vdDssDesc.stencilFront.depthFail);
            dssState.FrontFace.StencilFunc = VdToD3DCompareOp(vdDssDesc.stencilFront.comparison);

            dssState.BackFace.StencilFailOp = VdToD3DStencilOp(vdDssDesc.stencilBack.fail);
            dssState.BackFace.StencilPassOp = VdToD3DStencilOp(vdDssDesc.stencilBack.pass);
            dssState.BackFace.StencilDepthFailOp = VdToD3DStencilOp(vdDssDesc.stencilBack.depthFail);
            dssState.BackFace.StencilFunc = VdToD3DCompareOp(vdDssDesc.stencilBack.comparison);
        }
        
        
        /*

        // Multisample
        VkPipelineMultisampleStateCreateInfo multisampleCI{};
        multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        VkSampleCountFlagBits vkSampleCount = VdToVkSampleCount(desc.outputs.sampleCount);
        multisampleCI.rasterizationSamples = vkSampleCount;
        multisampleCI.alphaToCoverageEnable = desc.blendState.alphaToCoverageEnabled;
        pipelineCI.pMultisampleState = &multisampleCI;
*/
        {
            psoDesc.SampleMask = 0xffffffff; //This has to do with multi-sampling. 0xffffffff means point sampling is used. 
            auto& msState = psoDesc.SampleDesc;
            msState.Count = (std::uint8_t)desc.outputs.sampleCount;
            msState.Quality = 1;//TODO: Query quality support for device:
            //D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
            //msLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Replace with your render target format.
            //msLevels.SampleCount = 4; // Replace with your sample count.
            //msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
            //
            //m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels));
        }

/*
        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
        inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyCI.topology = VdToVkPrimitiveTopology(desc.primitiveTopology);
        inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        pipelineCI.pInputAssemblyState = &
*/
        {
            auto& topologyState = psoDesc.PrimitiveTopologyType;
            topologyState = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }

/*
        // Vertex Input State
        VkPipelineVertexInputStateCreateInfo vertexInputCI{};
        vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
*/
        
        auto& inputDescriptions = desc.shaderSet.vertexLayouts;
        auto bindingCount = inputDescriptions.size();
        unsigned attributeCount = 0;
        for (int i = 0; i < inputDescriptions.size(); i++)
        {
            attributeCount += inputDescriptions[i].elements.size();
        }

        std::vector<D3D12_INPUT_ELEMENT_DESC> iaDescs(attributeCount);
/*
            typedef struct D3D12_INPUT_ELEMENT_DESC
    {
    LPCSTR SemanticName;
    UINT SemanticIndex;
    DXGI_FORMAT Format;
    UINT InputSlot;
    UINT AlignedByteOffset;
    D3D12_INPUT_CLASSIFICATION InputSlotClass;
    UINT InstanceDataStepRate;
    } 	D3D12_INPUT_ELEMENT_DESC;
*/
            

        //std::vector<VkVertexInputBindingDescription> bindingDescs(bindingCount);
        //std::vector<VkVertexInputAttributeDescription> attributeDescs(attributeCount);

        int targetIndex = 0;
        int targetLocation = 0;
        for (int binding = 0; binding < inputDescriptions.size(); binding++)
        {
            auto& inputDesc = inputDescriptions[binding];
            //bindingDescs[binding].binding = binding;
            //bindingDescs[binding].inputRate = (inputDesc.instanceStepRate != 0) 
            //    ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE 
            //    : VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
            //bindingDescs[binding].stride = inputDesc.stride;
            
            unsigned currentOffset = 0;
            for (int location = 0; location < inputDesc.elements.size(); location++)
            {
                auto& inputElement = inputDesc.elements[location];

                iaDescs[targetIndex]./*LPCSTR*/ SemanticName;
                iaDescs[targetIndex]./*UINT*/ SemanticIndex = 0;
                iaDescs[targetIndex]./*DXGI_FORMAT*/ Format = VdToD3DShaderDataType(inputElement.format);
                iaDescs[targetIndex]./*UINT*/ InputSlot = binding;
                iaDescs[targetIndex]./*UINT*/ AlignedByteOffset = inputElement.offset != 0 
                                                                ? inputElement.offset 
                                                                : currentOffset;
                //TODO: [DXC, Vk] Support instanced draw?
                iaDescs[targetIndex]./*D3D12_INPUT_CLASSIFICATION*/ InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                iaDescs[targetIndex]./*UINT*/ InstanceDataStepRate = 0;
                
                targetIndex += 1;
                currentOffset += Helpers::FormatHelpers::GetSizeInBytes(inputElement.format);
            }

            targetLocation += inputDesc.elements.size();
        }
        
        auto& viState = psoDesc.InputLayout;
        viState.NumElements = iaDescs.size();
        viState.pInputElementDescs = iaDescs.data();
        


/*
        // Shader Stage

        VkSpecializationInfo specializationInfo{};
        auto& specDescs = desc.shaderSet.specializations;
        if (!specDescs.empty())
        {
            unsigned specDataSize = 0;
            for (auto& spec : specDescs) {
                specDataSize += GetSpecializationConstantSize(spec.type);
            }
            std::vector<std::uint8_t> fullSpecData(specDataSize);
            int specializationCount = specDescs.size();
            std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
            unsigned specOffset = 0;
            for (int i = 0; i < specializationCount; i++)
            {
                auto data = specDescs[i].data;
                auto srcData = (byte*)&data;
                auto dataSize = GetSpecializationConstantSize(specDescs[i].type);
                //Unsafe.CopyBlock(fullSpecData + specOffset, srcData, dataSize);
                memcpy(fullSpecData.data() + specOffset, srcData, dataSize);
                mapEntries[i].constantID = specDescs[i].id;
                mapEntries[i].offset = specOffset;
                mapEntries[i].size = dataSize;
                specOffset += dataSize;
            }
            specializationInfo.dataSize = specDataSize;
            specializationInfo.pData = fullSpecData.data();
            specializationInfo.mapEntryCount = specializationCount;
            specializationInfo.pMapEntries = mapEntries.data();
        }

        auto& shaders = desc.shaderSet.shaders;
        std::vector<VkPipelineShaderStageCreateInfo> stageCIs(shaders.size());
        for (unsigned i = 0; i < shaders.size(); i++){
            auto& shader = shaders[i];
            auto vkShader = reinterpret_cast<VulkanShader*>(shader.get());
            auto& stageCI = stageCIs[i];
            stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCI.module = vkShader->GetHandle();
            stageCI.stage = VdToVkShaderStageSingle(shader->GetDesc().stage);
            // stageCI.pName = CommonStrings.main; // Meh
            stageCI.pName = shader->GetDesc().entryPoint.c_str(); // TODO: DONT ALLOCATE HERE
            stageCI.pSpecializationInfo = &specializationInfo;
        }

        pipelineCI.stageCount = stageCIs.size();
        pipelineCI.pStages = stageCIs.data();

        // ViewportState
        // Vulkan spec specifies that there must be 1 viewport no matter
        // dynamic viewport state enabled or not...
        VkPipelineViewportStateCreateInfo viewportStateCI{};
        viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCI.viewportCount = 1;
        viewportStateCI.scissorCount = 1;

        pipelineCI.pViewportState = &viewportStateCI;

        // Pipeline Layout
        auto& resourceLayouts = desc.resourceLayouts;
        VkPipelineLayoutCreateInfo pipelineLayoutCI {};
        pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCI.setLayoutCount = resourceLayouts.size();
        std::vector<VkDescriptorSetLayout> dsls (resourceLayouts.size());
        for (int i = 0; i < resourceLayouts.size(); i++)
        {
            dsls[i] = PtrCast<VulkanResourceLayout>(resourceLayouts[i].get())->GetHandle();
        }
        pipelineLayoutCI.pSetLayouts = dsls.data();
        
        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(dev->LogicalDev(), &pipelineLayoutCI, nullptr, &pipelineLayout));
        pipelineCI.layout = pipelineLayout;
        
        // Create fake RenderPass for compatibility.
        auto& outputDesc = desc.outputs;
        
        //auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, VK_SAMPLE_COUNT_1_BIT);
        auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, vkSampleCount);
        */

        //Output formats
        auto color_count = desc.outputs.colorAttachment.size();
        if (color_count > 0) {
            

           psoDesc.NumRenderTargets = color_count;
           for (uint32_t i = 0; i < color_count; i++) {
              psoDesc.RTVFormats[i] = VdToD3DPixelFormat(desc.outputs.colorAttachment[i].format, false);
           }
        }

        if (desc.outputs.depthAttachment.has_value()) {
            psoDesc.DSVFormat = VdToD3DPixelFormat(desc.outputs.depthAttachment.value().format, true);
        }

        pipeline->multiview.view_mask = MAX2(view_mask, 1);
        if (view_mask != 0 && /* Is multiview */
            view_mask != 1 && /* Is non-trivially multiview */
            (view_mask & ~((1 << D3D12_MAX_VIEW_INSTANCE_COUNT) - 1)) == 0 && /* Uses only views 0 thru 3 */
            pdev->options3.ViewInstancingTier > D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED /* Actually supported */) {
           d3d12_gfx_pipeline_state_stream_new_desc(stream_desc, VIEW_INSTANCING, D3D12_VIEW_INSTANCING_DESC, vi);
           vi->pViewInstanceLocations = vi_locs;
           for (uint32_t i = 0; i < D3D12_MAX_VIEW_INSTANCE_COUNT; ++i) {
              vi_locs[i].RenderTargetArrayIndex = i;
              vi_locs[i].ViewportArrayIndex = 0;
              if (view_mask & (1 << i))
                 vi->ViewInstanceCount = i + 1;
           }
           vi->Flags = D3D12_VIEW_INSTANCING_FLAG_ENABLE_VIEW_INSTANCE_MASKING;
           pipeline->multiview.native_view_instancing = true;
        }
       
       
       /*
        
        pipelineCI.renderPass = compatRenderPass;
        
        VkPipeline devicePipeline;
        VK_CHECK(vkCreateGraphicsPipelines(dev->LogicalDev(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &devicePipeline));

        //auto vkVertShader = reinterpret_cast<VulkanShader*>(shaders[0].get());
        //auto vkFragShader = reinterpret_cast<VulkanShader*>(shaders[1].get());

        //_CreateStandardPipeline(dev->LogicalDev(),
        //    vkVertShader->GetHandle(), vkFragShader->GetHandle(),
        //    640,480, compatRenderPass, 
        //    pipelineLayout, devicePipeline
        //    );

        std::uint32_t resourceSetCount = desc.resourceLayouts.size();
        std::uint32_t dynamicOffsetsCount = 0;
        for(auto& layout : desc.resourceLayouts)
        {
            auto vkLayout = PtrCast<VulkanResourceLayout>(layout.get());
            dynamicOffsetsCount += vkLayout->GetDynamicBufferCount();
        }*/

        
        ComPtr<ID3D12PipelineState> pipelineState;
        if(FAILED(dev->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))){
            return nullptr;
        }

        auto rawPipe = new DXCGraphicsPipeline(dev);
        rawPipe->_pso = std::move(pipelineState);
        switch (desc.primitiveTopology)
        {
            // A list of isolated, 3-element triangles.
            case PrimitiveTopology::TriangleList: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
            // A series of connected triangles.
            case PrimitiveTopology::TriangleStrip: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
            // A series of isolated, 2-element line segments.
            case PrimitiveTopology::LineList: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
            // A series of connected line segments.
            case PrimitiveTopology::LineStrip: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
            // A series of isolated points.
            case PrimitiveTopology::PointList: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        }
        //rawPipe->_devicePipeline = devicePipeline;
        //rawPipe->_pipelineLayout = pipelineLayout;
        //rawPipe->_renderPass = compatRenderPass;
        //rawPipe->scissorTestEnabled = rsDesc.scissorTestEnabled;
        //rawPipe->resourceSetCount = resourceSetCount;
        //rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;

        //For BlendFactor::BlendFactor: 
        //The blend factor is the blend factor set with 
        //ID3D12GraphicsCommandList::OMSetBlendFactor. No pre-blend operation.

        /*void OMSetBlendFactor(
            [in, optional] const FLOAT [4] BlendFactor
        );*/
        rawPipe->_blendConstants[0] = desc.blendState.blendConstant.r;
        rawPipe->_blendConstants[1] = desc.blendState.blendConstant.g;
        rawPipe->_blendConstants[2] = desc.blendState.blendConstant.b;
        rawPipe->_blendConstants[3] = desc.blendState.blendConstant.a;
        
        rawPipe->_refCnts = std::move(refCnts);

        return sp(rawPipe);
    }

} // namespace Veldrid
