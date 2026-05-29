cmake_minimum_required(VERSION 3.8)

project("material_system")

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_subdirectory(third_party)
add_subdirectory(src)

# =========================================================
# OUTPUT DIR (FOR EXE + SHADERS)
# =========================================================
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${OUTPUT_DIR})

# =========================================================
# GLSL COMPILER
# =========================================================
find_program(GLSL_VALIDATOR glslangValidator
    HINTS
    $ENV{VULKAN_SDK}/Bin/
    $ENV{VULKAN_SDK}/Bin32/
)

# =========================================================
# SHADER OUTPUT DIR (NEXT TO EXE)
# =========================================================
set(SHADER_DIR "${OUTPUT_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_DIR})

# =========================================================
# INPUT SHADERS
# =========================================================
file(GLOB GLSL_FILES
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/shaders/*.comp"
)

# =========================================================
# BUILD SHADERS
# =========================================================
set(SPIRV_OUTPUTS)

foreach(GLSL ${GLSL_FILES})

    get_filename_component(FILE_NAME ${GLSL} NAME)

    set(SPIRV_FILE "${SHADER_DIR}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${GLSL_VALIDATOR}
        -V --target-env vulkan1.3
        ${GLSL}
        -o ${SPIRV_FILE}
        DEPENDS ${GLSL}
        COMMENT "Compiling shader ${FILE_NAME}"
    )

    list(APPEND SPIRV_OUTPUTS ${SPIRV_FILE})

endforeach()

add_custom_target(Shaders ALL
    DEPENDS ${SPIRV_OUTPUTS}
)