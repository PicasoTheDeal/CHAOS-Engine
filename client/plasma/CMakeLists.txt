# the kde plasma wallpaper plugin: a qml module with one item, ChaosView, that renders the engine inside plasmashell.
# built only with -DCHAOS_PLASMA=ON. needs qt6-base + qt6-declarative (arch: qt6-base qt6-declarative).
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick OpenGL Concurrent)
qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)

qt_add_qml_module(chaosqml
  URI org.chaos.wallpaper
  VERSION 1.0
  PLUGIN_TARGET chaosqmlplugin
  NO_PLUGIN_OPTIONAL
  OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/chaos/wallpaper
  SOURCES chaositem.h chaositem.cpp)
target_link_libraries(chaosqml PRIVATE chaos_render Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::Concurrent)
target_link_libraries(chaosqmlplugin PRIVATE chaos_render)
target_compile_options(chaosqml PRIVATE -Wall -Wextra)
# refuse to link a plugin with a hole in it: an undefined symbol only shows up when plasmashell fails to load the
# module, and that failure is a silent black desktop (2026-11-06: pickWallpaperSeed lived in the app, not a lib)
# a missing symbol in a shared lib links fine and then plasmashell silently refuses to dlopen it. black desktop, no message. never again
target_link_options(chaosqml PRIVATE -Wl,--no-undefined)
target_link_options(chaosqmlplugin PRIVATE -Wl,--no-undefined)

# where qt looks for qml modules on this system
if(NOT DEFINED QT6_INSTALL_QML)
  get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
  execute_process(COMMAND ${_qmake} -query QT_INSTALL_QML OUTPUT_VARIABLE QT6_INSTALL_QML OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
install(TARGETS chaosqml chaosqmlplugin DESTINATION ${QT6_INSTALL_QML}/org/chaos/wallpaper)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org/chaos/wallpaper/qmldir DESTINATION ${QT6_INSTALL_QML}/org/chaos/wallpaper)
# the wallpaper package plasma lists in its wallpaper settings
install(DIRECTORY package/ DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/wallpapers/org.chaos.wallpaper)
