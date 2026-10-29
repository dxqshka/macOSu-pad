find_package(glm CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)

add_library(ui STATIC
  src/style.cc
  src/widget.cc
  src/router.cc
  src/overlay.cc
  src/root.cc
)

target_include_directories(ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(ui PUBLIC
  glm::glm-header-only
  yoga::yogacore
)

add_library(ui_raylib STATIC
  src/raylib_painter.cc
  src/raylib_input.cc
)

target_link_libraries(ui_raylib PUBLIC
  ui
  raylib
)
