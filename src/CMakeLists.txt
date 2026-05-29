
# Add source to this project's executable.
add_executable(material_system
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


set_property(TARGET material_system PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:material_system>")

target_include_directories(material_system PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(material_system vkbootstrap vma glm tinyobjloader imgui stb_image fmt::fmt fastgltf)

target_link_libraries(material_system Vulkan::Vulkan SDL2::SDL2)
target_compile_definitions(material_system PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

add_dependencies(material_system Shaders)
