cmake_minimum_required(VERSION 3.15)

project(spv2dxil)

set(MESA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Mesa")

#need some generated sources thus python is needed
find_package(PythonInterp REQUIRED)

function(GenSrc working_dir gen_script out_dir out_file)

    file(MAKE_DIRECTORY ${out_dir})

    message("     Script: ${gen_script}")
    message("Working dir: ${working_dir}")
    message("    Out dir: ${out_dir}")

    execute_process(
        #COMMAND "${PYTHON_EXECUTABLE} --version"#"${gen_script}"
        COMMAND "${PYTHON_EXECUTABLE}" "${gen_script}" ${ARGN}
        WORKING_DIRECTORY ${working_dir}
        OUTPUT_FILE  "${out_dir}/${out_file}"
        RESULT_VARIABLE _resVar
        #OUTPUT_VARIABLE _outVar
        ERROR_VARIABLE _errVar
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(NOT ${_resVar} STREQUAL 0)
        set(errstr ${_resVar})
        if(NOT ${_errVar} STREQUAL "")
            set(errstr ${_errVar})
        endif()
        message(SEND_ERROR "Command ${PYTHON_EXECUTABLE} ${gen_script} error : ${errstr}")
    endif()    

endfunction(GenSrc)

function(SetTargetNoWarnings targetName)

    target_compile_options(${targetName} PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:/W0 /WX>
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
    )

endfunction()

#shared link directories
set(inc_src "${MESA_SRC_DIR}/src")
set(inc_include "${MESA_SRC_DIR}/include")


set(inc_mesa        "${inc_src}/mesa")
set(inc_mapi        "${inc_src}/mapi")
set(inc_gallium     "${inc_src}/gallium/include")
set(inc_gallium_aux "${inc_src}/gallium/auxiliary")
set(inc_amd_common  "${inc_src}/amd/common")
set(inc_tool        "${inc_src}/tool")
set(inc_virtio_gpu  "${inc_src}/virtio/virtio-gpu")
set(inc_frontends   "${inc_src}/gallium/frontends")

#dummy target for git_sha1
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/git_sha1.h" "#define MESA_GIT_SHA1 \"\"")
set(sha1_h "${CMAKE_CURRENT_BINARY_DIR}")

########## Extracted build flags ##########

#if not env["vulkan"]:
#    # Some Mesa file needs to use Vulkan includes.
#    env_thirdparty_mesa.Append(CPPPATH="#thirdparty/vulkan/include")
#
#mesa_sources += [v for v in list(Path(mesa_absdir).rglob("*.c"))]
#mesa_sources += [v for v in list(Path(mesa_absdir).rglob("*.cpp"))]
#mesa_sources = [str(v).replace(mesa_absdir, mesa_dir) for v in mesa_sources]
#mesa_sources = [v.replace("\\", "/") for v in mesa_sources]
## Avoid build results depending on if generated files already exist.
#mesa_sources = [v for v in mesa_sources if not v.startswith(mesa_gen_dir)]
#mesa_sources.sort()
## Include the list of the generated ones now.
#mesa_sources += mesa_gen_sources
#env_thirdparty_mesa.add_source_files(thirdparty_obj, mesa_sources)
#
## Clean C/C++/both flags to avoid std clashes.
#for key in ["CFLAGS", "CXXFLAGS", "CCFLAGS"]:
#    env_thirdparty_mesa[key] = [v for v in env_thirdparty_mesa[key] if "/std:" not in v and "-std=" not in v]

# Added by ourselves.
set (extra_defines 
    "WINDOWS_NO_FUTEX"
)

# These defines are inspired by the Meson build scripts in the original repo.
list(APPEND extra_defines
    "__STDC_CONSTANT_MACROS"
    "__STDC_FORMAT_MACROS"
    "__STDC_LIMIT_MACROS"
    "PACKAGE_VERSION=\"${CMAKE_PROJECT_VERSION}\""#, '\\"' + Path(mesa_absdir + "/VERSION").read_text().strip() + '\\"'
    "PACKAGE_BUGREPORT=\"https://gitlab.freedesktop.org/mesa/mesa/-/issues\""
    "PIPE_SUBSYSTEM_WINDOWS_USER"
    "_USE_MATH_DEFINES"
    "_Static_assert=static_assert"
)

if(${MSVC})
    list(APPEND extra_defines
        "_USE_MATH_DEFINES"
        "VC_EXTRALEAN"
        "_CRT_SECURE_NO_WARNINGS"
        "_CRT_SECURE_NO_DEPRECATE"
        "_SCL_SECURE_NO_WARNINGS"
        "_SCL_SECURE_NO_DEPRECATE"
        "_ALLOW_KEYWORD_MACROS"
        "_HAS_EXCEPTIONS=0"
        "NOMINMAX"
        "HAVE_STRUCT_TIMESPEC"
    )
    #env_thirdparty_mesa.Append(CFLAGS=["/std:c11"])
    #env_thirdparty_mesa.Append(CXXFLAGS=["/std:c++17"])
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:/std:c++17>)
    add_compile_options( $<$<COMPILE_LANGUAGE:C>:/std:c11>)
else()
    #env_thirdparty_mesa.Append(
    #    CPPDEFINES=[
    #        ("__MSVCRT_VERSION__", 0x0700),
    #    ]
    #)
    list(APPEND extra_defines "__MSVCRT_VERSION__=0x0700")
    #env_thirdparty_mesa.Append(CFLAGS=["-std=c11"])
    #env_thirdparty_mesa.Append(CXXFLAGS=["-std=cpp++17"])
    add_compile_options( $<$<COMPILE_LANGUAGE:CXX>:-std=cpp++17>)
    add_compile_options( $<$<COMPILE_LANGUAGE:C>:-std=c11>)

endif()
#env_thirdparty_mesa.Append(CPPDEFINES=extra_defines)

# This is needed since rendering_device_d3d12.cpp needs to include some Mesa internals.
#env_d3d12_rd.Prepend(CPPPATH=mesa_private_inc_paths)
# For the same reason as above, the defines must be the same as in the 3rd-party code itself.
#env_d3d12_rd.Append(CPPDEFINES=extra_defines)

# No point in fighting warnings in Mesa.

# Add all.
add_compile_definitions(${extra_defines})

###########################################



##########    compiler    ##########
set(MESA_COMPILER_PATH "${MESA_SRC_DIR}/src/compiler")
set(COMPILER_GENFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/compiler")

set(inc_compiler ${MESA_COMPILER_PATH})

set(files_libcompiler
    "${MESA_COMPILER_PATH}/builtin_type_macros.h"
    "${MESA_COMPILER_PATH}/glsl_types.cpp"
    "${MESA_COMPILER_PATH}/glsl_types.h"
    "${MESA_COMPILER_PATH}/nir_types.cpp"
    "${MESA_COMPILER_PATH}/nir_types.h"
    "${MESA_COMPILER_PATH}/shader_enums.c"
    "${MESA_COMPILER_PATH}/shader_enums.h"
    "${MESA_COMPILER_PATH}/shader_info.h"
)


#GenSrc("${MESA_COMPILER_PATH}" "glsl/ir_expression_operation.py" "${COMPILER_GENFILE_DIR}/ir_expression_operation.h")
#set(ir_expression_operation_h "${COMPILER_GENFILE_DIR}/ir_expression_operation.h")

add_library(compiler
    ${files_libcompiler}
    #${ir_expression_operation_h}
)

SetTargetNoWarnings(compiler)

target_link_libraries(compiler
    PRIVATE
        #dep_valgrind
)

target_include_directories(compiler
    PRIVATE
        ${inc_mapi}
        ${inc_mesa}
        ${inc_compiler}
        ${inc_include}
        ${inc_src}
        ${inc_gallium}
        ${inc_gallium_aux}
)


##########  compiler/nir  ##########
set(MESA_NIR_PATH "${MESA_COMPILER_PATH}/nir")

set(NIR_GENFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/compiler/nir")

function(GenNIRSrc var_name gen_script out_file )

    GenSrc("${MESA_NIR_PATH}" ${gen_script} ${NIR_GENFILE_DIR} ${out_file} ${ARGN})

    set(${var_name} "${NIR_GENFILE_DIR}/${out_file}" PARENT_SCOPE)

endfunction(GenNIRSrc)

GenNIRSrc("nir_builder_opcodes_h" "nir_builder_opcodes_h.py" "nir_builder_opcodes.h")
GenNIRSrc("nir_constant_expressions_c" "nir_constant_expressions.py" "nir_constant_expressions.c")
GenNIRSrc("nir_opcodes_h" "nir_opcodes_h.py" "nir_opcodes.h")
GenNIRSrc("nir_opcodes_c" "nir_opcodes_c.py" "nir_opcodes.c")
GenNIRSrc("nir_opt_algebraic_c" "nir_opt_algebraic.py" "nir_opt_algebraic.c")


file(MAKE_DIRECTORY ${NIR_GENFILE_DIR})

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "nir_intrinsics_h.py" "--outdir" ${NIR_GENFILE_DIR}
    WORKING_DIRECTORY ${MESA_NIR_PATH}
)
set(nir_intrinsics_h "${NIR_GENFILE_DIR}/nir_intrinsics_indices.h")

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "nir_intrinsics_indices_h.py" "--outdir" ${NIR_GENFILE_DIR}
    WORKING_DIRECTORY ${MESA_NIR_PATH}
)
set(nir_intrinsics_indices_h "${NIR_GENFILE_DIR}/nir_intrinsics_indices.h")

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "nir_intrinsics_c.py" "--outdir" ${NIR_GENFILE_DIR}
    WORKING_DIRECTORY ${MESA_NIR_PATH}
)
set(nir_intrinsics_c "${NIR_GENFILE_DIR}/nir_intrinsics.c")


