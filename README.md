<h1 align="center">Spun</h1>

<p align="center">A CD, vinyl and cassette music player for Linux.</p>

<p align="center">
  <img src="assets/screenshots/spun-vinyl.png" alt="Spun in vinyl mode with a gold tonearm and the Cider queue alongside it" width="1000">
</p>

<p align="center">
  <a href="https://buymeacoffee.com/E_Gurl">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Support Spun on Buy Me a Coffee" width="217" height="60">
  </a>
  <br>
  <sub>Optional support for Spun's development.</sub>
</p>

<p align="center">
  <a href="#install-on-cachyos-or-arch-linux">Install</a> ·
  <a href="#start-listening">Get started</a> ·
  <a href="#inside-the-player">Features</a> ·
  <a href="#troubleshooting-and-privacy">Help</a> ·
  <a href="#license">License</a>
</p>

Play local music or control Apple Music through Cider. Spun puts your album artwork on a spinning CD, vinyl record, cassette or TP-7-inspired recorder, with an interface inspired by Material Design 3. Optional 3D players add physical depth and lighting that follows Noctalia's wallpaper palette.

**Source-available · PolyForm Noncommercial 1.0.0.** Personal and other permitted noncommercial use is free. This is not an OSI-approved open-source license. [Read the license details](#license).

## Install on CachyOS or Arch Linux

Spun builds from source; there is no packaged installer yet. Open a terminal and run:

```bash
sudo pacman -S --needed base-devel git cmake ninja python qt6-base qt6-declarative qt6-multimedia qt6-svg taglib

git clone https://github.com/yappologistic/Spun.git
cd Spun
./scripts/build.sh -DBUILD_TESTING=OFF
./scripts/install-launcher.sh
```

**For 3D players**, also install `qt6-quick3d` and run the build command again. Spun includes 3D when that dependency is available. To build without it, add `-DSPUN_ENABLE_3D=OFF` to the build command.

Open **Spun** from your application menu, or run `./scripts/run.sh` from its folder. The launcher points to that folder. If you move it, run `./scripts/install-launcher.sh` again.

<details>
<summary>Building on another Linux distribution</summary>

You need a C++20 compiler, CMake 3.22+, Ninja, pkg-config, Python 3, Qt 6.8+ with Quick Controls, Multimedia, SVG and development files, and TagLib 2.0+ development files. Qt Quick 3D is optional. Package names differ between distributions. Spun is developed on CachyOS with Hyprland and Noctalia; desktop integration can vary elsewhere.

</details>

## Start listening

**Local music:** choose **Local**, then **+** to add tracks, or drop files and folders onto Spun. **More → Add music folder** includes artist and album subfolders. Imports show progress, skip songs already in the queue and can be cancelled without adding a partial import. Directory symlinks inside the folder are not followed. Your music files are not copied or modified. **More → Play demo** plays the included original soundcheck.

**Apple Music through Cider:** open Cider, sign in there, then choose **Cider** in Spun. Cider handles authentication and streaming and must remain running. Apple Music playback requires the appropriate access through Cider.

For search, browsing and the full queue:

1. Enable Cider's local API.
2. Open Spun's **Queue** and choose **Connect to Cider**.
3. Approve Spun in Cider. Spun remembers the connection.

For older Cider versions, choose **Use an app token** instead. Create one in Cider's **Settings → Connectivity → Manage External Application Access**, allow playback, queue, library and audio access, then paste it into Spun.

Spun connects to the local API on port 10767. Basic playback controls use Linux's media-player interface, so working playback alone does not confirm API access. Available actions depend on your Cider version and permissions.

## Inside the player

### CD, vinyl, cassette and TP-7

Choose a medium in **More → Preferences**. All four share playback controls, album details, lyrics and Mini mode.

- **CD:** album artwork on the disc, with gentle rotation or an optional **500 RPM** setting. This changes the visual speed only.
- **Vinyl:** a record with grooves and Relaxed, 33⅓ or 45 RPM rotation. Drag the needle to seek and play. The 3D turntable has a hinged dust cover, working speed buttons and a cue lever that lifts or lowers the needle to pause or play. Drag its housing to rotate it; double-click to reset the angle. Optional effects include crackle, hiss and groove skips.
- **Cassette:** Clear, Smoke and Cream finishes, moving tape reels and optional transport sounds. Reels turn counterclockwise during forward playback and reverse while rewinding.
- **TP-7:** a metal recorder with a draggable wheel, track display and working transport, rocker and volume controls. In 3D, drag the body to rotate it horizontally or tilt it vertically; double-click to reset the angle. The lower keys are Previous, Play/Pause and Next. The rocker seeks ten seconds, and the knob controls volume. The side keys open album details, the queue and the player menu. This is a playback appearance, without microphone recording.
- **Player bodies:** **Show player body** adds a turntable, portable cassette player or CD tray with a clear lid. Album changes exchange the medium with a brief sleeve or case animation. Tracks on the same album keep it in place, and playback starts immediately.
- **3D:** enable **3D player** for modeled discs, reels, cases and tonearm parts, with metallic surfaces and lighting tinted by Noctalia's current palette. Playback controls stay in their familiar positions. Mini mode remains compact, and reverse-side details stay flat for reading.
- **Seeking:** use the disc rim, vinyl needle or optional horizontal progress bar. Their previews stay in sync. Hold **Shift** for finer adjustments and **Esc** to cancel a drag.
- **Albums as records:** enable **Play albums as records** in Vinyl preferences to give each album track a groove band. A needle drop selects the song and position. Local mode uses loaded album tracks in track-number order; Cider starts the album and verifies the track before seeking. Missing durations or unavailable tracks fall back to single-song seeking. The circular and horizontal bars share the album timeline, so either can select a track and position.
- **Reverse view:** press **F** for album details and tracks. The disc and cassette also support double-clicking to flip. Press **Y** for lyrics when available. Local lyrics can come from matching `.lrc` or `.txt` files or embedded metadata. **View artwork** opens the full cover.
- **Mini mode:** a compact player with controls below the medium and an optional always-on-top setting. Hover or focus Next to preview the upcoming song when known.

### Browse through Cider

Use the top-left search button to browse songs, albums, playlists and artists without leaving Spun.

- Search Apple Music, browse your library or recently played tracks, and filter songs inside albums and playlists.
- Choose **Recently added** in Songs or Albums for newest-first browsing. Spun remembers your sorting choice.
- Open **For You** on the empty Search page for personalized albums, playlists and stations. Recommendations load when requested, with a manual Refresh option.
- Explore artist top songs, albums and similar artists. Filter discographies by full albums, singles and EPs, or live albums.
- Pin artists, collections and stations. **Releases** shows the latest available release from each pinned artist when you open it.
- Play stations, start song radio when available, or paste an Apple Music song, album, playlist or station link into Search.
- Use song menus to favorite, save to your library, copy a public song link or adjust song recommendations where Cider supports the action. Writing to Apple Music playlists is not currently supported.

### Queue and listening tools

- **Queue:** view artwork, search, reorder and remove tracks. The header estimates remaining listening time. Scrolling away keeps your place; **Current song** returns to playback.
- **Multiple tracks:** Ctrl-click individual tracks or Shift-click a range. Use the selection menu to queue or move them. Drag artwork sideways to reveal Queue and drop at a position among upcoming tracks.
- **Queue from here:** append a song and the rest of its album, playlist or saved queue in collection order, including tracks hidden by search. Spun loads remaining pages first; navigating away cancels loading.
- **Cleanup and Undo:** review duplicate removal or clearing upcoming songs before confirming. Removing a queued song offers a brief Undo when the queue has not changed. The current song and playback history are protected during cleanup.
- **Saved queues:** save the current and upcoming tracks as a local snapshot. Rename it, reorder or remove songs, append more tracks, and undo the last edit. Snapshots contain track references, not downloaded audio or Apple Music playlists.
- **Already queued:** a check beside browsing results marks current or upcoming songs. You can still add intentional repeats.
- **Quick jump:** press **Ctrl+K** to find queue tracks, pins, saved queues and listening bookmarks. Selecting a queue track plays it. It also works in Mini mode.
- **Bookmarks:** choose **Bookmark this moment** from the current Cider song's menu. Open it in Quick jump to resume at that position.
- **Session recovery:** optionally enable **Remember Cider session** in Preferences. After a crash or an empty queue, choose **Recover session…** from Queue or Quick jump. Recovery asks for confirmation and only restores into an empty Cider queue. Turning the preference off removes the checkpoint.

Cider changes refresh the visible queue and relevant details. Temporary connection failures preserve browsing results and offer reconnection when needed. Batch operations stop and report partial progress when Cider cannot confirm a change; uncertain playback actions are not retried automatically.

### Preferences and desktop integration

Choose your font, interface size, background blur, animations and media appearance in **More → Preferences**. Spun follows Noctalia's colors and reduced-motion setting when available. Reduced motion also skips loading sequences and perspective tilt.

Spun's desktop media entry follows the selected Local or Cider source, including artwork, playback state, volume and seeking. Cider may also expose its own entry. Hyprland integration depends on the compositor's supported interfaces.

**Audio settings** contains crossfade and, where supported by Cider, Automix and listening modes. **Audio quality** in the current song's menu shows what Cider reports and labels device output separately.

### Fonts

Spun uses your system font by default. Choose any installed family in **Preferences → Font**. Fonts are not bundled or downloaded automatically.

For the intended appearance, we recommend [Google Sans Flex](https://github.com/googlefonts/googlesans-flex/releases). Install the TTF with your desktop's font installer, reopen Spun and select it in Preferences. The font has its own [SIL Open Font License](https://github.com/googlefonts/googlesans-flex/blob/main/OFL.txt).

<details>
<summary><strong>Keyboard shortcuts</strong></summary>

| Action | Shortcut |
| --- | --- |
| Play / pause | Space |
| Previous / next track | Ctrl + Left / Right |
| Seek backward / forward | Left / Right |
| Add files / folder | Ctrl + O / Ctrl + Shift + O |
| Queue | Ctrl + L |
| Cider browser | Ctrl + B |
| Search the current panel | Ctrl + F |
| Quick jump | Ctrl + K |
| Select browser / upcoming queue tracks | Ctrl + A (track list focused) |
| Toggle focused browser track | Space (track list focused) |
| Select tracks / range | Ctrl-click / Shift-click |
| Mini mode | Ctrl + M |
| Flip medium / switch lyrics | F / Y |
| Mute | M |
| Keyboard help | F1 |
| Back / dismiss | Escape |
| Quit | Ctrl + Q |

</details>

## Update or remove

To update, open a terminal in your Spun folder:

```bash
git pull --ff-only
./scripts/build.sh -DBUILD_TESTING=OFF
```

To remove the application-menu entry:

```bash
rm "${XDG_DATA_HOME:-$HOME/.local/share}/applications/spun.desktop"
```

You can then delete the Spun source folder. Your music stays where it was. Preferences remain in `~/.config/spun/` unless you remove them separately.

## Troubleshooting and privacy

- **Cider plays, but search or the queue fails:** check that its local API is enabled and Spun's token has the required permissions. Playback and API connections are separate.
- **3D is unavailable:** install Qt Quick 3D and rebuild. Qt's software scenegraph backend does not support the 3D view; the regular player remains available.
- **A font is missing:** install it, reopen Spun and select it again. An unavailable saved font falls back to the system font.
- **An audio file will not play:** supported formats depend on the codecs available to Qt Multimedia on your distribution.

Spun does not ask for your Apple Music password. Its Cider token is stored with owner-only file permissions in `~/.config/spun/cider-connection.json`. Preferences and local listening data also stay in `~/.config/spun/`. Artwork and music metadata may be fetched during playback and browsing. Do not include tokens, private configuration, listening history or personal logs in issue reports.

## Development

<details>
<summary>Build, test and benchmark</summary>

Build the diagnostic companion and run the registered tests:

```bash
./scripts/build.sh -DBUILD_TESTING=ON
ctest --test-dir build --output-on-failure
```

Tests use temporary preferences and synthetic local API fixtures. Audio checks need a working user audio session, and API fixtures need permission to listen on loopback. Desktop-control tests use a private D-Bus session. Diagnostics are separate from the normal player.

For focused player, artwork, lighting, geometry and media checks:

```bash
./scripts/test-3d.sh
```

The default uses offscreen CPU rendering through Mesa and requires an available X display for its OpenGL context. It does not create visible windows. It checks recorder and turntable controls, physical rotation, seeking, rendered artwork and palette changes without exercising the full native window lifecycle.

For the complete 3D interaction and Cider-fixture suites, run `SPUN_TEST_RENDERER=native ./scripts/test-3d.sh` in a separate Wayland or X11 test session. This includes projected seeking, tonearm gestures, lid transitions, scaling and mode combinations. Set `SPUN_TEST_SCREEN` to the dedicated output name and `SPUN_TEST_OUTPUT` to a local capture directory. Missing 3D rendering fails the checks instead of silently skipping them.

For large imports, run `QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= ./build/spun-import-test --stress`. This creates 20,000 temporary sparse WAV files representing 625 GiB of audio without allocating that much disk space, then removes them.

`--inspect-cider` and `--inspect-library` inspect a live Cider connection without changing playback. The opt-in `--verify-cider-writes` changes playback, queue and audio settings and leaves song radio playing after restoring its temporary edits. Use a privately paired test profile with `--config /path/to/test/settings.ini`; never share its connection file or captured library data.

Run `python3 scripts/benchmark.py --output /tmp/spun-performance` for isolated performance comparisons. Offscreen results are not whole-desktop GPU measurements. `python3 scripts/make-demo.py` regenerates the original soundcheck after building and additionally requires FFmpeg.

</details>

Before submitting substantial code contributions, open an issue to discuss scope and contributor licensing. A patch does not transfer its copyright; future commercial distribution needs appropriate rights to contributed code and compliance with third-party licenses.

## License

Spun's original code and assets use the [PolyForm Noncommercial License 1.0.0](LICENSE), with [required notices and third-party credits](NOTICE).

The license permits noncommercial use, specified personal uses, modification and redistribution under its terms. It also permits use by certain charitable, educational, public research, public safety, health, environmental and government institutions, regardless of funding. It is broader than personal use only. Uses outside its permissions require a separate license from the relevant rights holder. The full license controls.

The author may offer separate commercial terms for code they own in the future. No paid edition is offered here.

Material Symbols Rounded icons retain their [Apache 2.0 license](licenses/MaterialSymbols-LICENSE.txt). Qt, TagLib and optional fonts retain their respective licenses. Spun is an independent project and is not endorsed by Google, Apple, Cider or Noctalia.
