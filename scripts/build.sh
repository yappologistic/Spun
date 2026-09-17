#!/usr/bin/env bash
set -euo pipefail
spun_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
spun_args=()
if [[ "$(uname -s)" == Darwin ]]; then
    # Homebrew keeps Qt and taglib outside CMake's and pkg-config's default paths.
    if command -v brew >/dev/null; then
        brew_prefix="$(brew --prefix)"
        spun_args+=("-DCMAKE_PREFIX_PATH=$brew_prefix/opt/qt;$brew_prefix/opt/taglib;$brew_prefix")
        export PKG_CONFIG_PATH="$brew_prefix/opt/taglib/lib/pkgconfig:$brew_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    fi
fi
cmake -S "$spun_root" -B "$spun_root/build" -G Ninja "${spun_args[@]}" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$spun_root/build" --parallel 4
