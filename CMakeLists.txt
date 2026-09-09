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
qt_policy(SET QTP0004 NEW)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib>=2.0)
qt_add_library(spun_core STATIC
    src/symbol.cpp src/symbol.h src/typography.cpp src/typography.h
    src/listening.cpp src/listening.h src/tapesound.cpp src/tapesound.h src/vinylnoise.cpp src/vinylnoise.h
    src/musicactions.cpp src/musicactions.h
    src/library.cpp src/library.h src/lyrics.cpp src/lyrics.h src/artwork.cpp src/artwork.h
    src/player.cpp src/player.h src/disc.cpp src/disc.h src/mpris.cpp src/mpris.h
    src/theme.cpp src/theme.h src/cider.cpp src/cider.h src/ciderevents.cpp src/ciderevents.h)
target_link_libraries(spun_core PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::QuickControls2
    Qt6::Svg Qt6::Multimedia Qt6::Concurrent Qt6::DBus Qt6::Network PkgConfig::TAGLIB)
target_compile_definitions(spun_core PUBLIC SPUN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(spun_core PRIVATE -Wall -Wextra -Wpedantic)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Keep Release code compact without changing floating-point semantics or
    # disabling exceptions. The animated geometry retains full speed optimization.
    target_compile_options(spun_core PRIVATE "$<$<CONFIG:Release>:${SPUN_SIZE_OPTIMIZATION};-ffunction-sections;-fdata-sections>")
    set_source_files_properties(src/disc.cpp PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-O3>")
endif()
file(GLOB SPUN_SYMBOLS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg")
set_source_files_properties(qml/SpunStyle.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# The font picker is prepared asynchronously on opening Preferences. Its source
# remains bundled; there is no need to carry another copy as startup bytecode.
set_source_files_properties(qml/FontPicker.qml PROPERTIES QT_QML_SKIP_CACHEGEN TRUE)
function(spun_application target)
    qt_add_executable(${target} src/main.cpp)
    target_link_libraries(${target} PRIVATE spun_core)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(${target} PROPERTIES QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode")
    # Compile QML at build time, preserving existing resource URLs and the UI.
    qt_add_qml_module(${target} URI SpunUi VERSION 1.0 NO_PLUGIN
        NO_RESOURCE_TARGET_PATH RESOURCE_PREFIX "/" OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${target}-qml"
        QML_FILES qml/SpunButton.qml qml/SpunSlider.qml qml/SeekSlider.qml qml/QuickJump.qml qml/NextTrackTip.qml qml/SavedQueuePicker.qml qml/Tonearm.qml qml/CassetteReels.qml qml/SpunText.qml qml/FontPicker.qml qml/SpunStateLayer.qml qml/SpunSpring.qml qml/SpunPopupEnter.qml qml/SpunStyle.qml qml/TrackContent.qml qml/SpunSearchField.qml qml/MenuEntry.qml qml/PreferenceSwitch.qml qml/TrackMenu.qml qml/Main.qml qml/LibraryPanel.qml qml/SpunToolTip.qml qml/IconButton.qml qml/Glyph.qml)
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
        target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:-Wl,--gc-sections>")
        if(SPUN_RELR_RUNTIME AND SPUN_RELR_SUPPORTED)
            target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:LINKER:-z,pack-relative-relocs>")
        endif()
    endif()
endfunction()
spun_application(spun)
if(CMAKE_STRIP AND CMAKE_BUILD_TYPE STREQUAL "Release")
    # Debug/RelWithDebInfo builds retain symbols; strip only the shipped Release app.
    add_custom_command(TARGET spun POST_BUILD COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:spun>)
endif()
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    spun_application(spun-diagnostics)
    target_sources(spun-diagnostics PRIVATE src/librarytest.cpp src/librarytest.h src/eventstest.cpp)
    target_compile_definitions(spun-diagnostics PRIVATE SPUN_DIAGNOSTICS)
    target_link_libraries(spun-diagnostics PRIVATE Qt6::Test)
    add_test(NAME spun-playback-and-ui COMMAND spun --self-test)
    set_tests_properties(spun-playback-and-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic;QT_QPA_PLATFORMTHEME=")
endif()
