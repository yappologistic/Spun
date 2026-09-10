#!/usr/bin/env bash
set -euo pipefail

# The default checks use CPU rendering and do not create visible windows.
# Set SPUN_TEST_RENDERER=native only in a separate test desktop/session.
# Fixtures never connect to the user's Cider account.
cd "$(dirname "$0")/.."
renderer=${SPUN_TEST_RENDERER:-cpu}
if [[ $renderer == cpu ]]; then
    if [[ -z ${DISPLAY:-} ]]; then
        echo 'CPU 3D rendering needs an existing X display socket for its software GL context.' >&2
        exit 2
    fi
    export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl QSG_RENDER_LOOP=basic
    export LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa LP_NUM_THREADS=2
    export QT_QPA_PLATFORMTHEME=
    suites=(artwork 3d-lighting media-ui)
    echo 'Running focused offscreen CPU checks. Native lifecycle tests require SPUN_TEST_RENDERER=native.'
elif [[ $renderer == native ]]; then
    if [[ ${QT_QUICK_BACKEND:-} == software || ${QT_QPA_PLATFORM:-wayland} == offscreen || ( -z ${WAYLAND_DISPLAY:-} && -z ${DISPLAY:-} ) ]]; then
        echo 'Native lifecycle tests require a separate Wayland or X11 test session.' >&2
        exit 2
    fi
    suites=(artwork 3d-lighting 3d-ui 3d-library media-ui)
else
    echo 'SPUN_TEST_RENDERER must be cpu or native.' >&2
    exit 2
fi
build_dir=${SPUN_BUILD_DIR:-build}
report_dir=${SPUN_TEST_OUTPUT:-/tmp/spun-3d-tests}
mkdir -p "$report_dir"
if [[ ! -x "$build_dir/spun-geometry-test" || ! -x "$build_dir/spun-diagnostics" ]]; then
    echo 'Build with SPUN_ENABLE_3D=ON and BUILD_TESTING=ON first.' >&2
    exit 2
fi
QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= "$build_dir/spun-geometry-test" 2>&1 | tee "$report_dir/geometry.log"
for suite in "${suites[@]}"; do
    suite_env=()
    if [[ $renderer == cpu && $suite == media-ui ]]; then suite_env=(env QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic); fi
    timeout "${SPUN_TEST_TIMEOUT:-240}s" nice -n 10 "${suite_env[@]}" "$build_dir/spun" "--test-$suite" --capture-dir "$report_dir/$suite" 2>&1 | tee "$report_dir/$suite.log"
done
