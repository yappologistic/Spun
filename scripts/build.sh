#!/usr/bin/env bash
set -euo pipefail
spun_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "$spun_root" -B "$spun_root/build" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$spun_root/build" --parallel 4
