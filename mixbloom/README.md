# MixBloom for OBS Studio

A calm, compact **audio mixer and track-routing dock** for the audio sources
already in OBS. Created by **Emmanuel Suah**. This is an independent project;
it is not an iRig product and does not require an iRig device or special driver.

**Live audio warning:** fader, mute, track and monitoring changes take effect in
OBS immediately and may affect a stream or recording already in progress. The
plugin makes no audio changes simply by opening. Test with a private recording
first; uninstalling the plugin does **not** undo changes made to OBS sources.

Built for **Windows x64 / OBS Studio 33.0.1**. Other OBS versions and operating
systems are not verified. A physical microphone or instrument still needs an
available input/interface supported by OBS; software cannot capture a device
that is not connected or otherwise available to OBS.

## Download and install — no compiler needed

1. In this repository on GitHub, open **Actions → MixBloom**.
2. Open a **successful** run. Scroll to the bottom, under **Artifacts**, and
   download **`obs-mixbloom-windows-x64`**. Extract the downloaded ZIP; the
   extracted files include `Install.bat`, `bin`, and `data`.
3. **Close OBS.** Right-click `Install.bat` and choose **Run as administrator**.
   If an older `obs-mixbloom` folder is already installed, back it up and remove
   it first; the installer deliberately refuses to overwrite it.
4. Start OBS → **Docks → MixBloom**. If it does not appear, check **Help → Log
   Files → View Current Log** for `[obs-mixbloom] loaded`.

The default OBS 33 installation is:

```text
C:\ProgramData\obs-studio\plugins\obs-mixbloom\obs-mixbloom.dll
C:\ProgramData\obs-studio\plugins\obs-mixbloom\data\locale\en-US.ini
```

For a **portable OBS 33** copy, close OBS and run the extracted PowerShell
installer with its `-PluginRoot` set to the `plugins` folder inside that
portable OBS root (the folder containing `bin`). For example, from PowerShell:

```powershell
.\Install.ps1 -PluginRoot 'D:\MyPortableOBS\plugins'
```

The artifact includes `SHA256SUMS.txt`, `LICENSE`, and `SOURCE-CODE.txt` for the
corresponding source. The DLL and installer are **unsigned**; a matching hash
only checks consistency with the artifact, not publisher identity. To uninstall,
close OBS and run `Uninstall.bat` as administrator (or use `Uninstall.ps1
-PluginRoot ...` for portable OBS). Existing OBS audio changes remain in OBS.

## Use the dock

1. Configure a **Mic/Aux**, **Desktop Audio**, or another audio-producing source
   in OBS. MixBloom lists public audio-capable inputs in the current scene
   collection, including sources that might not be active in the current scene.
   It refreshes automatically; **Refresh** also reads OBS immediately. It does
   not create or select a physical input for you.
2. Adjust a source's **Level** or use **Mute / Unmute**. These are the source's
   actual OBS controls, not a separate mixer; mute affects all its tracks. The
   fader ranges from silence to **0 dB** using a cubic curve. If the source was
   boosted above 0 dB elsewhere in OBS, its actual value is displayed and left
   alone until you move this fader, which caps it at 0 dB. A level locked in the
   OBS mixer stays locked here.
3. Toggle **output tracks 1–6** for each source. The selected tracks belong to
   the source across OBS scenes, not just the visible scene. MixBloom asks before
   removing the last selected track. You must also select the appropriate
   streaming/recording tracks in **OBS Settings → Output**. A selected button
   alone does not guarantee your audience can hear the source.
4. **Monitor: Off / On** controls OBS monitoring to its selected monitoring
   device *in addition to* the source's selected output tracks. Enabling it asks
   for confirmation: use headphones and check the monitoring device to avoid
   feedback. Sources OBS marks unsafe for self-monitoring, commonly desktop
   output capture, cannot be enabled here; existing monitoring can be turned
   off. Monitoring-only routing is not offered.
5. The small **source peak** meter shows the loudest channel supplied by the OBS
   source meter after the fader. It can move even when muted and it is not a
   measurement of the final stream, recording, loudness, or true peak. No meter
   movement can mean the source is inactive or not sending audio; it is not a
   diagnosis. Listen to and verify the final output separately.

Source names and controls update when OBS changes. Actions target the source's
OBS UUID, so similarly named sources remain separate; controls pause during a
scene-collection switch. MixBloom reads the OBS fader lock and current track
mask again before applying changes, and preserves other track-mask bits. There
is no solo, automatic mixing, virtual cable, effects processing, separate mix
bus, preset/undo system, or independent device capture in this release.

## Troubleshooting

- **No sources:** add or enable a suitable audio source in OBS Settings → Audio
  or the Sources list and check that the correct scene collection is open.
- **No sound in the recording/stream:** check that the source is active and
  unmuted in OBS, its desired track is selected both here and in OBS output
  settings, the input device works, and filters/routing are correct. Make a
  private recording and listen back; the dock meter is not proof of output.
- **Feedback:** turn monitoring off, use headphones, and check that desktop
  capture does not recapture the monitoring device. OBS device configurations
  can change independently; the warning cannot detect every feedback path.
- **Dock missing:** check the OBS log and installation path, verify the DLL and
  data came from the same artifact, and restart OBS. This build targets OBS
  33.0.1 and is not a compatibility promise for older versions.

MixBloom does not send audio, source names or analytics over the network and
has no account. Its meter callback reads values provided by OBS without storing
or recording audio. Source settings and any changes you make remain managed by
OBS.

## Tests and source

`./tests/run-tests.sh` exercises the C++ mixer model and real adapter against a
fake OBS implementation. The Windows workflow builds the actual Qt dock and
exercises it against fake OBS, tests the portable install/uninstall and
overwrite protection, then starts the packaged DLL in a disposable real OBS
33.0.1 portable installation. The latter is **only a load smoke test**:
it does not prove operation with a real microphone, monitoring device, stream,
recording, listener or every OBS setup. Build instructions for developers use
`cmake --preset windows-x64` and `cmake --build --preset windows-x64 --config
RelWithDebInfo`, with Visual Studio 2022 and CMake 3.28+.

GPL-2.0-or-later; see `LICENSE`. No warranty.
