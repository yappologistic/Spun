# Recent-feature regression coverage

Build and run from the repository root:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure -j1
```

The recent improvement batch contains six features. The focused tests cover their behavior and failure paths:

| Feature | Automated coverage |
| --- | --- |
| Upcoming-song preload | One-song limit, ready-buffer consumption, failure fallback, queue removal, shuffle plan, bitrate invalidation, source cancellation and buffer cleanup. |
| Immersion | Ctrl+I, idle chrome, pointer and keyboard reveal, hidden tooltips, focus, help, Mini transitions, repeated toggles and queue/library restoration. |
| Browsing continuity | Navigation, search and scroll restoration, failed refresh, pagination, stale replies, six-page and 20,000-row limits, account/folder isolation. |
| Play next | Insert position, paused/current transport preservation, shuffle priority, manual skip under repeat-one, append distinction and rejection of foreign-account/non-song rows. |
| Retry | Repeated retry, late success/failure rejection, same queue entry, resumed audio, and the rendered Retry action. |
| Playlist actions | Cached and refreshed ownership permissions, owner deletion and read-only rejection through the server integrations. |

`spun-remote-library` runs nine additional controller scenarios for each of the Jellyfin and Subsonic/Navidrome source identities. Its controllable adapter intentionally completes cancelled requests to expose stale-result bugs. These cases complement the existing protocol tests; they do not replace real-server validation.

`spun-immersive-media-ui` runs automatically in CTest. To run the two focused suites:

```sh
ctest --test-dir build -L regression --output-on-failure -j1
```

All CTest UI runs use Qt's offscreen software renderer. Audio fixtures are muted. For app-only screenshots, run `spun --test-media-ui --capture-dir /tmp/spun-ui-captures` with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME=`.

The complete suite also checks local import, desktop media controls, frame timing, YouTube, playback, geometry, and Jellyfin/Subsonic protocols. Optional real-server runners are `scripts/test-jellyfin-server.py` and `scripts/test-subsonic-server.py`. Supply a server executable, `--test-binary build/spun`, and a new `--output` directory outside the repository. They create private loopback servers with generated music and temporary accounts, then stop the servers. Software rendering is the default; `--renderer 3d` uses offscreen Mesa rendering. These fixtures require Qt audio-service and local-socket access.
