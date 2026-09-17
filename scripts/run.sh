#!/usr/bin/env bash
set -euo pipefail
spun_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
spun_binary="$spun_root/build/spun"
# macOS builds ship as a bundle; the executable lives inside it.
if [[ "$(uname -s)" == Darwin ]]; then spun_binary="$spun_root/build/spun.app/Contents/MacOS/spun"; fi
if [[ ! -x "$spun_binary" ]]; then "$spun_root/scripts/build.sh"; fi
exec "$spun_binary" "$@"
