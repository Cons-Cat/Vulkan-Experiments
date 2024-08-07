cmake_minimum_required(VERSION 3.27)

# `libktx` requires C.
project(game LANGUAGES C CXX)

add_executable(game)
set_target_properties(game PROPERTIES CXX_STANDARD 26)
target_sources(game PRIVATE
  src/main.cpp
  src/camera.cpp
  src/window.cpp
  src/sync.cpp
  src/bindless.cpp
  src/vulkan_flow.cpp
  src/light.cpp
)

target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)

target_compile_options(game PRIVATE
  -std=gnu++26
  -mavx2
  -mfma
  -mbmi
  -mbmi2
  -Wall -Wextra
  -Wconversion

  -Wno-missing-field-initializers
  -Wno-unused-parameter
  -Wno-switch
  -Wno-sign-compare
  -Wno-c++26-extensions # Sometimes clang still complains about these.

  -fmerge-all-constants
  -fcolor-diagnostics
  -fno-omit-frame-pointer # Required for UBsan.

  -fsanitize=undefined

  # Nsight Graphics gets confused by asan.
  #-fsanitize=address

  -fdebug-info-for-profiling
  -fdebug-macro
  -ggdb3 -pipe
)

target_link_options(game PRIVATE
  -fsanitize=undefined
  #-fsanitize=address
)

# TODO: Support building release mode shaders as well.
# TODO: Add `BYPRODUCTS`.
set(shaders ${CMAKE_SOURCE_DIR}/src/shaders.slang)

# TODO: Use `-specialize` instead of `-D`.
add_custom_target(shaders
  # Vertex shader for camera.
  COMMAND slangc ${shaders} -o ${CMAKE_CURRENT_BINARY_DIR}/vertex_camera.spv -profile sm_6_6 -entry demo_vertex_main -O3 -Wno-39001 -emit-spirv-via-glsl -Dis_camera
  # Vertex shader for light sources.
  COMMAND slangc ${shaders} -o ${CMAKE_CURRENT_BINARY_DIR}/vertex_light.spv -profile sm_6_6 -entry demo_vertex_main -O3 -Wno-39001 -emit-spirv-via-glsl
  # Fragment shader.
  COMMAND slangc ${shaders} -o ${CMAKE_CURRENT_BINARY_DIR}/fragment.spv -entry demo_fragment_main -O3 -Wno-39001 -emit-spirv-via-glsl
  # Culling compute shader.
  COMMAND slangc ${shaders} -o ${CMAKE_CURRENT_BINARY_DIR}/culling.spv -entry culling_main -O3 -Wno-39001 -emit-spirv-via-glsl
  # Compositing triangle shader.
  COMMAND slangc ${shaders} -o ${CMAKE_CURRENT_BINARY_DIR}/composite_vertex.spv -entry composite_vertex_main -O3 -Wno-39001 -emit-spirv-via-glsl
  # Compositing fragment shader for deferred rendering.
  COMMAND slangc ${shaders} -o ${CMAKE_CURRENT_BINARY_DIR}/composite_fragment.spv -entry composite_fragment_main -O3 -Wno-39001 -emit-spirv-via-glsl
)

# Copy skybox texture next to executable.
add_custom_command(
  TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy 
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/skybox.ktx2 
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

find_package(Vulkan REQUIRED)
find_package(VulkanHeaders REQUIRED)

add_subdirectory(Vulkan-WSIWindow ${CMAKE_BINARY_DIR}/WSIWindow)
add_subdirectory(vk-bootstrap)
add_subdirectory(Vookoo)
add_subdirectory(KTX-Software)
add_subdirectory(glm)

target_link_libraries(game PRIVATE
   Vulkan::Vulkan
   Vulkan::Headers
   vk-bootstrap::vk-bootstrap
   ${CMAKE_DL_LIBS}
   WSIWindow
   vookoo_interface
   glm::glm
   ktx
)

message(STATUS "Vulkan Headers Version: ${VulkanHeaders_VERSION}")

target_compile_definitions(game PRIVATE
  VULKAN_HPP_TYPESAFE_CONVERSION
  VULKAN_HPP_HAS_SPACESHIP_OPERATOR
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  GLM_ENABLE_EXPERIMENTAL
  GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# WSI Layers.
if(WIN32)
  target_compile_definitions(game PRIVATE
    VK_USE_PLATFORM_WIN32_KHR
  )
else()
  target_compile_definitions(game PRIVATE
    VK_USE_PLATFORM_XCB_KHR
  )
endif()
