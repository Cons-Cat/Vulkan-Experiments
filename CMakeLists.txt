cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

add_executable(game)
set_property(TARGET game PROPERTY CXX_STANDARD 23)
target_sources(game PRIVATE
  main.cpp)
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(game PRIVATE
  -mavx2
  -mfma
  -mbmi
  -mbmi2
  -Wall -Wextra

  -Wno-missing-field-initializers
  -Wno-unused-parameter

  -fno-omit-frame-pointer # Required for UBsan.
  -fsanitize=undefined
#  -fsanitize=hwaddress

  -ggdb3 -pipe
)

add_subdirectory(filament/)
#target_link_libraries(game PRIVATE TNT)

# target_link_libraries(
#     game PRIVATE
#     #hwasan
#     ubsan
#   )
