cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(macosu_pad VERSION 0.3.1 LANGUAGES C CXX OBJCXX)

if(NOT APPLE)
  message(FATAL_ERROR "macOSu-pad only supports macOS")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 23)
  message(FATAL_ERROR
    "Clang 23 is required (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}); configure through a preset "
    "(see CMakePresets.json), which loads cmake/toolchains/macos-clang23.cmake")
endif()

set(CMAKE_CXX_STANDARD          26)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD          26)
set(CMAKE_OBJCXX_EXTENSIONS        OFF)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

add_compile_options(
  -Wall
  -Wextra
  # Assets are baked in with #embed (C23, a Clang extension in C++).
  -Wno-c23-extensions
  # Designated initializers leave the remaining fields at their defaults on purpose.
  -Wno-missing-designated-field-initializers
  "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>"
)

include(CTest)

add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
