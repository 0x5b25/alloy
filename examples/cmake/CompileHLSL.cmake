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
