find_package(toml11 CONFIG REQUIRED)

add_library(trackpad_core STATIC
  src/config.cc
  src/cursor_hider.cc
  src/engine.cc
  src/multitouch.cc
  src/output.cc
  src/overlay_shape.cc
  src/tracker.cc
  src/settings/area_geometry.cc
)

target_include_directories(trackpad_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(trackpad_core
  PUBLIC
    "-framework ApplicationServices"
    "-framework CoreFoundation"
  PRIVATE
    toml11::toml11
)

add_executable(macOSu-pad
  src/app.cc
  src/main.cc
  src/mac/shell.mm
  src/settings/pads.cc
  src/settings/settings_window.cc
  src/settings/theme.cc
  src/settings/widgets.cc
)

target_compile_definitions(macOSu-pad PRIVATE MACOSU_PAD_VERSION="${PROJECT_VERSION}")

target_link_libraries(macOSu-pad PRIVATE
  trackpad_core
  ui_raylib
  "-framework AppKit"
)

# Signed ad hoc here; scripts/build re-signs with a stable identity so Accessibility survives rebuilds.
set(MACOSU_PAD_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/macOSu-pad.app")
configure_file(Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)

add_custom_command(
  OUTPUT "${MACOSU_PAD_BUNDLE}/Contents/MacOS/macOSu-pad"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${MACOSU_PAD_BUNDLE}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MACOSU_PAD_BUNDLE}/Contents/MacOS"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" "${MACOSU_PAD_BUNDLE}/Contents/Info.plist"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:macOSu-pad>" "${MACOSU_PAD_BUNDLE}/Contents/MacOS/macOSu-pad"
  COMMAND codesign --force --sign - "${MACOSU_PAD_BUNDLE}"
  DEPENDS macOSu-pad "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
  COMMENT "Bundling macOSu-pad.app"
  VERBATIM
)
add_custom_target(bundle ALL DEPENDS "${MACOSU_PAD_BUNDLE}/Contents/MacOS/macOSu-pad")