set(files_libnir
    "${MESA_NIR_PATH}/nir.c"
    "${MESA_NIR_PATH}/nir.h"
    "${MESA_NIR_PATH}/nir_builder.c"
    "${MESA_NIR_PATH}/nir_builder.h"
    "${MESA_NIR_PATH}/nir_builtin_builder.c"
    "${MESA_NIR_PATH}/nir_builtin_builder.h"
    "${MESA_NIR_PATH}/nir_conversion_builder.h"
    "${MESA_NIR_PATH}/nir_clone.c"
    "${MESA_NIR_PATH}/nir_constant_expressions.h"
    "${MESA_NIR_PATH}/nir_control_flow.c"
    "${MESA_NIR_PATH}/nir_control_flow.h"
    "${MESA_NIR_PATH}/nir_control_flow_private.h"
    "${MESA_NIR_PATH}/nir_deref.c"
    "${MESA_NIR_PATH}/nir_deref.h"
    "${MESA_NIR_PATH}/nir_divergence_analysis.c"
    "${MESA_NIR_PATH}/nir_dominance.c"
    "${MESA_NIR_PATH}/nir_format_convert.h"
    "${MESA_NIR_PATH}/nir_from_ssa.c"
    "${MESA_NIR_PATH}/nir_gather_info.c"
    "${MESA_NIR_PATH}/nir_gather_ssa_types.c"
    "${MESA_NIR_PATH}/nir_gather_xfb_info.c"
    "${MESA_NIR_PATH}/nir_group_loads.c"
    "${MESA_NIR_PATH}/nir_gs_count_vertices.c"
    "${MESA_NIR_PATH}/nir_inline_functions.c"
    "${MESA_NIR_PATH}/nir_inline_uniforms.c"
    "${MESA_NIR_PATH}/nir_instr_set.c"
    "${MESA_NIR_PATH}/nir_instr_set.h"
    "${MESA_NIR_PATH}/nir_legacy.c"
    "${MESA_NIR_PATH}/nir_legacy.h"
    "${MESA_NIR_PATH}/nir_linking_helpers.c"
    "${MESA_NIR_PATH}/nir_liveness.c"
    "${MESA_NIR_PATH}/nir_loop_analyze.c"
    "${MESA_NIR_PATH}/nir_loop_analyze.h"
    "${MESA_NIR_PATH}/nir_lower_alu.c"
    "${MESA_NIR_PATH}/nir_lower_alu_width.c"
    "${MESA_NIR_PATH}/nir_lower_alpha_test.c"
    "${MESA_NIR_PATH}/nir_lower_amul.c"
    "${MESA_NIR_PATH}/nir_lower_array_deref_of_vec.c"
    "${MESA_NIR_PATH}/nir_lower_atomics_to_ssbo.c"
    "${MESA_NIR_PATH}/nir_lower_bitmap.c"
    "${MESA_NIR_PATH}/nir_lower_blend.c"
    "${MESA_NIR_PATH}/nir_lower_bool_to_bitsize.c"
    "${MESA_NIR_PATH}/nir_lower_bool_to_float.c"
    "${MESA_NIR_PATH}/nir_lower_bool_to_int32.c"
    "${MESA_NIR_PATH}/nir_lower_cl_images.c"
    "${MESA_NIR_PATH}/nir_lower_clamp_color_outputs.c"
    "${MESA_NIR_PATH}/nir_lower_clip.c"
    "${MESA_NIR_PATH}/nir_lower_clip_cull_distance_arrays.c"
    "${MESA_NIR_PATH}/nir_lower_clip_disable.c"
    "${MESA_NIR_PATH}/nir_lower_clip_halfz.c"
    "${MESA_NIR_PATH}/nir_lower_const_arrays_to_uniforms.c"
    "${MESA_NIR_PATH}/nir_lower_continue_constructs.c"
    "${MESA_NIR_PATH}/nir_lower_convert_alu_types.c"
    "${MESA_NIR_PATH}/nir_lower_variable_initializers.c"
    "${MESA_NIR_PATH}/nir_lower_discard_if.c"
    "${MESA_NIR_PATH}/nir_lower_discard_or_demote.c"
    "${MESA_NIR_PATH}/nir_lower_double_ops.c"
    "${MESA_NIR_PATH}/nir_lower_drawpixels.c"
    "${MESA_NIR_PATH}/nir_lower_fb_read.c"
    "${MESA_NIR_PATH}/nir_lower_flatshade.c"
    "${MESA_NIR_PATH}/nir_lower_flrp.c"
    "${MESA_NIR_PATH}/nir_lower_fp16_conv.c"
    "${MESA_NIR_PATH}/nir_lower_fragcoord_wtrans.c"
    "${MESA_NIR_PATH}/nir_lower_frag_coord_to_pixel_coord.c"
    "${MESA_NIR_PATH}/nir_lower_fragcolor.c"
    "${MESA_NIR_PATH}/nir_lower_frexp.c"
    "${MESA_NIR_PATH}/nir_lower_global_vars_to_local.c"
    "${MESA_NIR_PATH}/nir_lower_goto_ifs.c"
    "${MESA_NIR_PATH}/nir_lower_gs_intrinsics.c"
    "${MESA_NIR_PATH}/nir_lower_helper_writes.c"
    "${MESA_NIR_PATH}/nir_lower_load_const_to_scalar.c"
    "${MESA_NIR_PATH}/nir_lower_locals_to_regs.c"
    "${MESA_NIR_PATH}/nir_lower_idiv.c"
    "${MESA_NIR_PATH}/nir_lower_image.c"
    "${MESA_NIR_PATH}/nir_lower_image_atomics_to_global.c"
    "${MESA_NIR_PATH}/nir_lower_indirect_derefs.c"
    "${MESA_NIR_PATH}/nir_lower_input_attachments.c"
    "${MESA_NIR_PATH}/nir_lower_int64.c"
    "${MESA_NIR_PATH}/nir_lower_interpolation.c"
    "${MESA_NIR_PATH}/nir_lower_int_to_float.c"
    "${MESA_NIR_PATH}/nir_lower_io.c"
    "${MESA_NIR_PATH}/nir_lower_io_arrays_to_elements.c"
    "${MESA_NIR_PATH}/nir_lower_io_to_temporaries.c"
    "${MESA_NIR_PATH}/nir_lower_io_to_scalar.c"
    "${MESA_NIR_PATH}/nir_lower_io_to_vector.c"
    "${MESA_NIR_PATH}/nir_lower_is_helper_invocation.c"
    "${MESA_NIR_PATH}/nir_lower_multiview.c"
    "${MESA_NIR_PATH}/nir_lower_mediump.c"
    "${MESA_NIR_PATH}/nir_lower_mem_access_bit_sizes.c"
    "${MESA_NIR_PATH}/nir_lower_memcpy.c"
    "${MESA_NIR_PATH}/nir_lower_memory_model.c"
    "${MESA_NIR_PATH}/nir_lower_non_uniform_access.c"
    "${MESA_NIR_PATH}/nir_lower_packing.c"
    "${MESA_NIR_PATH}/nir_lower_passthrough_edgeflags.c"
    "${MESA_NIR_PATH}/nir_lower_patch_vertices.c"
    "${MESA_NIR_PATH}/nir_lower_phis_to_scalar.c"
    "${MESA_NIR_PATH}/nir_lower_pntc_ytransform.c"
    "${MESA_NIR_PATH}/nir_lower_point_size.c"
    "${MESA_NIR_PATH}/nir_lower_point_size_mov.c"
    "${MESA_NIR_PATH}/nir_lower_point_smooth.c"
    "${MESA_NIR_PATH}/nir_lower_poly_line_smooth.c"
    "${MESA_NIR_PATH}/nir_lower_printf.c"
    "${MESA_NIR_PATH}/nir_lower_reg_intrinsics_to_ssa.c"
    "${MESA_NIR_PATH}/nir_lower_readonly_images_to_tex.c"
    "${MESA_NIR_PATH}/nir_lower_returns.c"
    "${MESA_NIR_PATH}/nir_lower_robust_access.c"
    "${MESA_NIR_PATH}/nir_lower_samplers.c"
    "${MESA_NIR_PATH}/nir_lower_scratch.c"
    "${MESA_NIR_PATH}/nir_lower_shader_calls.c"
    "${MESA_NIR_PATH}/nir_lower_single_sampled.c"
    "${MESA_NIR_PATH}/nir_lower_ssbo.c"
    "${MESA_NIR_PATH}/nir_lower_subgroups.c"
    "${MESA_NIR_PATH}/nir_lower_system_values.c"
    "${MESA_NIR_PATH}/nir_lower_task_shader.c"
    "${MESA_NIR_PATH}/nir_lower_tess_coord_z.c"
    "${MESA_NIR_PATH}/nir_lower_tex_shadow.c"
    "${MESA_NIR_PATH}/nir_lower_tex.c"
    "${MESA_NIR_PATH}/nir_lower_texcoord_replace.c"
    "${MESA_NIR_PATH}/nir_lower_texcoord_replace_late.c"
    "${MESA_NIR_PATH}/nir_lower_to_source_mods.c"
    "${MESA_NIR_PATH}/nir_lower_two_sided_color.c"
    "${MESA_NIR_PATH}/nir_lower_undef_to_zero.c"
    "${MESA_NIR_PATH}/nir_lower_vars_to_ssa.c"
    "${MESA_NIR_PATH}/nir_lower_var_copies.c"
    "${MESA_NIR_PATH}/nir_lower_vec_to_movs.c"
    "${MESA_NIR_PATH}/nir_lower_vec_to_regs.c"
    "${MESA_NIR_PATH}/nir_lower_vec3_to_vec4.c"
    "${MESA_NIR_PATH}/nir_lower_viewport_transform.c"
    "${MESA_NIR_PATH}/nir_lower_wpos_center.c"
    "${MESA_NIR_PATH}/nir_lower_wpos_ytransform.c"
    "${MESA_NIR_PATH}/nir_lower_wrmasks.c"
    "${MESA_NIR_PATH}/nir_lower_bit_size.c"
    "${MESA_NIR_PATH}/nir_lower_ubo_vec4.c"
    "${MESA_NIR_PATH}/nir_lower_uniforms_to_ubo.c"
    "${MESA_NIR_PATH}/nir_lower_sysvals_to_varyings.c"
    "${MESA_NIR_PATH}/nir_metadata.c"
    "${MESA_NIR_PATH}/nir_mod_analysis.c"
    "${MESA_NIR_PATH}/nir_move_vec_src_uses_to_dest.c"
    "${MESA_NIR_PATH}/nir_normalize_cubemap_coords.c"
    "${MESA_NIR_PATH}/nir_opt_access.c"
    "${MESA_NIR_PATH}/nir_opt_barriers.c"
    "${MESA_NIR_PATH}/nir_opt_combine_stores.c"
    "${MESA_NIR_PATH}/nir_opt_comparison_pre.c"
    "${MESA_NIR_PATH}/nir_opt_conditional_discard.c"
    "${MESA_NIR_PATH}/nir_opt_constant_folding.c"
    "${MESA_NIR_PATH}/nir_opt_copy_prop_vars.c"
    "${MESA_NIR_PATH}/nir_opt_copy_propagate.c"
    "${MESA_NIR_PATH}/nir_opt_cse.c"
    "${MESA_NIR_PATH}/nir_opt_dce.c"
    "${MESA_NIR_PATH}/nir_opt_dead_cf.c"
    "${MESA_NIR_PATH}/nir_opt_dead_write_vars.c"
    "${MESA_NIR_PATH}/nir_opt_find_array_copies.c"
    "${MESA_NIR_PATH}/nir_opt_fragdepth.c"
    "${MESA_NIR_PATH}/nir_opt_gcm.c"
    "${MESA_NIR_PATH}/nir_opt_idiv_const.c"
    "${MESA_NIR_PATH}/nir_opt_if.c"
    "${MESA_NIR_PATH}/nir_opt_intrinsics.c"
    "${MESA_NIR_PATH}/nir_opt_large_constants.c"
    "${MESA_NIR_PATH}/nir_opt_load_store_vectorize.c"
    "${MESA_NIR_PATH}/nir_opt_loop_unroll.c"
    "${MESA_NIR_PATH}/nir_opt_memcpy.c"
    "${MESA_NIR_PATH}/nir_opt_move.c"
    "${MESA_NIR_PATH}/nir_opt_move_discards_to_top.c"
    "${MESA_NIR_PATH}/nir_opt_non_uniform_access.c"
    "${MESA_NIR_PATH}/nir_opt_offsets.c"
    "${MESA_NIR_PATH}/nir_opt_peephole_select.c"
    "${MESA_NIR_PATH}/nir_opt_phi_precision.c"
    "${MESA_NIR_PATH}/nir_opt_preamble.c"
    "${MESA_NIR_PATH}/nir_opt_ray_queries.c"
    "${MESA_NIR_PATH}/nir_opt_reassociate_bfi.c"
    "${MESA_NIR_PATH}/nir_opt_rematerialize_compares.c"
    "${MESA_NIR_PATH}/nir_opt_remove_phis.c"
    "${MESA_NIR_PATH}/nir_opt_shrink_stores.c"
    "${MESA_NIR_PATH}/nir_opt_shrink_vectors.c"
    "${MESA_NIR_PATH}/nir_opt_sink.c"
    "${MESA_NIR_PATH}/nir_opt_trivial_continues.c"
    "${MESA_NIR_PATH}/nir_opt_undef.c"
    "${MESA_NIR_PATH}/nir_opt_uniform_atomics.c"
    "${MESA_NIR_PATH}/nir_opt_vectorize.c"
    "${MESA_NIR_PATH}/nir_passthrough_gs.c"
    "${MESA_NIR_PATH}/nir_passthrough_tcs.c"
    "${MESA_NIR_PATH}/nir_phi_builder.c"
    "${MESA_NIR_PATH}/nir_phi_builder.h"
    "${MESA_NIR_PATH}/nir_print.c"
    "${MESA_NIR_PATH}/nir_propagate_invariant.c"
    "${MESA_NIR_PATH}/nir_range_analysis.c"
    "${MESA_NIR_PATH}/nir_range_analysis.h"
    "${MESA_NIR_PATH}/nir_remove_dead_variables.c"
    "${MESA_NIR_PATH}/nir_remove_tex_shadow.c"
    "${MESA_NIR_PATH}/nir_repair_ssa.c"
    "${MESA_NIR_PATH}/nir_scale_fdiv.c"
    "${MESA_NIR_PATH}/nir_schedule.c"
    "${MESA_NIR_PATH}/nir_schedule.h"
    "${MESA_NIR_PATH}/nir_search.c"
    "${MESA_NIR_PATH}/nir_search.h"
    "${MESA_NIR_PATH}/nir_search_helpers.h"
    "${MESA_NIR_PATH}/nir_serialize.c"
    "${MESA_NIR_PATH}/nir_serialize.h"
    "${MESA_NIR_PATH}/nir_split_64bit_vec3_and_vec4.c"
    "${MESA_NIR_PATH}/nir_split_per_member_structs.c"
    "${MESA_NIR_PATH}/nir_split_var_copies.c"
    "${MESA_NIR_PATH}/nir_split_vars.c"
    "${MESA_NIR_PATH}/nir_sweep.c"
    "${MESA_NIR_PATH}/nir_to_lcssa.c"
    "${MESA_NIR_PATH}/nir_trivialize_registers.c"
    "${MESA_NIR_PATH}/nir_validate.c"
    "${MESA_NIR_PATH}/nir_vla.h"
    "${MESA_NIR_PATH}/nir_worklist.c"
    "${MESA_NIR_PATH}/nir_worklist.h"
    "${MESA_NIR_PATH}/nir_xfb_info.h"
)

