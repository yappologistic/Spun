#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
command -v node >/dev/null || { echo 'Install Node.js before setting up YouTube Music.' >&2; exit 1; }
python3 -m venv "$root/runtime/youtube"
"$root/runtime/youtube/bin/python" -m pip install -r "$root/helper/requirements.txt"
echo 'YouTube Music is ready. No Google account is needed.'
