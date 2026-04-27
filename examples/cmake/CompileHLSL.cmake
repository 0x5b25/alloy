#Usages:
# compile_shader(<target_name>
#    [SOURCES <hlsl_src>...
#        TYPES <shader_types>... #can be vs, ps, cs...
#        [WITH_DBG_INFO]        #preserve debug info in generated DXIL
#    ]...
# )
#

function(compile_shader TARGET_NAME)

    # Map short type names to entry point names
    macro(_get_entry_point TYPE OUT_VAR)
        string(REGEX MATCH "([a-z]+)_([0-9_]+)" _ ${TYPE})

        string(TOUPPER "${CMAKE_MATCH_1}" _UP)
        set(${OUT_VAR} "${_UP}Main")
    endmacro()

    set(HLSL_COMPILER ${DXC_EXECUTABLE_DIR}/Debug/dxc)
    set(HLSL_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

    file(MAKE_DIRECTORY ${HLSL_OUTPUT_DIR})

    set(SHADER_PRODUCTS)

    # ---------------------------------------------------------------
    # Parse the variadic argument list.
    # Grammar:  (SOURCES <file>... TYPES <type>... [WITH_DBG_INFO])*
    # We walk the list token-by-token with a small state machine.
    # ---------------------------------------------------------------
    set(_STATE "IDLE")           # IDLE | IN_SOURCES | IN_TYPES
    set(_CUR_SOURCES)
    set(_CUR_TYPES)
    set(_CUR_DBG OFF)

    # Helper macro: flush the current SOURCES/TYPES group
    macro(_flush_group)
        if(_CUR_SOURCES AND _CUR_TYPES)
            foreach(_SRC IN LISTS _CUR_SOURCES)
                cmake_path(ABSOLUTE_PATH _SRC NORMALIZE)
                cmake_path(GET _SRC STEM _SNAME)

                foreach(_TYPE IN LISTS _CUR_TYPES)
                    _get_entry_point(${_TYPE} _ENTRY)

                    set(_RESULT "${HLSL_OUTPUT_DIR}/${_SNAME}_${_TYPE}.h")

                    if(_CUR_DBG)
                        set(_DBG_FLAGS -Zi -Qembed_debug)
                    else()
                        set(_DBG_FLAGS)
                    endif()

                    add_custom_command(
                        OUTPUT ${_RESULT}
                        COMMAND ${HLSL_COMPILER}
                            -E ${_ENTRY}
                            -T ${_TYPE}
                            -Fh ${_RESULT}
                            ${_DBG_FLAGS}
                            ${_SRC}
                        DEPENDS ${_SRC}
                        COMMENT "Compiling ${_TYPE} shader from ${_SRC}"
                        VERBATIM
                    )

                    list(APPEND SHADER_PRODUCTS ${_RESULT})
                endforeach()
            endforeach()
        endif()

        set(_CUR_SOURCES)
        set(_CUR_TYPES)
        set(_CUR_DBG OFF)
    endmacro()

    foreach(_TOKEN IN ITEMS ${ARGN})
        if(_TOKEN STREQUAL "SOURCES")
            _flush_group()
            set(_STATE "IN_SOURCES")
        elseif(_TOKEN STREQUAL "TYPES")
            set(_STATE "IN_TYPES")
        elseif(_TOKEN STREQUAL "WITH_DBG_INFO")
            set(_CUR_DBG ON)
        elseif(_STATE STREQUAL "IN_SOURCES")
            list(APPEND _CUR_SOURCES ${_TOKEN})
        elseif(_STATE STREQUAL "IN_TYPES")
            list(APPEND _CUR_TYPES ${_TOKEN})
        endif()
    endforeach()

    # Flush the last group
    _flush_group()

    if(NOT SHADER_PRODUCTS)
        message(FATAL_ERROR "compile_shader(${TARGET_NAME}): no shader products generated. Check SOURCES/TYPES arguments.")
    endif()

    add_library(${TARGET_NAME} INTERFACE)
    target_sources(${TARGET_NAME} PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
        FILES ${SHADER_PRODUCTS})

endfunction()


# Usage:
#   add_shader_object_depends(<shader_target>
#       [TARGET_DIRECTORY <target>]
#       FILES <src1> [<src2> ...])
#
# Wires the generated shader headers owned by <shader_target> as explicit
# implicit dependencies (OBJECT_DEPENDS) of each listed source file. This
# makes Ninja know about the header -> object-file edge at planning time,
# so a single incremental build will both regenerate the headers and
# recompile the listed consumers in one pass.
#
# Arguments:
#   <shader_target>   A target created by compile_shader().
#   FILES             One or more C/C++ source files that #include the
#                     generated shader headers. Relative paths are
#                     resolved against CMAKE_CURRENT_SOURCE_DIR.
#   TARGET_DIRECTORY  (Optional) A target whose source directory should
#                     own the OBJECT_DEPENDS property. Use this when the
#                     helper is called from a different directory than
#                     the consumer target was defined in. Defaults to
#                     the current directory scope.
#
# Notes:
#   - Source-file properties have directory scope. By default the property
#     is set in the calling directory; pass TARGET_DIRECTORY to redirect
#     it to the consumer target's directory (requires CMake >= 3.18).
function(add_shader_object_depends SHADER_TARGET)
    cmake_parse_arguments(_ASOD
        ""                 # options
        "TARGET_DIRECTORY" # one-value
        "FILES"            # multi-value
        ${ARGN})

    if(NOT TARGET ${SHADER_TARGET})
        message(FATAL_ERROR
            "add_shader_object_depends: '${SHADER_TARGET}' is not a target.")
    endif()

    if(NOT _ASOD_FILES)
        message(FATAL_ERROR
            "add_shader_object_depends: FILES <src>... is required.")
    endif()

    if(_ASOD_TARGET_DIRECTORY AND NOT TARGET ${_ASOD_TARGET_DIRECTORY})
        message(FATAL_ERROR
            "add_shader_object_depends: TARGET_DIRECTORY "
            "'${_ASOD_TARGET_DIRECTORY}' is not a target.")
    endif()

    # Pull the generated shader headers out of the shader target's
    # HEADERS file set (populated by compile_shader()).
    get_target_property(_SHADER_HEADERS ${SHADER_TARGET}
        HEADER_SET_HEADERS)

    if(NOT _SHADER_HEADERS)
        # Fallback for CMake versions that expose the file set under a
        # different property name.
        get_target_property(_SHADER_HEADERS ${SHADER_TARGET}
            INTERFACE_HEADER_SETS_TO_VERIFY)
    endif()

    if(NOT _SHADER_HEADERS)
        message(FATAL_ERROR
            "add_shader_object_depends: could not extract shader headers "
            "from target '${SHADER_TARGET}'. Was it created with "
            "compile_shader()?")
    endif()

    # Normalize each consumer file path. Relative paths resolve against
    # the caller's CMAKE_CURRENT_SOURCE_DIR, matching the convention used
    # by add_executable / add_library.
    set(_CONSUMER_FILES)
    foreach(_F IN LISTS _ASOD_FILES)
        if(NOT IS_ABSOLUTE "${_F}")
            set(_F "${CMAKE_CURRENT_SOURCE_DIR}/${_F}")
        endif()
        cmake_path(NORMAL_PATH _F)
        list(APPEND _CONSUMER_FILES "${_F}")
    endforeach()

    # Apply OBJECT_DEPENDS, optionally redirecting to the consumer
    # target's directory scope so the property is visible to the
    # generator regardless of where this helper is called from.
    if(_ASOD_TARGET_DIRECTORY)
        set_source_files_properties(${_CONSUMER_FILES}
            TARGET_DIRECTORY ${_ASOD_TARGET_DIRECTORY}
            PROPERTIES
                OBJECT_DEPENDS "${_SHADER_HEADERS}")
    else()
        set_source_files_properties(${_CONSUMER_FILES}
            PROPERTIES
                OBJECT_DEPENDS "${_SHADER_HEADERS}")
    endif()
endfunction()
