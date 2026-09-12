#!/usr/bin/env bash
set -euo pipefail
spun_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$spun_root/work/youtube-preview"
exec "$spun_root/build/spun" --isolated --youtube --config "$spun_root/work/youtube-preview/settings.ini"
