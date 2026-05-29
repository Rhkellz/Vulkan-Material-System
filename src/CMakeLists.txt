
# Add source to this project's executable.
add_executable(vulkan_guide
    main.cpp
    vk_engine.cpp
    vk_engine.h
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_images.h
    vk_images.cpp
    vk_descriptors.h
    vk_descriptors.cpp
    vk_pipelines.h
    vk_pipelines.cpp
    vk_loader.h
    vk_loader.cpp
)


set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide vkbootstrap vma glm tinyobjloader imgui stb_image fmt::fmt fastgltf)

target_link_libraries(vulkan_guide Vulkan::Vulkan SDL2::SDL2)
target_compile_definitions(vulkan_guide PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

add_dependencies(vulkan_guide Shaders)
