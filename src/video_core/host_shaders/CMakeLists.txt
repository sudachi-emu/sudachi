# SPDX-FileCopyrightText: 2018 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

set(FIDELITYFX_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/externals/FidelityFX-FSR/ffx-fsr)

set(FIDELITYFX_FILES
    ${FIDELITYFX_INCLUDE_DIR}/ffx_a.h
    ${FIDELITYFX_INCLUDE_DIR}/ffx_fsr1.h
)

set(GLSL_INCLUDES
    fidelityfx_fsr.frag
    ${FIDELITYFX_FILES}
)

set(SHADER_FILES
    astc_decoder.comp
    blit_color_float.frag
    block_linear_unswizzle_2d.comp
    block_linear_unswizzle_3d.comp
    convert_abgr8_srgb_to_d24s8.frag
    convert_abgr8_to_d24s8.frag
    convert_abgr8_to_d32f.frag
    convert_d32f_to_abgr8.frag
    convert_d24s8_to_abgr8.frag
    convert_depth_to_float.frag
    convert_float_to_depth.frag
    convert_msaa_to_non_msaa.comp
    convert_non_msaa_to_msaa.comp
    convert_s8d24_to_abgr8.frag
    full_screen_triangle.vert
    fxaa.frag
    fxaa.vert
    opengl_convert_s8d24.comp
    opengl_copy_bc4.comp
    opengl_fidelityfx_fsr.frag
    opengl_fidelityfx_fsr_easu.frag
    opengl_fidelityfx_fsr_rcas.frag
    opengl_lmem_warmup.comp
    opengl_present.frag
    opengl_present.vert
    opengl_present_scaleforce.frag
    opengl_smaa.glsl
    pitch_unswizzle.comp
    present_bicubic.frag
    present_gaussian.frag
    queries_prefix_scan_sum.comp
    queries_prefix_scan_sum_nosubgroups.comp
    resolve_conditional_render.comp
    smaa_edge_detection.vert
    smaa_edge_detection.frag
    smaa_blending_weight_calculation.vert
    smaa_blending_weight_calculation.frag
    smaa_neighborhood_blending.vert
    smaa_neighborhood_blending.frag
    vulkan_blit_depth_stencil.frag
    vulkan_color_clear.frag
    vulkan_color_clear.vert
    vulkan_depthstencil_clear.frag
    vulkan_fidelityfx_fsr.vert
    vulkan_fidelityfx_fsr_easu_fp16.frag
    vulkan_fidelityfx_fsr_easu_fp32.frag
    vulkan_fidelityfx_fsr_rcas_fp16.frag
    vulkan_fidelityfx_fsr_rcas_fp32.frag
    vulkan_present.frag
    vulkan_present.vert
    vulkan_present_scaleforce_fp16.frag
    vulkan_present_scaleforce_fp32.frag
    vulkan_quad_indexed.comp
    vulkan_turbo_mode.comp
    vulkan_uint8.comp
)

find_program(GLSLANGVALIDATOR "glslangValidator")
if ("${GLSLANGVALIDATOR}" STREQUAL "GLSLANGVALIDATOR-NOTFOUND")
    message(FATAL_ERROR "Required program `glslangValidator` not found.")
endif()

set(GLSL_FLAGS "")
set(SPIR_V_VERSION "spirv1.3")
set(QUIET_FLAG "--quiet")

set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
set(HOST_SHADERS_INCLUDE ${SHADER_INCLUDE} PARENT_SCOPE)

set(INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/source_shader.h.in)
set(HEADER_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/StringShaderHeader.cmake)

# Check if `--quiet` is available on host's glslangValidator version
# glslangValidator prints to STDERR iff an unrecognized flag is passed to it
execute_process(
    COMMAND
        ${GLSLANGVALIDATOR} ${QUIET_FLAG}
    ERROR_VARIABLE
        GLSLANG_ERROR
    # STDOUT variable defined to silence unnecessary output during CMake configuration
    OUTPUT_VARIABLE
        GLSLANG_OUTPUT
)

if (NOT GLSLANG_ERROR STREQUAL "")
    message(WARNING "Refusing to use unavailable flag `${QUIET_FLAG}` on `${GLSLANGVALIDATOR}`")
    set(QUIET_FLAG "")
endif()

foreach(FILENAME IN ITEMS ${SHADER_FILES})
    string(REPLACE "." "_" SHADER_NAME ${FILENAME})
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME})
    # Skip generating source headers on Vulkan exclusive files
    if (NOT ${FILENAME} MATCHES "vulkan.*")
        set(SOURCE_HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}.h)
        add_custom_command(
            OUTPUT
                ${SOURCE_HEADER_FILE}
            COMMAND
                ${CMAKE_COMMAND} -P ${HEADER_GENERATOR} ${SOURCE_FILE} ${SOURCE_HEADER_FILE} ${INPUT_FILE}
            MAIN_DEPENDENCY
                ${SOURCE_FILE}
            DEPENDS
                ${INPUT_FILE}
                # HEADER_GENERATOR should be included here but msbuild seems to assume it's always modified
        )
        set(SHADER_HEADERS ${SHADER_HEADERS} ${SOURCE_HEADER_FILE})
    endif()
    # Skip compiling to SPIR-V OpenGL exclusive files
    if (NOT ${FILENAME} MATCHES "opengl.*")
        string(TOUPPER ${SHADER_NAME}_SPV SPIRV_VARIABLE_NAME)
        set(SPIRV_HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}_spv.h)
        add_custom_command(
            OUTPUT
                ${SPIRV_HEADER_FILE}
            COMMAND
                ${GLSLANGVALIDATOR} -V ${QUIET_FLAG} -I"${FIDELITYFX_INCLUDE_DIR}" ${GLSL_FLAGS} --variable-name ${SPIRV_VARIABLE_NAME} -o ${SPIRV_HEADER_FILE} ${SOURCE_FILE} --target-env ${SPIR_V_VERSION}
            MAIN_DEPENDENCY
                ${SOURCE_FILE}
        )
        set(SHADER_HEADERS ${SHADER_HEADERS} ${SPIRV_HEADER_FILE})
    endif()
endforeach()

foreach(FILEPATH IN ITEMS ${FIDELITYFX_FILES})
    get_filename_component(FILENAME ${FILEPATH} NAME)
    string(REPLACE "." "_" HEADER_NAME ${FILENAME})
    set(SOURCE_FILE ${FILEPATH})
    set(SOURCE_HEADER_FILE ${SHADER_DIR}/${HEADER_NAME}.h)
    add_custom_command(
        OUTPUT
            ${SOURCE_HEADER_FILE}
        COMMAND
            ${CMAKE_COMMAND} -P ${HEADER_GENERATOR} ${SOURCE_FILE} ${SOURCE_HEADER_FILE} ${INPUT_FILE}
        MAIN_DEPENDENCY
            ${SOURCE_FILE}
        DEPENDS
            ${INPUT_FILE}
            # HEADER_GENERATOR should be included here but msbuild seems to assume it's always modified
    )
    set(SHADER_HEADERS ${SHADER_HEADERS} ${SOURCE_HEADER_FILE})
endforeach()

set(SHADER_SOURCES ${SHADER_FILES})
list(APPEND SHADER_SOURCES ${GLSL_INCLUDES})

add_custom_target(host_shaders
    DEPENDS
        ${SHADER_HEADERS}
    SOURCES
        ${SHADER_SOURCES}
)
