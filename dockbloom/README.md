# DockBloom for OBS Studio

**One place for more of your show.** DockBloom, by Emmanuel Suah, is a Windows OBS plugin that brings selected docks together without taking their contents away from OBS or other plugins.

- Include existing OBS docks, existing browser docks, and compatible third-party plugin docks. Add your own web docks with an `http://` or `https://` URL.
- **Focus** shows one included dock. **Grid** shows up to four at a time, with pages for the rest. The list holds up to 256 docks, including up to 100 DockBloom web docks; this is a DockBloom limit, not a statement about an OBS limit.
- Search your list, switch to an included dock, and restore a snapshot of the previous OBS layout. The **? Help** button opens an offline guide inside OBS.

## Requirements and download

- Windows x64, OBS Studio 33.x (the build targets and is tested against OBS **33.0.1**), with OBS Browser enabled for DockBloom web docks. Other OBS versions and all third-party docks cannot be guaranteed compatible.
- Download the prebuilt `obs-dockbloom-windows-x64` artifact from a **successful DockBloom run** under the repository's **Actions** tab. Scroll to the bottom of the run page to **Artifacts**. The repository's Code/Download ZIP is source code, not the plugin.
- The build is unsigned. Check the source and hashes if you wish; do not disable Windows security globally.

## Install on Windows

1. Close OBS completely. Extract the downloaded artifact ZIP using **Extract All**. Open the extracted folder containing `Install.bat` and the `bin` and `data` folders.
2. Right-click **Install.bat** and choose **Run as administrator**. The installer only adds `%ProgramData%\obs-studio\plugins\obs-dockbloom`; it refuses to overwrite an existing folder. If Windows asks for permission, inspect the package and approve only if you trust it.
3. Start OBS. Open **Docks → DockBloom**. If you do not see it, verify OBS is 64-bit version 33 and the DLL and locale files are in the plugin folder, then restart OBS.

To install manually, create `%ProgramData%\obs-studio\plugins\obs-dockbloom`, copy `bin\64bit\obs-dockbloom.dll` directly into it, and copy the package's `data` folder alongside the DLL. Do not copy the test driver or another plugin's files. OBS portable installations can use their own `plugins` folder instead; see `Install.ps1 -PluginRoot <path>`. The script's optional `-LegacyLayout` uses `bin\64bit` under the destination if needed.

## First use

1. In OBS, unlock docks if **Docks → Lock Docks** is checked. Open **Docks → DockBloom**. The separate **DockBloom Workspace** dock is the workspace anchor. It can be moved to a different OBS dock area before arranging.
2. In DockBloom, click **Refresh** to discover newly loaded docks. Check any combination of available docks; checking alone does not move them. Click **+ Add web**, enter a name and trusted URL, and select **Add dock** to add a new browser panel.
3. Choose **Focus view** or **Grid view** and confirm the first arrangement. DockBloom first saves a snapshot of the entire OBS dock layout, then moves the selected native docks. **Focus** shows the selected included dock. In **Grid**, use **Previous** / **Next**; select a dock and click **Show selected dock** to jump to its page.
4. To undo the arrangement, click **Restore previous OBS layout** and confirm. This restores the whole saved layout, so it can overwrite layout changes made after the snapshot. Docks added or removed since then may need manual adjustment. The selection is retained for later use.

Arrange and test your workspace **before going live**. Docks on other grid pages and nonfocused docks are hidden; they can still consume resources or have their own behavior. If you uncheck a dock after arranging, it stays in its current position/visibility until manually moved or the snapshot is restored. The snapshot persists until restored; subsequent arrangements do not overwrite it.

## Important limits and privacy

- DockBloom moves the actual native `QDockWidget`s using Qt's main-window docking API; it does not extract their contents or intentionally change scenes, sources, audio, or stream settings. Some third-party docks, browser pages, custom/native widgets, fixed dock areas, or OBS versions may react differently; test them before use. A dock must have a unique stable object name and allow the workspace's dock area. A duplicate/unnamed or temporarily missing dock is not moved. Renaming or removing a third-party or OBS custom browser dock may change its identity. Use **Refresh** after adding docks.
- **Focus** and **Grid** alter OBS's real layout, not a private copy. Grid pages hide other included docks. The backup is a full Qt/OBS window-state snapshot, not an export of scenes or sources, and it may not restore docks that no longer exist. It is not a replacement for your normal OBS configuration backup.
- Restore the snapshot **before uninstalling** if you want the earlier layout back. Closing OBS or uninstalling does not automatically restore it; OBS normally saves the current workspace layout on exit.
- Web pages have network access, can use cookies and may open popups. Only add sites you trust. DockBloom stores addresses—including query-string tokens—in plain text in `%APPDATA%\obs-studio\plugin_config\obs-dockbloom\workspace.json` (portable OBS uses its portable config folder). Do not put passwords or secrets in URLs. Removing a web dock or uninstalling DockBloom does **not** clear browser cookies or remove saved settings. OBS Browser manages browser data separately.
- The browser bridge is pinned to the OBS 33.0.1 obs-browser panel API; if an incompatible or missing browser component is detected, new DockBloom web docks are unavailable rather than calling an unknown interface. Existing OBS docks can still be used.

## Update or uninstall

Before removing the plugin, use **Restore previous OBS layout** if desired, then close OBS. Run **Uninstall.bat** as administrator to remove only DockBloom's plugin folder. To update, back up that folder, uninstall the old copy, and install the new artifact. Existing settings, saved web addresses and the layout snapshot remain in OBS's `plugin_config\obs-dockbloom` folder; remove that folder manually only if you no longer need them. Browser cookies may remain in OBS Browser's data.

## Development and licenses

The plugin is GPL-2.0-or-later; see [LICENSE](LICENSE). The copied OBS Project browser panel interface and its original license are in `third_party/obs-browser/`. CI builds against OBS 33.0.1 and runs Qt tests with a fake host plus a disposable, automated Windows OBS test. Those tests are not a guarantee for every third-party plugin, authenticated website, hardware configuration, or live production.
