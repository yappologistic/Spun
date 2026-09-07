#!/usr/bin/env bash
set -euo pipefail
spun_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
spun_apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
mkdir -p "$spun_apps"
python3 - "$spun_root" "$spun_apps/spun.desktop" <<'PY'
import pathlib, sys
root, destination = map(pathlib.Path, sys.argv[1:])
def escape(value):
    return str(value).replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%')
content = f'''[Desktop Entry]
Type=Application
Name=Spun
Comment=CD-shaped music player for local files and Cider.
Exec="{escape(root / 'scripts/run.sh')}" %F
Icon={root / 'assets/spun.svg'}
Terminal=false
Categories=AudioVideo;Audio;Player;
MimeType=audio/mpeg;audio/flac;audio/ogg;audio/x-wav;audio/mp4;
StartupNotify=true
StartupWMClass=spun
'''
destination.write_text(content)
print(f'Installed {destination}')
PY
if command -v update-desktop-database >/dev/null; then update-desktop-database "$spun_apps"; fi
