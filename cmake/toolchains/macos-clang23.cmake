# SDK libc++ (not Homebrew's) so availability annotations fail at compile time, not at launch on older macOS.
include_guard(GLOBAL)

set(_at_llvm_hints "$ENV{LLVM_ROOT}" /opt/homebrew/opt/llvm@23 /usr/local/opt/llvm@23 /opt/homebrew/opt/llvm
                   /usr/local/opt/llvm)
find_program(_at_clang NAMES clang PATHS ${_at_llvm_hints} PATH_SUFFIXES bin NO_DEFAULT_PATH)
find_program(_at_clangxx NAMES clang++ PATHS ${_at_llvm_hints} PATH_SUFFIXES bin NO_DEFAULT_PATH)
if(NOT _at_clang OR NOT _at_clangxx)
  message(FATAL_ERROR "clang 23 not found: `brew install llvm@23` or set LLVM_ROOT to an LLVM 23 install")
endif()

set(CMAKE_C_COMPILER "${_at_clang}")
set(CMAKE_CXX_COMPILER "${_at_clangxx}")
set(CMAKE_OBJC_COMPILER "${_at_clang}")
set(CMAKE_OBJCXX_COMPILER "${_at_clangxx}")

# Apple's ar/ranlib: LLVM's ranlib rewrites universal archives into a format ld64 cannot read.
foreach(_at_tool ar ranlib)
  execute_process(
    COMMAND xcrun --find ${_at_tool}
    OUTPUT_VARIABLE _at_tool_path
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  string(TOUPPER "${_at_tool}" _at_var)
  set(CMAKE_${_at_var} "${_at_tool_path}" CACHE FILEPATH "" FORCE)
endforeach()

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS version")
endif()

if(NOT CMAKE_OSX_SYSROOT)
  execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE _at_sdk
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  set(CMAKE_OSX_SYSROOT "${_at_sdk}" CACHE PATH "macOS SDK")
endif()

set(_at_libcxx "-nostdinc++ -isystem ${CMAKE_OSX_SYSROOT}/usr/include/c++/v1")
string(APPEND CMAKE_CXX_FLAGS_INIT " ${_at_libcxx}")
string(APPEND CMAKE_OBJCXX_FLAGS_INIT " ${_at_libcxx}")
