#!/usr/bin/env bash
set -euo pipefail
spun_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -x "$spun_root/build/spun" ]]; then "$spun_root/scripts/build.sh"; fi
exec "$spun_root/build/spun" "$@"