add_library(nir
    ${files_libnir}
    ${nir_opt_algebraic_c}
    ${nir_opcodes_c}
    ${nir_opcodes_h}
    ${nir_constant_expressions_c}
    ${nir_builder_opcodes_h}
    ${nir_intrinsics_c}
    ${nir_intrinsics_h}
    ${nir_intrinsics_indices_h}
  #include_directories : [inc_include, inc_src, inc_mapi, inc_mesa, inc_gallium, inc_gallium_aux, inc_compiler],
  #c_args : [c_msvc_compat_args, no_override_init_args],
  #gnu_symbol_visibility : 'hidden',
  #dependencies : dep_valgrind,
  #link_with : libcompiler,
  #build_by_default : false,
)

SetTargetNoWarnings(nir)

# Headers-only dependency
add_library(idep_nir_headers INTERFACE)

target_sources(idep_nir_headers
    INTERFACE
        ${nir_opcodes_h}
        ${nir_builder_opcodes_h}
        ${nir_intrinsics_h}
        ${nir_intrinsics_indices_h}
)

target_include_directories(idep_nir_headers
    INTERFACE
        ${MESA_NIR_PATH}
        ${NIR_GENFILE_DIR}
)

# Also link with nir
target_link_libraries(nir 
    PRIVATE
        #dep_valgrind
    PUBLIC
        idep_nir_headers
        idep_mesautil
        compiler
)

