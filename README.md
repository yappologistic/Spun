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

Play local music, or control Apple Music through Cider. Your album artwork becomes the disc, with a quiet interface inspired by Material Design 3 and colors that follow Noctalia. Browse songs, albums and playlists, search your library, and manage the queue without constantly switching apps.

**Source-available · PolyForm Noncommercial 1.0.0.** Free for the purposes allowed by the license, including personal and noncommercial use. This is not an OSI-approved open-source project. [Read the license details](#license).

## Install on CachyOS or Arch Linux

Spun currently builds from source; there is no packaged installer yet. Open a terminal and run:

```bash
sudo pacman -S --needed base-devel git cmake ninja python qt6-base qt6-declarative qt6-multimedia qt6-svg taglib

git clone https://github.com/yappologistic/Spun.git
cd Spun
./scripts/build.sh -DBUILD_TESTING=OFF
./scripts/install-launcher.sh
```

Open **Spun** from your application menu, or run `./scripts/run.sh` from the Spun folder. Keep that folder where it is: the launcher points to it. If you move it, run `./scripts/install-launcher.sh` again from its new location.

<details>
<summary>Building on another Linux distribution</summary>

Other Linux distributions need a C++20 compiler, CMake 3.22+, Ninja, pkg-config, Python 3, Qt 6.8+ with Quick Controls, Multimedia, SVG and development files, and TagLib 2.0+ development files. Package names differ between distributions. Spun is developed on CachyOS with Hyprland and Noctalia; desktop integration can vary elsewhere.

</details>

## Start listening

**Local music:** choose **Local**, then **+** to add tracks. You can also drop audio files or a folder onto Spun. Use **More → Add music folder** to include songs in artist and album subfolders. Spun shows import progress, skips songs already in the queue, and lets you cancel without adding a partial import. Directory symlinks inside the folder are not followed. Your music files are not copied or modified. **More → Play demo** plays the included original soundcheck.

**Apple Music through Cider:** open Cider, sign in there, and choose **Cider** in Spun. Cider still handles authentication and streaming and must remain running. Apple Music playback requires the appropriate Apple Music access through Cider.

For search, library browsing and the full queue:

1. Enable Cider's local API.
2. Open Spun’s **Queue** and choose **Connect to Cider**.
3. Approve Spun in Cider. Spun remembers the connection.

With older versions, choose **Use an app token** instead. Create one in Cider’s **Settings → Connectivity → Manage External Application Access**, allow playback, queue, library and audio access, then paste it into Spun.

Spun connects to Cider's local API on port 10767. Basic playback controls use Linux's media-player interface; working playback alone does not mean the API connection is ready. Available library and playback actions depend on your Cider version and its permissions.

## Inside the player

- **Queue:** use the top-right button to view tracks with artwork, search, reorder or remove them. The header estimates the time left for the current and upcoming songs, excluding repeats and future autoplay. Missing durations are marked as incomplete or a lower bound; queue search does not change the estimate.
- **Queue from here:** inside an album, playlist or saved queue, open a song’s **⋯** menu and choose **Queue from here**. Spun loads the remaining pages, then appends that song and everything after it in collection order, including tracks hidden by search. Navigating away cancels loading. Ranges containing unavailable tracks are rejected before any songs are queued; the limit is 5,000 songs. If Cider stops confirming additions, Spun reports partial progress without retrying uncertain writes.
- **Listening bookmarks:** choose **Bookmark this moment** in the current Cider song’s **⋯** menu. Find the bookmark in **Ctrl+K**, then select it to play from that position. Use its remove button to delete it. Up to 100 bookmarks are kept locally, with no downloaded audio. Spun verifies the playing song before seeking and checks the resulting position.
- **Session recovery:** enable **Remember Cider session** in Preferences. Spun checkpoints the current and upcoming songs when the queue changes and about every 15 seconds during playback. After a crash or an empty queue, choose **Recover session…** from Queue’s menu or Quick jump, then confirm. Recovery starts playback at the saved position and only restores into an empty Cider queue; it never clears an existing queue. Turning the preference off removes the checkpoint. Cider must be connected and the saved songs must remain available.
- **Quick jump:** press **Ctrl+K**, or choose **More → Quick jump**, to find pins, saved queues and tracks in the current queue. Type to filter, use ↑/↓ and Enter, or click a result. Queue results play the selected track; pins and saved queues open for browsing. Station pins open station search. Quick jump works in Mini mode too and keeps it compact when you play a song; Escape closes it. Results are limited to 40 matches, so narrow the search for long queues.
- **Browse:** in Cider mode, use the top-left search button for Apple Music search, your library and recently played tracks. Albums and playlists open to their track lists, with search inside each collection.
- **For You:** choose **For You** on the empty Search page for personalized albums, playlists and stations through your existing Cider connection. Open collections, play stations, or use their menus to queue or pin them. Recommendations retain their group context, omit repeated cards and load more pages on demand. Search filters the feed and checks remaining pages; Refresh fetches it again. There is no background recommendation polling. Empty results or missing permissions are shown explicitly; the feed is bounded to 1,000 cards and 100 pages.
- **Discography filters:** on an artist’s **Albums** tab, use **All releases** to switch to **Full albums**, **Singles & EPs**, or **Live albums**. Categories come from the catalog through Cider. Collection search and pagination work within the chosen category, and Back from an album restores the filter and your place.
- **Stations:** choose **Stations** in Search, or paste an Apple Music station link. Click a station to listen, and use its **⋯** menu to pin it. Station pins play directly from the empty Search page.
- **Recently added:** in Songs or Albums, select **Recently added** for newest-first browsing. **A–Z** restores alphabetical order; **History** opens recently played songs. Your sorting choice is remembered. Pages are sorted by Apple Music, without downloading the whole library.
- **Song radio:** choose **Start radio** in a song’s **⋯** menu, including the player and queue. Spun checks for an available station through Cider when the menu opens. Uploaded songs and some catalog songs have no station. Checking availability never starts playback.
- **Artist discovery:** choose **Similar** on an artist page to explore related artists. Back restores the previous artist, tab and scroll position.
- **Latest releases:** choose **Releases** beside your pins to see the latest available release from each pinned artist, newest first. This is a catalog view, not a notification feed or a complete release history. Search the list, open albums, or play and queue them normally. Results are fetched in batches only when opened and cached in memory for up to an hour; use Refresh to check again. Partial failures offer Retry and preserve previous results when possible.
- **Live sync:** while Spun is visible in Cider mode, Cider’s change events trigger batched queue readbacks and refresh open song/audio details. The event connection retries after outages; periodic queue reconciliation remains as a fallback. No extra runtime dependency is needed.
- **Artists:** search the **Artists** category, click a song’s artist name or choose **View artist** from its menu. Switch between **Top songs** and **Albums**, search either list, play or queue songs, and use Back to return. Top songs uses your existing Cider connection; no separate Apple developer account, token, or sign-in is needed. Ambiguous names stay in search so you can choose the right artist.
- **Drag to queue:** drag a song’s artwork sideways to reveal Queue, then drop it at the highlighted position among upcoming tracks. Dragging a selected song includes the whole selection. The edges scroll the queue; Escape or dropping outside returns to browsing. Spun checks the queue between writes and stops if Cider changes it.
- **Undo removal:** removing a queued song offers **Undo** for eight seconds. It restores the original position, including duplicates. Queue changes invalidate Undo; removing the currently playing song cannot undo playback.
- **Select tracks:** Ctrl-click individual songs, Shift-click a range, or choose **Select track** from a song’s **⋯** menu. Use the selection’s **⋯** to play next or add to queue in the displayed order. Ctrl+A selects loaded, available songs; Space toggles the focused song. Escape clears the selection. If Cider cannot confirm a batch, Spun stops and reports how many tracks were confirmed before you retry.
- **Shuffle collections:** use the Shuffle button beside Play in an album or playlist, or choose **Shuffle** in its menu, including pinned collections.
- **Recent searches:** submitted searches and searches whose results you open or play stay on this device. The empty Search page shows the last eight; use **×** to remove one. Typing alone and pasted music links are not saved.
- **Already queued:** a small check beside a song in browsing results marks it as current or upcoming in Cider. Matching uses song IDs, including library-to-catalog IDs when available. History is excluded, and you can still queue intentional repeats. Queue status refreshes while browsing; unavailable status hides the checks.
- **Queue multiselect:** Ctrl-click upcoming tracks, Shift-click a range, or use **Select track** in a song’s **⋯** menu. The selection’s **⋯** can move songs up, down, next or to the end, or remove them after a preview. Ctrl+A selects visible upcoming tracks; Escape clears selection. Spun preserves their order, protects the current song and history, and stops if Cider changes the queue.
- **Queue cleanup:** in Cider’s Queue, choose **⋯ → Remove duplicates…** or **Clear upcoming…**. Review the tracks before confirming. Cleanup preserves the current song and history, verifies each removal, and stops if the queue changes. Duplicate detection uses song IDs, not titles; it also removes upcoming repeats of the current song. Future autoplay can still add songs.
- **Pinned artists:** pin an artist from its page or **⋯** menu. Artist pins sit alongside your other pins and open **Top songs**, with **Albums** one click away. Pins are local to Spun and do not change Apple Music favorites.
- **Pinned collections:** choose **Pin collection** from an album or playlist’s **⋯** menu. Your pins appear on the empty Search page; click a cover to play, or use its menu to open or unpin it. Pins stay on this device.
- **Motion:** subtle spring feedback for controls and bounded fades follow [Material’s motion guidance](https://m3.material.io/styles/motion/overview/how-it-works), including reduced-motion preferences.
- **Connection recovery:** temporary API outages retry while the browser or queue is open. Browsing results stay visible; expired access offers reconnection. Spun never automatically replays a failed playback action.
- **Track actions:** use a song's **⋯** menu to play next or add it to the queue. Supported Cider actions also include favorites and saving songs to your library. Writing songs into playlists is not currently supported.
- **Add to saved queue:** choose this from a browsing song or selection’s **⋯** menu, or from a Queue selection. Pick an existing snapshot and choose whether to skip duplicate song IDs. This changes only the local snapshot; it does not alter playback or Apple Music playlists.
- **Saved queues:** in Cider’s Queue, open **⋯ → Save queue** and give it a name. Spun saves the current and upcoming tracks in order. Open **⋯ → Saved queues** to browse them, play them next or append them to the existing queue. These are local snapshots of track references, not Apple Music playlists or downloaded audio. Use its **⋯ → Rename saved queue**, or open it and use a song’s **⋯** menu to move or remove tracks. Moves are available when the track filter is empty. Edits save locally without touching Cider’s playing queue; empty snapshots can still be renamed or deleted. Deleting a snapshot asks for confirmation.
- **Undo saved edits:** after renaming a saved queue, moving/removing a saved track or appending songs, the notice offers **Undo** for eight seconds. It restores the last edit only. Another edit replaces that opportunity; external changes or deletion invalidate it. The previous version stays briefly on disk rather than as a second track list in RAM. This never changes Cider’s playing queue.
- **Audio settings:** **More → Preferences → Audio settings** contains crossfade, plus Automix and Off/Gaming/Unwind listening modes when your Cider version and token support them. Changes are read back from Cider before being shown as confirmed.
- **Song preferences:** the current song’s **⋯** menu includes **Suggest less like this** and **Clear dislike**, using Cider’s confirmed rating.
- **Audio quality:** choose **Audio quality** in the current song’s **⋯** menu. Spun displays the quality Cider reports and labels device output separately; it does not infer lossless or Atmos from device support. Details are fetched only when opened or refreshed.
- **Copy song link:** use **Copy song link** in the current song, browser or Cider queue’s **⋯** menu. Library songs use their public catalog identity; uploaded or unavailable songs may have no shareable link.
- **Music links:** paste an Apple Music song, album, playlist or station link into search, or drop it on the player. Spun shows the result before you choose playback.
- **Interface size:** choose 85–150% in **Preferences → Interface size**. Text, controls and artwork scale together; large layouts fit the available screen.
- **Fine seeking:** hold **Shift** while dragging a progress bar, the disc rim or vinyl needle. Progress bars preview your position and seek on release; **Esc** cancels. Cassette reels wind with the preview and respect reduced motion and the cassette sound preference.
- **Artwork:** choose **View artwork** from the player's **⋯** menu to inspect the full cover without the disc cutout.
- **Queue browsing:** scrolling away keeps your place when playback advances. **Current song** returns to the playing track and resumes following it.
- **Disc reverse:** double-click the disc or press **F** to see album details and tracks. Switch to lyrics with **Y** when available. Local lyrics can come from matching `.lrc` / `.txt` files or embedded metadata.
- **Disc appearance:** choose **CD**, **Vinyl** or **Cassette** in **More → Preferences**. Vinyl keeps the artwork on its center label, with grooves, a small spindle hole, and a gold tonearm. The arm lowers for playback, tracks inward through the song, and parks on pause. It scales with Mini mode, stays out of the reverse view and disc swaps, and respects reduced motion. Cassette mode adds a tape shell, animated reels, a horizontal progress bar and optional transport sounds. All three styles share playback, reverse-side details, lyrics and Mini mode. Your choice is remembered.
- **Vinyl controls:** choose 33⅓ or 45 RPM, enable a horizontal progress bar, or add optional crackle, hiss and groove skips in Preferences. Drag the needle onto the grooves to seek and play.
- **Mini mode:** a little disc with controls underneath. Optionally keep it above other windows in Preferences. Hover or keyboard-focus **Next** to preview the upcoming artwork, title and artist. Cider’s queue is checked on demand; an unknown next track is shown honestly, including local shuffle. No extra polling runs while the preview is closed.

**More → Preferences** contains CD/Vinyl appearance, the font picker, background blur, disc animation and other playback options. Spun follows Noctalia's colors and reduced-motion preference when available. Hyprland integration depends on the compositor's supported interfaces.

### Fonts

Spun uses your system font by default. Choose any installed family in **More → Preferences → Font**, or return to **System default**. Fonts are not bundled or downloaded automatically.

For the intended appearance, we recommend [Google Sans Flex](https://github.com/googlefonts/googlesans-flex/releases). Download the font from its official releases, install the TTF with your desktop's font installer, reopen Spun, and select it in Preferences. The font has its own [SIL Open Font License](https://github.com/googlefonts/googlesans-flex/blob/main/OFL.txt); keep the accompanying license with your font files.

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
| Flip disc / switch lyrics | F / Y |
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

You can then delete the Spun source folder. Your music stays where it was. Preferences remain in `~/.config/spun/` unless you choose to remove them separately.

## Troubleshooting and privacy

**Cider plays, but search or the queue fails:** check that Cider is running, its local API is enabled, and Spun's application token is valid with the required permissions. A playback connection and an API connection are separate.

**A font is missing:** install it on your system, reopen Spun and select it in Preferences. An unavailable saved font falls back to your system font.

**An audio file will not play:** supported formats depend on the codecs available to Qt Multimedia on your distribution.

Spun does not ask for your Apple Music password. The Cider application token is stored locally with owner-only file permissions in `~/.config/spun/cider-connection.json`; preferences live in `~/.config/spun/settings.ini`, and pinned collections in `~/.config/spun/cider-pins.json`. Artwork and music metadata may be fetched as part of playback and browsing. Do not share your token, private configuration, listening history or personal logs in issue reports.

This repository contains source code, license notices, UI icons, an original synthesized demo with generated cover art, and the approved project screenshot above. Private account configuration, logs and other desktop captures are excluded.

## Development

<details>
<summary>Build, test and benchmark</summary>

Build the diagnostic companion and run isolated playback, API-fixture and UI checks:

```bash
./scripts/build.sh -DBUILD_TESTING=ON
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic QT_QPA_PLATFORMTHEME= ./build/spun --self-test
```

For recursive folder-import checks, run `QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= ./build/spun-import-test`. Add `--stress` to test 20,000 nested sparse WAV files representing 625 GiB of audio without allocating that much disk space. The temporary files are removed after the run.

The tests use temporary preferences and synthetic local API fixtures. Audio checks need a working user audio session, and API fixtures need permission to listen on loopback. The normal player is built separately from the diagnostic executable.

For a connected Cider instance, `--inspect-cider` and `--inspect-library` check live data without changing playback. The opt-in `--verify-cider-writes` also exercises playback, volume, repeat, queue insertion/removal/Undo and audio settings, then starts song radio. It restores its temporary queue and setting edits but leaves radio playing. Use `--config /path/to/test/settings.ini` with a privately paired test profile; never share its connection file or captured library data.

For controlled performance measurements, run `python3 scripts/benchmark.py --output /tmp/spun-performance`. Its isolated scenes do not connect to your Cider instance. Offscreen measurements are useful comparisons, not whole-desktop GPU measurements. Regenerate the original soundcheck with `python3 scripts/make-demo.py` after building; this additionally requires FFmpeg.

</details>

Bug reports are welcome. Before submitting substantial code contributions, open an issue to discuss scope and contributor licensing. Any future commercial distribution needs appropriate rights to contributed code as well as compliance with third-party licenses; submitting a patch does not transfer its copyright.

## License

Spun's original code and assets are offered under the [PolyForm Noncommercial License 1.0.0](LICENSE), with the [required notice and third-party credits](NOTICE).

The license permits noncommercial use, specified personal uses, modification and redistribution under its terms. It also expressly permits use by certain charitable, educational, public research, public safety, health, environmental and government institutions, regardless of funding. It is therefore broader than “personal use only.” Uses outside its permissions require a separate license from the relevant rights holder. The full license controls.

This licensing choice allows the author to offer separate commercial terms for code they own in the future. No paid edition is being offered here.

Material Symbols Rounded icons remain under their own [Apache 2.0 license](licenses/MaterialSymbols-LICENSE.txt). Qt, TagLib and optional fonts retain their respective licenses; Spun's noncommercial terms do not replace those licenses. Spun is an independent project and is not endorsed by Google, Apple, Cider or Noctalia.
