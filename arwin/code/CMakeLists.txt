cmake_minimum_required(VERSION 3.20)
project(Arwin LANGUAGES CXX)

# ==================== Build Counter ====================
set(COUNTER_FILE "${CMAKE_BINARY_DIR}/build_count.txt")
if(NOT EXISTS "${COUNTER_FILE}")
    file(WRITE "${COUNTER_FILE}" "0")
endif()
file(READ "${COUNTER_FILE}" BUILD_COUNT)
math(EXPR BUILD_COUNT "${BUILD_COUNT} + 1")
file(WRITE "${COUNTER_FILE}" "${BUILD_COUNT}")
message(STATUS "=== BUILD #${BUILD_COUNT} ===")

# ==================== MSVC Compiler Flags ====================
if(MSVC)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_compile_options(
        /utf-8 
        /EHsc 
        /MD 
        -nologo 
        -fp:fast 
        -Gm- 
        -Od 
        -Oi 
        /WX /W4                    # Treat warnings as errors for your code
        /Zc:threadSafeInit-
        /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
        -wd4202 -wd4100 -wd4189 -wd4244 -wd4996 -wd4456 -wd4324
        -FC -Z7
    )

    add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

    option(ENABLE_DEBUG "Enable DEBUG define" OFF)
    if(ENABLE_DEBUG)
        add_compile_definitions(DEBUG)
    endif()

else()
    message(FATAL_ERROR "This CMakeLists.txt is written for MSVC only.")
endif()

# ==================== Include Paths ====================
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3-3.4.2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3_image-3.4.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3_ttf-3.2.2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/fastgltf-0.6.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/glm-1.0.3"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ==================== Source Files ====================
set(SOURCES
    main.cpp
    vk_engine.cpp
    vk_initializers.cpp
    vk_images.cpp
    vk_descriptors.cpp
    vk_pipelines.cpp
    vk_loader.cpp
    camera.cpp

    VkBootstrap.cpp
    simdjson.cpp

    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_impl_sdl3.cpp
    imgui_impl_vulkan.cpp

    # fastgltf
    ../external/fastgltf-0.6.0/src/fastgltf.cpp
    ../external/fastgltf-0.6.0/src/base64.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ==================== Linking ====================
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3-3.4.2/lib/x64/SDL3.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3_image-3.4.0/lib/x64/SDL3_image.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3_ttf-3.2.2/lib/x64/SDL3_ttf.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan-1.lib"
)

# Linker flags
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "/DEBUG /PDB:main.pdb /INCREMENTAL:NO /OPT:REF"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ==================== WARNING SUPPRESSION (Improved) ====================

# Third-party files get relaxed warning settings
set(THIRD_PARTY_FLAGS 
    /wd4715 /wd4267 /wd4458 /wd4127 /wd4505 /wd4244 
    /wd4100 /wd4456 /wd4324 /wd5262 /wd4702
)

set_source_files_properties(
    ../external/fastgltf-0.6.0/src/fastgltf.cpp
    ../external/fastgltf-0.6.0/src/base64.cpp
    PROPERTIES COMPILE_OPTIONS "${THIRD_PARTY_FLAGS}"
)

set_source_files_properties(
    simdjson.cpp
    PROPERTIES COMPILE_OPTIONS "${THIRD_PARTY_FLAGS}"
)

set_source_files_properties(
    imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_tables.cpp 
    imgui_widgets.cpp imgui_impl_sdl3.cpp imgui_impl_vulkan.cpp
    VkBootstrap.cpp
    PROPERTIES COMPILE_OPTIONS "${THIRD_PARTY_FLAGS}"
)

# ==================== Copy DLLs (Fixed - no pwsh issue) ====================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3-3.4.2/lib/x64/SDL3.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3_image-3.4.0/lib/x64/SDL3_image.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL3_ttf-3.2.2/lib/x64/SDL3_ttf.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying SDL3, SDL3_image and SDL3_ttf DLLs to output directory"
    VERBATIM
)

message(STATUS "Arwin CMake configured successfully (Build #${BUILD_COUNT})")