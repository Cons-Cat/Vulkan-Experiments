cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

add_executable(game)
set_property(TARGET game PROPERTY CXX_STANDARD 26)
target_sources(game PRIVATE
  main.cpp
  camera.cpp
  window.cpp
  bindless.cpp
  vulkan_flow.cpp
)

target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(game PRIVATE
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

  -ggdb3 -pipe
)

target_link_options(game PRIVATE
  -fsanitize=undefined
  #-fsanitize=address
)

option(DebugVertices OFF)
if (DebugVertices)
  target_compile_definitions(game PRIVATE "DEBUG_VERTICES")
endif()

# TODO: Use `-emit-spirv-directly`.
# TODO: Support building release mode shaders as well.
add_custom_target(shaders ALL
  COMMAND slangc ${CMAKE_SOURCE_DIR}/shaders.slang -o ${CMAKE_CURRENT_BINARY_DIR}/vertex.spv -profile sm_6_6 -entry demo_vertex_main -g -O2 -Wno-39001
  COMMAND slangc ${CMAKE_SOURCE_DIR}/shaders.slang -o ${CMAKE_CURRENT_BINARY_DIR}/fragment.spv -entry demo_fragment_main -g -O2 -Wno-39001
  COMMAND slangc ${CMAKE_SOURCE_DIR}/shaders.slang -o ${CMAKE_CURRENT_BINARY_DIR}/composite_vertex.spv -entry composite_vertex_main -g -O2 -Wno-39001
  COMMAND slangc ${CMAKE_SOURCE_DIR}/shaders.slang -o ${CMAKE_CURRENT_BINARY_DIR}/composite_fragment.spv -entry composite_fragment_main -g -O2 -Wno-39001
)

find_package(Vulkan REQUIRED)
find_package(VulkanHeaders REQUIRED)

add_subdirectory(Vulkan-WSIWindow ${CMAKE_BINARY_DIR}/WSIWindow)
add_subdirectory(vk-bootstrap)
add_subdirectory(Vookoo)
add_subdirectory(glm)

target_link_libraries(game PRIVATE
   Vulkan::Vulkan
   Vulkan::Headers
   vk-bootstrap::vk-bootstrap
   ${CMAKE_DL_LIBS}
   WSIWindow
   vookoo_interface
   glm::glm
)

message(STATUS "Vulkan Headers Version: ${VulkanHeaders_VERSION}")

target_compile_definitions(game PRIVATE
  VULKAN_HPP_TYPESAFE_CONVERSION
  VULKAN_HPP_HAS_SPACESHIP_OPERATOR
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  GLM_ENABLE_EXPERIMENTAL
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
