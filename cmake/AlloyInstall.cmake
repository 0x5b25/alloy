include(GNUInstallDirs)

function(_alloy_get_component_install_args OUT_VAR COMPONENT_NAME)
    if("${COMPONENT_NAME}" STREQUAL "" OR "${COMPONENT_NAME}" STREQUAL "NO")
        set(${OUT_VAR} "" PARENT_SCOPE)
    else()
        set(${OUT_VAR} COMPONENT "${COMPONENT_NAME}" PARENT_SCOPE)
    endif()
endfunction()

function(alloy_install_runtime_deps)
    set(oneValueArgs
        COMPONENT
        RUNTIME_DESTINATION
        DX12_DESTINATION
        APPLE_FRAMEWORK_DESTINATION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "alloy_install_runtime_deps: unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT ARG_RUNTIME_DESTINATION)
        set(ARG_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()

    if(NOT ARG_DX12_DESTINATION)
        set(ARG_DX12_DESTINATION "${ARG_RUNTIME_DESTINATION}/D3D12")
    endif()

    if(NOT APPLE_FRAMEWORK_DESTINATION)
        set(APPLE_FRAMEWORK_DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    endif()

    set(_component_arg)
    if(ARG_COMPONENT)
        list(APPEND _component_arg COMPONENT "${ARG_COMPONENT}")
    endif()

    set(_backend_dxc_enabled FALSE)
    set(_backend_vk_enabled FALSE)
    set(_backend_mtl_enabled FALSE)

    if(TARGET Veldrid)
        get_target_property(_backend_dxc_enabled Veldrid BACKEND_DXC)
        get_target_property(_backend_vk_enabled Veldrid BACKEND_VK)
        get_target_property(_backend_mtl_enabled Veldrid BACKEND_MTL)
    else()
        set(_backend_dxc_enabled ${VLD_BACKEND_DXC})
        set(_backend_vk_enabled ${VLD_BACKEND_VK})
        set(_backend_mtl_enabled ${VLD_BACKEND_MTL})
    endif()

    if(NOT _backend_dxc_enabled)
        set(_backend_dxc_enabled FALSE)
    endif()

    if(NOT _backend_vk_enabled)
        set(_backend_vk_enabled FALSE)
    endif()

    if(NOT _backend_mtl_enabled)
        set(_backend_mtl_enabled FALSE)
    endif()

    if(_backend_dxc_enabled AND COMMAND alloy_install_dxc_runtime_deps)
        alloy_install_dxc_runtime_deps(
            ${_component_arg}
            RUNTIME_DESTINATION "${ARG_RUNTIME_DESTINATION}"
            DX12_DESTINATION "${ARG_DX12_DESTINATION}")
    endif()

    if(_backend_vk_enabled AND COMMAND alloy_install_vulkan_runtime_deps)
        alloy_install_vulkan_runtime_deps(
            ${_component_arg}
            RUNTIME_DESTINATION "${ARG_RUNTIME_DESTINATION}")
    endif()

    if(_backend_mtl_enabled AND COMMAND alloy_install_metal_runtime_deps)
        alloy_install_metal_runtime_deps(
            ${_component_arg}
            APPLE_FRAMEWORK_DESTINATION "${ARG_APPLE_FRAMEWORK_DESTINATION}")
    endif()
endfunction()

function(_alloy_get_bundle_name TARGET_NAME OUT_VAR)
    get_target_property(_output_name ${TARGET_NAME} OUTPUT_NAME)
    if(NOT _output_name)
        set(_output_name "${TARGET_NAME}")
    endif()

    get_target_property(_bundle_extension ${TARGET_NAME} BUNDLE_EXTENSION)
    if(NOT _bundle_extension)
        set(_bundle_extension "app")
    endif()

    if(_bundle_extension MATCHES "^\\.")
        set(${OUT_VAR} "${_output_name}${_bundle_extension}" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "${_output_name}.${_bundle_extension}" PARENT_SCOPE)
    endif()
endfunction()

function(alloy_install_target TARGET_NAME)
    set(oneValueArgs
        COMPONENT
        RUNTIME_DESTINATION
        BUNDLE_DESTINATION
        DX12_DESTINATION
        APPLE_FRAMEWORK_DESTINATION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "alloy_install_target: unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "alloy_install_target: '${TARGET_NAME}' is not a target")
    endif()

    get_target_property(_target_type ${TARGET_NAME} TYPE)
    if(NOT _target_type STREQUAL "EXECUTABLE")
        message(FATAL_ERROR
            "alloy_install_target: '${TARGET_NAME}' must be an executable target")
    endif()

    if(NOT ARG_COMPONENT)
        set(ARG_COMPONENT "${TARGET_NAME}")
    endif()

    if(NOT ARG_RUNTIME_DESTINATION)
        set(ARG_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()

    if(NOT ARG_BUNDLE_DESTINATION)
        set(ARG_BUNDLE_DESTINATION ".")
    endif()

    _alloy_get_component_install_args(_component_install_args "${ARG_COMPONENT}")

    install(TARGETS ${TARGET_NAME}
        RUNTIME DESTINATION "${ARG_RUNTIME_DESTINATION}" ${_component_install_args}
        BUNDLE DESTINATION "${ARG_BUNDLE_DESTINATION}" ${_component_install_args})

    set(_runtime_deps_args
        COMPONENT "${ARG_COMPONENT}"
        RUNTIME_DESTINATION "${ARG_RUNTIME_DESTINATION}")

    if(ARG_DX12_DESTINATION)
        list(APPEND _runtime_deps_args DX12_DESTINATION "${ARG_DX12_DESTINATION}")
    endif()

    if(ARG_APPLE_FRAMEWORK_DESTINATION)
        list(APPEND _runtime_deps_args
            APPLE_FRAMEWORK_DESTINATION "${ARG_APPLE_FRAMEWORK_DESTINATION}")
    elseif(APPLE)
        get_target_property(_is_bundle ${TARGET_NAME} MACOSX_BUNDLE)
        if(_is_bundle)
            _alloy_get_bundle_name(${TARGET_NAME} _bundle_name)
            if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
                set(_framework_destination
                    "${ARG_BUNDLE_DESTINATION}/${_bundle_name}/Frameworks")
            else()
                set(_framework_destination
                    "${ARG_BUNDLE_DESTINATION}/${_bundle_name}/Contents/Frameworks")
            endif()
        else()
            set(_framework_destination "${ARG_RUNTIME_DESTINATION}")
        endif()

        list(APPEND _runtime_deps_args
            APPLE_FRAMEWORK_DESTINATION "${_framework_destination}")
    endif()

    alloy_install_runtime_deps(${_runtime_deps_args})
endfunction()