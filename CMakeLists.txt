cmake_minimum_required(VERSION 3.22)
project(Spun VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(CTest)
include(CheckIPOSupported)
include(CheckCXXSourceCompiles)
include(CheckLinkerFlag)
include(CheckCXXCompilerFlag)
set(SPUN_SIZE_OPTIMIZATION -Os)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    check_cxx_compiler_flag(-Oz SPUN_OZ_SUPPORTED)
    if(SPUN_OZ_SUPPORTED)
        set(SPUN_SIZE_OPTIMIZATION -Oz)
    endif()
endif()
check_ipo_supported(RESULT SPUN_IPO_SUPPORTED OUTPUT SPUN_IPO_ERROR)
if(SPUN_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Packed relative relocations require a compatible loader. Older target
    # libc/toolchains keep the normal ELF tables when building from source.
    check_cxx_source_compiles("#include <features.h>
        #if !defined(__GLIBC_PREREQ)
        #error Unsupported loader
        #elif !__GLIBC_PREREQ(2,36)
        #error Older loader
        #endif
        int main() { return 0; }" SPUN_RELR_RUNTIME)
    if(SPUN_RELR_RUNTIME)
        check_linker_flag(CXX "LINKER:-z,pack-relative-relocs" SPUN_RELR_SUPPORTED)
    endif()
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 Multimedia Concurrent DBus Network Svg)
option(SPUN_ENABLE_3D "Build optional Qt Quick 3D player view" ON)
if(SPUN_ENABLE_3D)
    find_package(Qt6 6.8 QUIET COMPONENTS Quick3D)
endif()
qt_policy(SET QTP0004 NEW)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib>=2.0)
qt_add_library(spun_core STATIC
    src/symbol.cpp src/symbol.h src/typography.cpp src/typography.h
    src/listening.cpp src/listening.h src/tapesound.cpp src/tapesound.h src/vinylnoise.cpp src/vinylnoise.h
    src/musicactions.cpp src/musicactions.h
    src/library.cpp src/library.h src/lyrics.cpp src/lyrics.h src/artwork.cpp src/artwork.h
    src/tx6.cpp src/tx6.h src/youtube.cpp src/youtube.h src/jellyfinapi.cpp src/jellyfinapi.h src/remotemusicapi.h src/remotelibrary.cpp src/remotelibrary.h src/jellyfin.h src/subsonicapi.cpp src/subsonicapi.h
    src/player.cpp src/player.h src/disc.cpp src/disc.h src/recorder.cpp src/recorder.h src/playerbody.cpp src/playerbody.h src/mpris.cpp src/mpris.h
    src/theme.cpp src/theme.h src/cider.cpp src/cider.h src/ciderevents.cpp src/ciderevents.h)
target_link_libraries(spun_core PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::QuickControls2
    Qt6::Svg Qt6::Multimedia Qt6::Concurrent Qt6::DBus Qt6::Network PkgConfig::TAGLIB)
if(TARGET Qt6::Quick3D AND SPUN_ENABLE_3D)
    target_sources(spun_core PRIVATE src/deckgeometry.cpp src/deckgeometry.h src/cablegeometry.cpp src/cablegeometry.h src/mediageometry.cpp src/mediageometry.h)
    target_link_libraries(spun_core PUBLIC Qt6::Quick3D)
    target_compile_definitions(spun_core PUBLIC SPUN_WITH_3D=1)
    set(SPUN_3D_QML qml/Player3D.qml qml/Recorder3D.qml qml/Tx6_3D.qml qml/Tx6Cable3D.qml qml/Vinyl3D.qml qml/Cassette3D.qml qml/Cd3D.qml)
endif()
set(SPUN_DEMO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/First-Light.flac" CACHE FILEPATH "Installed or source demo audio file")
target_compile_definitions(spun_core PUBLIC SPUN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" SPUN_DEMO_FILE="${SPUN_DEMO_FILE}")
target_compile_options(spun_core PRIVATE -Wall -Wextra -Wpedantic)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Keep Release code compact without changing floating-point semantics or
    # disabling exceptions. The animated geometry retains full speed optimization.
    target_compile_options(spun_core PRIVATE "$<$<CONFIG:Release>:${SPUN_SIZE_OPTIMIZATION};-ffunction-sections;-fdata-sections>")
    set_source_files_properties(src/disc.cpp src/mediageometry.cpp src/deckgeometry.cpp src/cablegeometry.cpp PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-O3>")
endif()
file(GLOB SPUN_SYMBOLS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg")
set_source_files_properties(qml/SpunStyle.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# The font picker is prepared asynchronously on opening Preferences. Its source
# remains bundled; there is no need to carry another copy as startup bytecode.
set_source_files_properties(qml/FontPicker.qml PROPERTIES QT_QML_SKIP_CACHEGEN TRUE)
if(APPLE)
    # Finder needs an .icns; build one from the shipped PNG with the system tools.
    set(SPUN_MACOS_ICON "${CMAKE_CURRENT_BINARY_DIR}/spun.icns")
    set(SPUN_ICONSET "${CMAKE_CURRENT_BINARY_DIR}/spun.iconset")
    add_custom_command(OUTPUT "${SPUN_MACOS_ICON}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${SPUN_ICONSET}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPUN_ICONSET}"
        COMMAND sips -z 16 16     "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_16x16.png" > /dev/null
        COMMAND sips -z 32 32     "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_16x16@2x.png" > /dev/null
        COMMAND sips -z 32 32     "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_32x32.png" > /dev/null
        COMMAND sips -z 64 64     "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_32x32@2x.png" > /dev/null
        COMMAND sips -z 128 128   "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_128x128.png" > /dev/null
        COMMAND sips -z 256 256   "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_128x128@2x.png" > /dev/null
        COMMAND sips -z 256 256   "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_256x256.png" > /dev/null
        COMMAND sips -z 512 512   "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_256x256@2x.png" > /dev/null
        COMMAND sips -z 512 512   "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_512x512.png" > /dev/null
        COMMAND sips -z 1024 1024 "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png" --out "${SPUN_ICONSET}/icon_512x512@2x.png" > /dev/null
        COMMAND iconutil -c icns "${SPUN_ICONSET}" -o "${SPUN_MACOS_ICON}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/spun-icon.png"
        COMMENT "Generating spun.icns" VERBATIM)
endif()
function(spun_application target)
    qt_add_executable(${target} src/main.cpp)
    if(APPLE)
        # Ship a real .app so Finder, the Dock and Launchpad show Spun's icon.
        # A bundled binary can be activated by the window server, which the
        # keyboard-focus checks in the diagnostics build depend on.
        set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Spun"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.spun.${target}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
        if(target STREQUAL "spun")
            target_sources(${target} PRIVATE "${SPUN_MACOS_ICON}")
            set_source_files_properties("${SPUN_MACOS_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
            set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE "spun.icns")
        endif()
    endif()
    target_link_libraries(${target} PRIVATE spun_core)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(${target} PROPERTIES QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode")
    # Compile QML at build time, preserving existing resource URLs and the UI.
    qt_add_qml_module(${target} URI SpunUi VERSION 1.0 NO_PLUGIN
        NO_RESOURCE_TARGET_PATH RESOURCE_PREFIX "/" OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${target}-qml"
        QML_FILES qml/RemoteLibraryPanel.qml qml/SpunChoiceButton.qml qml/SpunLoading.qml qml/YoutubePanel.qml ${SPUN_3D_QML} qml/Recorder.qml qml/RecorderDisplay.qml qml/Tx6Controls.qml qml/Tx6Labels.qml qml/Tx6Display.qml qml/CassetteControls.qml qml/CdControls.qml qml/CdDisplay.qml qml/SpunButton.qml qml/SpunSlider.qml qml/SeekSlider.qml qml/QuickJump.qml qml/NextTrackTip.qml qml/SavedQueuePicker.qml qml/Tonearm.qml qml/CassetteReels.qml qml/SpunText.qml qml/FontPicker.qml qml/SpunStateLayer.qml qml/SpunSpring.qml qml/SpunPopupEnter.qml qml/SpunStyle.qml qml/TrackContent.qml qml/SpunSearchField.qml qml/MenuEntry.qml qml/PreferenceSwitch.qml qml/TrackMenu.qml qml/Main.qml qml/LibraryPanel.qml qml/SpunToolTip.qml qml/IconButton.qml qml/Glyph.qml)
    # Preserve the exact SVGs and URLs; Qt's default threshold leaves many of
    # these small resources uncompressed. Parsed symbols are already cached.
    if(QT_FEATURE_zstd)
        set(asset_compression --compress-algo zstd --compress 19 --threshold 0)
    else()
        set(asset_compression --compress-algo zlib --compress 9 --threshold 0)
    endif()
    qt_add_resources(${target} "${target}_assets" PREFIX "/"
        FILES ${SPUN_SYMBOLS} assets/spun-window.png licenses/MaterialSymbols-LICENSE.txt
        OPTIONS ${asset_compression})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:${SPUN_SIZE_OPTIMIZATION};-ffunction-sections;-fdata-sections>")
        if(APPLE)
            target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:-Wl,-dead_strip>")
        else()
            target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:-Wl,--gc-sections>")
        endif()
        if(SPUN_RELR_RUNTIME AND SPUN_RELR_SUPPORTED)
            target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:LINKER:-z,pack-relative-relocs>")
        endif()
    endif()
endfunction()
spun_application(spun)
if(CMAKE_STRIP AND CMAKE_BUILD_TYPE STREQUAL "Release")
    # Debug/RelWithDebInfo builds retain symbols; strip only the shipped Release app.
    if(APPLE)
        add_custom_command(TARGET spun POST_BUILD COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:spun>)
    else()
        add_custom_command(TARGET spun POST_BUILD COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:spun>)
    endif()
endif()
if(BUILD_TESTING)
    if(APPLE)
        # Qt's darwin multimedia backend only loads media under the cocoa
        # platform plugin; offscreen leaves QMediaPlayer in LoadingMedia forever.
        # cocoa opens real windows, so the keyboard checks then need a focused,
        # idle desktop. Set this to offscreen for a headless run that keeps every
        # other check deterministic and fails only the audio ones.
        set(SPUN_MACOS_TEST_PLATFORM "cocoa" CACHE STRING
            "Qt platform plugin for macOS test suites that decode audio")
        set(SPUN_AUDIO_PLATFORM "QT_QPA_PLATFORM=${SPUN_MACOS_TEST_PLATFORM}")
    else()
        set(SPUN_AUDIO_PLATFORM "QT_QPA_PLATFORM=offscreen")
    endif()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    spun_application(spun-diagnostics)
    target_sources(spun-diagnostics PRIVATE src/subsonictest.cpp src/jellyfintest.cpp src/youtubetest.cpp src/librarytest.cpp src/librarytest.h src/eventstest.cpp src/mediauitest.cpp src/threedtest.cpp src/artworktest.cpp src/recordertest.cpp src/turntabletest.cpp src/cassettedecktest.cpp src/cddecktest.cpp src/tx6test.cpp)
    target_sources(spun-diagnostics PRIVATE src/performancetest.cpp src/performancetest.h)
    target_compile_definitions(spun-diagnostics PRIVATE SPUN_DIAGNOSTICS)
    target_link_libraries(spun-diagnostics PRIVATE Qt6::Test)
    if(TARGET Qt6::Quick3D AND SPUN_ENABLE_3D)
        qt_add_executable(spun-geometry-test src/geometrytest.cpp)
        target_link_libraries(spun-geometry-test PRIVATE spun_core Qt6::Test)
        add_test(NAME spun-3d-geometry COMMAND spun-geometry-test)
        set_tests_properties(spun-3d-geometry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
    endif()
    qt_add_executable(spun-media-test src/mediatest.cpp)
    target_link_libraries(spun-media-test PRIVATE spun_core Qt6::Test)
    find_program(SPUN_DBUS_RUN_SESSION dbus-run-session)
    if(SPUN_DBUS_RUN_SESSION AND NOT APPLE)
        add_test(NAME spun-desktop-media COMMAND ${SPUN_DBUS_RUN_SESSION} -- $<TARGET_FILE:spun-media-test>)
        set_tests_properties(spun-desktop-media PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
    endif()
    qt_add_executable(spun-import-test src/importtest.cpp)
    target_link_libraries(spun-import-test PRIVATE spun_core Qt6::Test)
    add_test(NAME spun-folder-import COMMAND spun-import-test)
    set_tests_properties(spun-folder-import PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
    add_test(NAME spun-refresh-independent-motion COMMAND spun-diagnostics --test-performance)
    set_tests_properties(spun-refresh-independent-motion PROPERTIES ENVIRONMENT "SPUN_PERF_TIMING_ONLY=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic;QT_QPA_PLATFORMTHEME=")
    add_test(NAME spun-youtube COMMAND spun --test-youtube)
    set_tests_properties(spun-youtube PROPERTIES ENVIRONMENT "${SPUN_AUDIO_PLATFORM};QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic;QT_QPA_PLATFORMTHEME=")
    add_test(NAME spun-playback-and-ui COMMAND spun --self-test)
    set_tests_properties(spun-playback-and-ui PROPERTIES ENVIRONMENT "${SPUN_AUDIO_PLATFORM};QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic;QT_QPA_PLATFORMTHEME=")
endif()

if(BUILD_TESTING)
    qt_add_executable(spun-jellyfin-protocol-test src/jellyfinprotocoltest.cpp)
    target_link_libraries(spun-jellyfin-protocol-test PRIVATE spun_core Qt6::Test)
    add_test(NAME spun-jellyfin-protocol COMMAND spun-jellyfin-protocol-test)
    set_tests_properties(spun-jellyfin-protocol PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
endif()

if(BUILD_TESTING)
    qt_add_executable(spun-subsonic-protocol-test src/subsonicprotocoltest.cpp)
    target_link_libraries(spun-subsonic-protocol-test PRIVATE spun_core Qt6::Test)
    add_test(NAME spun-subsonic-protocol COMMAND spun-subsonic-protocol-test)
    set_tests_properties(spun-subsonic-protocol PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
endif()

if(BUILD_TESTING)
    qt_add_executable(spun-remote-library-test src/remotelibrarytest.cpp)
    target_link_libraries(spun-remote-library-test PRIVATE spun_core Qt6::Test)
    add_test(NAME spun-remote-library COMMAND spun-remote-library-test)
    set_tests_properties(spun-remote-library PROPERTIES ENVIRONMENT "${SPUN_AUDIO_PLATFORM};QT_QPA_PLATFORMTHEME=")
    add_test(NAME spun-immersive-media-ui COMMAND spun --test-media-ui)
    set_tests_properties(spun-remote-library PROPERTIES TIMEOUT 120 LABELS "remote;regression")
    set_tests_properties(spun-immersive-media-ui PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic;QT_QPA_PLATFORMTHEME="
        TIMEOUT 180 LABELS "ui;regression")
endif()
