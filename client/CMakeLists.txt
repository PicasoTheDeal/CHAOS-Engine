cmake_minimum_required(VERSION 3.20)
project(chaos_engine VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CHAOS_REGISTRY_URL "https://chaos-eng.tetstack.dev" CACHE STRING "default registry url baked into the binary (CHAOS_REGISTRY env and settings.json override it)")

find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11 xext)
find_package(CURL REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client wayland-egl egl)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
find_package(nlohmann_json 3 REQUIRED)

# ---- vendored ------------------------------------------------------------
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

add_library(blake3 STATIC
  third_party/blake3/blake3.c
  third_party/blake3/blake3_dispatch.c
  third_party/blake3/blake3_portable.c)
target_include_directories(blake3 PUBLIC third_party/blake3)
target_compile_definitions(blake3 PRIVATE BLAKE3_NO_SSE2 BLAKE3_NO_SSE41 BLAKE3_NO_AVX2 BLAKE3_NO_AVX512)

add_library(imgui STATIC
  third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
  third_party/imgui/backends/imgui_impl_glfw.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui SYSTEM PUBLIC third_party/imgui third_party/imgui/backends)
target_link_libraries(imgui PUBLIC glfw)

# ---- wayland protocol glue, generated at build time from the vendored xml ----
set(WLP ${CMAKE_SOURCE_DIR}/third_party/wayland-protocols)
set(WLGEN ${CMAKE_BINARY_DIR}/wayland-gen)
file(MAKE_DIRECTORY ${WLGEN})
foreach(proto wlr-layer-shell-unstable-v1 xdg-shell)
  add_custom_command(OUTPUT ${WLGEN}/${proto}-client-protocol.h ${WLGEN}/${proto}-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header ${WLP}/${proto}.xml ${WLGEN}/${proto}-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WLP}/${proto}.xml ${WLGEN}/${proto}-protocol.c
    DEPENDS ${WLP}/${proto}.xml COMMENT "wayland-scanner ${proto}")
  list(APPEND WL_SOURCES ${WLGEN}/${proto}-protocol.c)
endforeach()
add_library(wlprotocols STATIC ${WL_SOURCES})
target_include_directories(wlprotocols PUBLIC ${WLGEN})
target_link_libraries(wlprotocols PUBLIC PkgConfig::WAYLAND)

# ---- engine core (everything except main, so tests can link it) ---------
add_library(chaos_core STATIC
  src/seed/rng.cpp
  src/seed/seed.cpp
  src/seed/base58.cpp
  src/seed/bip39.cpp
  src/seed/rarity.cpp
  src/util/png.cpp
  src/vault/vault.cpp
  src/chaos_file/format.cpp
  src/settings.cpp
  src/vault/activation.cpp
  src/wallpaper_seed.cpp
  src/model/cheng.cpp
  src/platform/power.cpp
  src/sync/api.cpp
  src/sync/session_flow.cpp
  src/shape/parametric.cpp
  src/shape/superellipsoid.cpp
  src/shape/primitives.cpp
  src/shape/generators.cpp
  src/shape/mutate.cpp
  src/scene/palette.cpp
  src/scene/dimension.cpp
  src/scene/mode.cpp)
target_include_directories(chaos_core PUBLIC src ${CMAKE_SOURCE_DIR}/../shared)
target_link_libraries(chaos_core PUBLIC blake3 glm::glm OpenSSL::Crypto PkgConfig::SODIUM nlohmann_json::nlohmann_json CURL::libcurl)
target_compile_definitions(chaos_core PUBLIC CHAOS_REGISTRY_URL="${CHAOS_REGISTRY_URL}" CHAOS_VERSION_STRING="${PROJECT_VERSION}")
target_include_directories(chaos_core SYSTEM PUBLIC third_party/stb)
target_compile_definitions(chaos_core PUBLIC GLM_ENABLE_EXPERIMENTAL)
target_compile_options(chaos_core PRIVATE -Wall -Wextra -Wshadow -Wmisleading-indentation)

