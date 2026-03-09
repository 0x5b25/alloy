# Using fetch content for 3rd-party libraries 
# to avoid adding these dependencies into main repo
include(FetchContent)

macro(fetch_git_repo URL TAG TARGET_NAME)
    FetchContent_Declare(
        ${TARGET_NAME}
        GIT_REPOSITORY ${URL}
        GIT_TAG        ${TAG}
    )

    message(STATUS "Fetching dependency ${TARGET_NAME} from ${URL}...")

    FetchContent_MakeAvailable(${TARGET_NAME})

    FetchContent_GetProperties(
        ${TARGET_NAME}
        SOURCE_DIR  ${TARGET_NAME}_SRC_DIR
        BINARY_DIR  ${TARGET_NAME}_BIN_DIR
        POPULATED   ${TARGET_NAME}_POPULATED
    )

endmacro()

########################
# glfw
########################
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)

fetch_git_repo(
    git@github.com:glfw/glfw.git
    7b6aead9fb88b3623e3b3725ebb42670cbe4c579 # version 3.4
    glfw
)

########################
# glm
########################
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
fetch_git_repo(
    git@github.com:g-truc/glm.git
    8d1fd52e5ab5590e2c81768ace50c72bae28f2ed # version 1.0.3
    glm
)

########################
# imgui
########################

fetch_git_repo(
    git@github.com:ocornut/imgui.git
    6ded5230d043aa32c755e65c910c2af5002fb9f9 # version 1.92.6
    imgui
)

add_library(imgui STATIC)
target_sources(imgui
    PRIVATE
        "${imgui_SRC_DIR}/imgui.cpp"
        "${imgui_SRC_DIR}/imgui_draw.cpp"
        "${imgui_SRC_DIR}/imgui_demo.cpp"
        "${imgui_SRC_DIR}/imgui_tables.cpp"
        "${imgui_SRC_DIR}/imgui_widgets.cpp"
        "${imgui_SRC_DIR}/backends/imgui_impl_glfw.cpp"
)
target_include_directories(imgui PUBLIC "${imgui_SRC_DIR}/imgui")
target_link_libraries(imgui PRIVATE glfw)
target_compile_features(imgui PRIVATE cxx_std_20)

########################
# alloy
########################
message(STATUS "Adding alloy library assuming project lives under alloy repository")
add_subdirectory(${PROJECT_SOURCE_DIR}/../ ${CMAKE_CURRENT_BINARY_DIR}/alloy)

########################
# MISC settings
########################
set_target_properties(imgui glfw glm PROPERTIES FOLDER "demo/3rd-party")