target_include_directories(nir
    PRIVATE
        ${inc_include}
        ${inc_src}
        ${inc_mapi}
        ${inc_mesa}
        ${inc_gallium}
        ${inc_gallium_aux}
        ${inc_compiler}
        ${MESA_NIR_PATH}
    PUBLIC
        ${NIR_GENFILE_DIR}
)


##########  compiler/spirv  ##########
set(MESA_SPV_PATH "${MESA_COMPILER_PATH}/spirv")

set(SPV_GENFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/compiler/spirv")


function(GenSPVSrc var_name gen_script out_file)

    file(MAKE_DIRECTORY ${SPV_GENFILE_DIR})

    execute_process(
        COMMAND "${PYTHON_EXECUTABLE}" "${gen_script}" ${ARGN} "${SPV_GENFILE_DIR}/${out_file}"
        WORKING_DIRECTORY ${MESA_SPV_PATH}
    )

    set(${var_name} "${SPV_GENFILE_DIR}/${out_file}" PARENT_SCOPE)

endfunction(GenSPVSrc)


GenSPVSrc(vtn_gather_types_c "vtn_gather_types_c.py" "vtn_gather_types.c" "spirv.core.grammar.json" )
GenSPVSrc(spirv_info_c "spirv_info_c.py" "spirv_info.c" "spirv.core.grammar.json" )
GenSPVSrc(vtn_generator_ids_h "vtn_generator_ids_h.py" "vtn_generator_ids.h" "spir-v.xml" )

set(files_libvtn
    "${MESA_SPV_PATH}/GLSL.ext.AMD.h"
    "${MESA_SPV_PATH}/GLSL.std.450.h"
    "${MESA_SPV_PATH}/OpenCL.std.h"
    "${MESA_SPV_PATH}/gl_spirv.c"
    "${MESA_SPV_PATH}/nir_spirv.h"
    "${MESA_SPV_PATH}/spirv.h"
    "${MESA_SPV_PATH}/spirv_info.h"
    "${MESA_SPV_PATH}/spirv_to_nir.c"
    "${MESA_SPV_PATH}/vtn_alu.c"
    "${MESA_SPV_PATH}/vtn_amd.c"
    "${MESA_SPV_PATH}/vtn_cfg.c"
    "${MESA_SPV_PATH}/vtn_glsl450.c"
    "${MESA_SPV_PATH}/vtn_opencl.c"
    "${MESA_SPV_PATH}/vtn_private.h"
    "${MESA_SPV_PATH}/vtn_structured_cfg.c"
    "${MESA_SPV_PATH}/vtn_subgroup.c"
    "${MESA_SPV_PATH}/vtn_variables.c"
)

add_library(vtn
    ${files_libvtn}
    ${spirv_info_c}
    ${vtn_gather_types_c}
    ${vtn_generator_ids_h}
)

SetTargetNoWarnings(vtn)

#libvtn = static_library(
#  'vtn',
#  [files_libvtn,
#   spirv_info_c,
#   vtn_gather_types_c,
#   vtn_generator_ids_h,
#  ],
#  include_directories : [inc_include, inc_src, inc_mapi, inc_mesa, inc_compiler],
#  c_args : [c_msvc_compat_args, no_override_init_args],
#  gnu_symbol_visibility : 'hidden',
#  dependencies : [idep_nir, dep_valgrind],
#  link_with : libcompiler,
#  build_by_default : false,
#)
#
#idep_vtn = declare_dependency(
#  dependencies : [idep_nir, idep_mesautil],
#  link_with : libvtn,
#)


target_link_libraries(vtn 
    PRIVATE
        #dep_valgrind
    PUBLIC
        nir
        idep_mesautil
)

target_include_directories(vtn
    PRIVATE
        ${inc_include}
        ${inc_src}
        ${inc_mapi}
        ${inc_mesa}
        ${inc_gallium}
        ${inc_compiler}
    PUBLIC
        ${MESA_SPV_PATH}
        ${SPV_GENFILE_DIR}
)


########## dozen ##########

set(MESA_DZN_PATH "${MESA_SRC_DIR}/src/microsoft")
set(MESA_DZN_PATH_spv2dxil "${MESA_DZN_PATH}/spirv_to_dxil")
set(MESA_DZN_PATH_compiler "${MESA_DZN_PATH}/compiler")

#dxil_compiler
set(DZN_DXILC_PATH "${MESA_DZN_PATH}/compiler")

set(DXILC_GENFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/microsoft/compiler")

file(MAKE_DIRECTORY ${DXILC_GENFILE_DIR})

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "dxil_nir_algebraic.py" "-p" ${MESA_NIR_PATH}
    OUTPUT_FILE  "${DXILC_GENFILE_DIR}/dxil_nir_algebraic.c"
    WORKING_DIRECTORY ${DZN_DXILC_PATH}
)
set(dxil_nir_algebraic_c "${DXILC_GENFILE_DIR}/dxil_nir_algebraic.c")

add_library(dxil_compiler
    "${MESA_DZN_PATH_compiler}/dxil_buffer.c"
    "${MESA_DZN_PATH_compiler}/dxil_container.c"
    "${MESA_DZN_PATH_compiler}/dxil_dump.c"
    "${MESA_DZN_PATH_compiler}/dxil_enums.c"
    "${MESA_DZN_PATH_compiler}/dxil_function.c"
    "${MESA_DZN_PATH_compiler}/dxil_module.c"
    "${MESA_DZN_PATH_compiler}/dxil_nir.c"
    "${MESA_DZN_PATH_compiler}/dxil_nir_lower_int_cubemaps.c"
    "${MESA_DZN_PATH_compiler}/dxil_nir_lower_int_samplers.c"
    "${MESA_DZN_PATH_compiler}/dxil_nir_lower_vs_vertex_conversion.c"
    "${MESA_DZN_PATH_compiler}/dxil_signature.c"
    "${MESA_DZN_PATH_compiler}/dxil_nir_tess.c"
    "${MESA_DZN_PATH_compiler}/nir_to_dxil.c"
#if host_machine.system() == 'windows'
    "${MESA_DZN_PATH_compiler}/dxil_validator.cpp"
#endif
    ${dxil_nir_algebraic_c}
    
)

SetTargetNoWarnings(dxil_compiler)

target_link_libraries(dxil_compiler 
    PRIVATE
        idep_nir_headers
        #dep_valgrind
        
    PUBLIC
        idep_mesautil
        dep_version
)

target_include_directories(dxil_compiler
    PRIVATE
        ${inc_include}
        ${inc_src}
        ${inc_mapi}
        ${inc_mesa}
        ${inc_compiler}
        ${inc_gallium}
        ${sha1_h}
    PUBLIC
        ${MESA_DZN_PATH_compiler}
)

#libspirv_to_dxil

add_library(spirv_to_dxil

    "${MESA_DZN_PATH_spv2dxil}/dxil_spirv_nir.c"
    "${MESA_DZN_PATH_spv2dxil}/dxil_spirv_nir.h"
    "${MESA_DZN_PATH_spv2dxil}/dxil_spirv_nir_lower_bindless.c"
    "${MESA_DZN_PATH_spv2dxil}/spirv_to_dxil.c"
    "${MESA_DZN_PATH_spv2dxil}/spirv_to_dxil.h"
    
)


SetTargetNoWarnings(spirv_to_dxil)

target_link_libraries(spirv_to_dxil 
    PRIVATE
        
        vtn
        dxil_compiler
    PUBLIC
        nir
)

target_include_directories(spirv_to_dxil
    PRIVATE
        ${inc_include}
        ${inc_src}
        ${inc_compiler}
        ${inc_gallium}
        ${sha1_h}
    PUBLIC
        ${MESA_DZN_PATH_spv2dxil}
)

#add_executable(spirv2dxil 
#    "${MESA_DZN_PATH_spv2dxil}/spirv2dxil.c"
#)
#
#target_link_libraries(spirv2dxil
#    PRIVATE
#        nir
#        idep_libdxil_compiler
#        idep_getopt
#        spirv_to_dxil
#)


#add_executable(demo 
#    "${CMAKE_CURRENT_SOURCE_DIR}/demo.cpp"
#)

#target_link_libraries(demo
#    PRIVATE
#        #nir
#        #idep_libdxil_compiler
#        #idep_getopt
#        spirv_to_dxil
#        vtn
#        dxil_compiler
#)

