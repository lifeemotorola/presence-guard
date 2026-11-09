# OnAirTwin for OBS Studio

**Rehearse a failure without touching the broadcast.** OnAirTwin is a local,
read-only OBS dock for producers who want to understand which scenes reference
a camera, microphone, or other source and whether their chosen backup is
configured in those scenes. It is an **early beta**, not a safety system.

Created by **Emmanuel Suah**. Built against **OBS Studio 33.0.1** (Windows x64).
The UI/model have automated fake-OBS tests; unless specifically stated otherwise,
they are not an end-to-end test in your OBS installation.

## Download and install (no compiler required)

1. Open this repository on GitHub → **Actions → OnAirTwin**.
2. Open the newest **successful** run. At the bottom, under **Artifacts**,
   download **`obs-onair-twin-windows-x64`**. Extract the ZIP.
3. Close OBS. In the extracted folder, right-click **`Install.bat`** →
   **Run as administrator**. If an earlier copy is installed, back it up and
   remove that plugin folder first; the installer will not overwrite it.
4. Start OBS Studio 33 → **Docks → OnAirTwin**. Look for
   `[obs-onair-twin] loaded` in **Help → Log Files → View Current Log** if needed.

The installer places the DLL and locale data under
`C:\ProgramData\obs-studio\plugins\obs-onair-twin\`. The artifact contains
`SOURCE-CODE.txt` pointing to the corresponding source, `LICENSE`, and a SHA-256
checksum for the DLL. The plugin is **unsigned**; a matching checksum only
establishes consistency with that artifact, not publisher identity or safety.
To uninstall, close OBS and run `Uninstall.bat` as administrator. Your locally
saved baseline remains under OBS's `plugin_config\obs-onair-twin` folder.

For OBS 32 or older, the optional PowerShell `-LegacyLayout` switch uses the
old `bin\64bit` layout, but **compatibility with older OBS versions is not
verified**. This beta is intended for OBS 33.0.1.

## How to use it

1. **Scan OBS setup.** The dock reads the current scene collection, its nested
   scenes and groups, source visibility flags, mute state and global-audio
   channels. It does not change any of them.
2. When the production is configured as intended, **Save current baseline**.
   The single local baseline is saved atomically. Replacing an existing one
   requires confirmation. On future scans the report compares source identities,
   names, scene/item membership, visibility flags, mute state and global-audio
   channel assignments. Switching scene alone is not reported as drift. A
   baseline from another collection is not compared.
3. Pick the source to assume unavailable and, optionally, an **operator-selected
   backup**. Press **Simulate failure**. It scans again and lists scenes that
   reference the selected source, with enabled/disabled scene-item paths and
   the backup's configured paths. Global audio is listed separately. Muted or
   capability-mismatched backups are flagged.
4. **Export HTML report** saves the snapshot and scenario in a portable file.
   Open it in a browser to print or save as PDF. It is a report of that **scan
   time**, not a continuously updating monitor.

Nothing is disconnected, muted, switched, or enabled by this plugin. A backup
may be present but still require a manual scene/source switch; OnAirTwin does
**not** perform failover. Partial or inconsistent scans are rejected rather
than reported as clean. Large or cyclic configurations may be marked incomplete.

## Important limits

- An enabled path is an OBS scene visibility setting, **not proof of visible
  pixels or audible sound**. Audio visibility, track routing and global output
  interact in ways this model does not test.
- A simulated failure does not physically unplug or test a camera/mic. It cannot
  detect a frozen frame, black camera, silent mic, broken capture driver, filter,
  crop/transform, transition, occlusion, missing file, platform outage, or what
  viewers actually see. Backup compatibility and switching must be rehearsed in
  the real setup, ideally with a private test and viewer-side monitoring.
- The baseline compares only the properties listed above. **No detected change
  does not mean the show is safe.** It is not a backup or restore tool.
- Source and scene **names** are saved in the baseline and appear in exports.
  The plugin does not intentionally read source settings, URLs, file paths,
  stream keys, passwords, media, or audio/video. A user can put private text in
  a name, so inspect a report before sharing. No account, telemetry or network
  request is made by OnAirTwin.
- The snapshot can become stale as soon as OBS changes. Scan again before using
  it. User-defined settings, third-party modules and multi-canvas/output
  behavior are not comprehensively modeled.

## Why this idea might sell

Working name, not a trademark clearance. For churches, schools, sports clubs and
small event-production teams, a paid event can be much more costly to fail than
a casual stream. The product hypothesis is a simple rehearsal report and
configuration-change history for those teams, rather than another general
health dashboard. [Stream Health Monitor](https://obsproject.com/forum/resources/stream-health-monitor-dock-fix-it-assistant.2358/)
already offers health checks and a pre-stream checklist, and
[Scene Collection Manager](https://obsproject.com/forum/resources/scene-collection-manager.1434/)
already offers collection backups. **We cannot guarantee this concept is
unique, market-leading, or commercially successful.**

Before setting a price, interview at least five potential paying production
teams, run real rehearsals with them and ask for paid pilot commitments. An
annual support/updates price in the **$79–$149 range** is only a hypothesis to
test, not a validated market price. A sellable release would need real OBS
integration tests, better production reporting, signed distribution, support and
licensing review. OBS says distributed plugin binaries must give recipients a
way to access corresponding source; recipients also have GPL redistribution
rights. Selling builds, support or services can still be possible, but get legal
advice before committing to a business model.

## Developers

`./tests/run-tests.sh` runs the pure C++ model and the real snapshot adapter
against fake OBS. The Windows workflow also builds Qt JSON/report tests and
runs the actual dock against fake OBS in an offscreen Qt session. These tests do
not replace a real OBS/device/output test. Build with `cmake --preset windows-x64`
and `cmake --build --preset windows-x64 --config RelWithDebInfo` after installing
Visual Studio 2022 and CMake 3.28+. Distributed under GPL-2.0-or-later; see
`LICENSE` and `SOURCE-CODE.txt` in the build artifact.