# ---- gl renderer: the app and the plasma plugin both draw with this ---------
add_library(chaos_render STATIC
  src/render/shader.cpp
  src/render/gl_mesh.cpp
  src/render/post.cpp
  src/render/renderer.cpp
  src/render/gl_boot.cpp
  src/util/screenshot.cpp)
target_link_libraries(chaos_render PUBLIC chaos_core glad)
target_compile_options(chaos_render PRIVATE -Wall -Wextra -Wshadow -Wmisleading-indentation)
target_compile_definitions(chaos_render PUBLIC CHAOS_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders" CHAOS_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/chaos")
set_target_properties(chaos_core chaos_render glad blake3 wlprotocols PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- app ------------------------------------------------------------------
add_executable(chaos
  src/main.cpp
  src/cli.cpp
  src/platform/window.cpp
  src/platform/x11_wallpaper.cpp
  src/platform/wl_layer.cpp
  src/wallpaper.cpp
  src/ui/panel.cpp
  src/ui/thumbs.cpp)
target_link_libraries(chaos PRIVATE chaos_render glfw OpenGL::GL imgui PkgConfig::X11 nlohmann_json::nlohmann_json wlprotocols PkgConfig::WAYLAND)
target_include_directories(chaos SYSTEM PRIVATE third_party/stb)
target_include_directories(chaos_render SYSTEM PRIVATE third_party/stb)
target_compile_options(chaos PRIVATE -Wall -Wextra -Wshadow -Wmisleading-indentation)
target_compile_definitions(chaos PRIVATE CHAOS_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders" CHAOS_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# ---- plasma wallpaper plugin (kde). needs qt6: -DCHAOS_PLASMA=ON ---------------
option(CHAOS_PLASMA "build the kde plasma wallpaper plugin (needs Qt6 Quick)" OFF)
if(CHAOS_PLASMA)
  add_subdirectory(plasma)
endif()

# ---- dev registry: the server's half, in c++, until the real one exists ----
add_executable(chaos-registry-dev tools/registry_dev.cpp)
target_link_libraries(chaos-registry-dev PRIVATE chaos_core)
target_compile_options(chaos-registry-dev PRIVATE -Wall -Wextra -Wshadow -Wmisleading-indentation)

add_executable(chaos-vectors tools/vectors.cpp)
target_link_libraries(chaos-vectors PRIVATE chaos_core)

# ---- tests ----------------------------------------------------------------
find_package(Catch2 3 QUIET)
if(Catch2_FOUND)
  add_executable(chaos_tests tests/test_format.cpp tests/test_rng.cpp tests/test_shape.cpp tests/test_mode.cpp tests/test_seed_forms.cpp tests/test_vault.cpp tests/test_chaos_file.cpp tests/test_settings.cpp tests/test_activation.cpp tests/test_wallpaper_seed.cpp tests/test_cheng.cpp)
  target_link_libraries(chaos_tests PRIVATE chaos_core Catch2::Catch2WithMain)
  include(CTest)
  include(Catch)
  catch_discover_tests(chaos_tests)
endif()

# ---- install + packaging --------------------------------------------------
include(GNUInstallDirs)
install(TARGETS chaos chaos-registry-dev RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY shaders assets DESTINATION ${CMAKE_INSTALL_DATADIR}/chaos)
install(FILES ../packaging/chaos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ../packaging/chaos.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ../docs/chaos-file-format.md ../docs/ownership-model.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(CPACK_PACKAGE_NAME "chaos-engine")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "PicasoTheDeal")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CHAOS: a seeded 3D dimension as your linux wallpaper")
set(CPACK_GENERATOR "DEB;TGZ")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libglfw3 (>= 3.3), libsodium23, libssl3t64 | libssl3, libcurl4t64 | libcurl4, libx11-6, libxext6, libgl1")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "custom")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_REQUIRES "glfw, libsodium, openssl-libs, libcurl, libX11, libXext, mesa-libGL")
set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_STRIP_FILES ON)
include(CPack)
