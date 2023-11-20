cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

add_executable(game)
set_property(TARGET game PROPERTY CXX_STANDARD 26)
target_sources(game PRIVATE
  main.cpp render_pass.cpp)
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(game PRIVATE
  -mavx2
  -mfma
  -mbmi
  -mbmi2
  -Wall -Wextra
  #-Wconversion

  -Wno-missing-field-initializers
  -Wno-unused-parameter
  -Wno-switch
  -Wno-sign-compare
  -Wno-c++26-extensions # Sometimes clang still complains about these.

  -fmerge-all-constants
  -fcolor-diagnostics
  -fno-omit-frame-pointer # Required for UBsan.

  -fsanitize=undefined
  #-fsanitize=address

  -ggdb3 -pipe
)

target_link_options(game PRIVATE
  -fsanitize=undefined
  #-fsanitize=address
)

find_package(Vulkan REQUIRED)

add_subdirectory(Vulkan-WSIWindow ${CMAKE_BINARY_DIR}/WSIWindow)
add_subdirectory(vk-bootstrap)
add_subdirectory(Vookoo)

target_link_libraries(game PRIVATE
   ubsan
   #asan
   Vulkan::Vulkan
   vk-bootstrap::vk-bootstrap
   ${CMAKE_DL_LIBS}
   WSIWindow
   vookoo_interface
)

target_compile_definitions(game PRIVATE
  VULKAN_HPP_TYPESAFE_CONVERSION
  VULKAN_HPP_HAS_SPACESHIP_OPERATOR
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
