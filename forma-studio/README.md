# Forma Studio 0.1

Forma Studio is an **early, offline Windows 3D scene editor** by Emmanuel Suah. This is the first stage of a larger, easier-to-use creation suite—not a replacement for Blender. It lets you assemble primitive-based scenes, style them, make non-destructive arrays, animate object transforms, and export a still or a 3D snapshot. There is **no AI in this version**.

## Install and start

The target is 64-bit Windows 10 or 11 with a graphics driver that supports **WebGL 2**, a mouse, and room for the extracted folder. Performance depends on the graphics hardware and scene complexity. The Windows build is a portable folder, not a single executable or a conventional installer.

1. Download the `forma-studio-windows-x64` ZIP from a successful **Forma Studio** GitHub Actions run in this repository. If it is published on the Creator Toolkit downloads page, you can get the ZIP there instead. Actions artifacts may require GitHub sign-in and expire.
2. Right-click the ZIP → **Extract All**. Put the extracted files in a writable local folder, for example `Documents\Forma Studio`. Do not run the app inside the ZIP.
3. Double-click **FormaStudio.exe**. Keep `resources` and every other packaged file and folder beside the executable. To move the app, move the whole folder.
4. The starter scene opens. Select an object in the list or viewport; add a shape from the left, then use the gizmo and inspector. Choose **File → New scene** for a blank project.
5. **Save project** creates a `.forma` file wherever you choose. Save often. There is no autosave, recovery service, or cloud backup. Reopen with **File → Open project**.

The executable is **unsigned**. Windows may display an unknown-publisher/reputation warning; inspect downloads before deciding whether to run them. Compare the SHA-256 values in `SHA256SUMS.txt` with the matching Actions run. A matching checksum identifies a build; it does **not** establish the publisher's identity or prove the software is safe. Do not turn off system security globally.

## Making a scene

- **Build:** Add rounded cubes, cubes, spheres, cylinders, cones, toruses, planes, and low-poly icospheres. Select, move, rotate, scale, duplicate, hide, delete, and rename objects. Numeric position, rotation in degrees, and scale are in the Object inspector. Undo/redo retains up to 40 scene edits in the current session.
- **Look:** The Material inspector controls one base color, roughness, and metalness per object. Scene controls the background, ambient/key light intensities, and exposure. The fixed studio floor and rim light are viewport helpers, not editable light objects.
- **Repeat:** Enable **Array** in the Object inspector. Choose a centered linear array along the object's local X axis, or a radial array in its local XZ plane. Change count and spacing/radius. Transforming the object also transforms its copies. Arrays remain editable in `.forma`; they are baked as individual meshes in GLB.
- **Navigate:** Drag to orbit, right-drag to pan, and scroll to zoom. The view menu offers perspective-positioned front, top, and right views (these are not orthographic cameras). Grid and wireframe buttons affect the editor view only. Frame selected with **F**.
- **Animate:** Select an object, set a frame, and choose **Add keyframe**. Once an object has a keyframe, transform edits automatically create/update a key at the current frame. Add another key at another frame, scrub or press Play. Position, Euler rotation, and scale interpolate linearly. Delete a key at the current frame using the trash button. The default rate is 24 fps; the end frame is editable. The current frame and view toggles are not saved as project settings; projects reopen at frame 0.
- **Export:** **3D model** exports a binary glTF (`.glb`) snapshot of **visible objects at the current frame**, including basic materials and baked array copies. It does not include animation, the camera, lights, grid, or studio floor. **Viewport image** saves a PNG of the realtime view, with background, fixed floor, lighting and shadows but without the grid or selection gizmos. The image size follows the viewport; this is not an offline or ray-traced renderer.

Shortcuts when not typing: **Q** select, **G** move, **R** rotate, **S** scale, **F** frame selected, **Space** play/pause, **Ctrl+D** duplicate, **Delete** delete, **Ctrl+Z** undo, **Ctrl+Shift+Z** or **Ctrl+Y** redo. **Ctrl+S**, **Ctrl+Shift+S**, **Ctrl+O**, and **Ctrl+N** work from property fields too. The `?` button in the app shows the quick reference.

## Important limits

- This is object-level primitive assembly, not polygon/vertex/sculpt editing. No UVs, textures, imported models, custom mesh topology, modifiers beyond the simple array, rigging, particles, physics, video editor, compositing, or ray tracing are implemented.
- There is no AI assistant, generative model, prompt interface, or automatic retopology. Any such capabilities are future work and are not promised by the current build.
- `.forma` is Forma Studio's versioned JSON project format, **not** `.blend`. Forma cannot open Blender files or import GLB/other 3D formats. GLB export is a one-way current-frame interchange; keep the `.forma` project to retain editable arrays and animation.
- Up to 120 objects, 20 array copies per object, 300 transform keys per object, and 600 end frames. Dense scenes can be slow well before those limits. Project files must be at most 10 MiB; exports are limited to 100 MiB in the desktop app.
- No autosave or cloud sync. On New, Open, or Close, unsaved scene changes prompt you to save or discard; a crash or power loss can still lose unsaved work. Project files are written through a temporary file in the selected directory; an I/O error can prevent saving. Keep independent backups of important work.
- On a machine without working WebGL 2, the project controls can appear but the 3D view and exports cannot run. Update the graphics driver or use a supported GPU. The automated software-rendering tests do not replace testing on your hardware.

## Privacy

No account is needed. The app does not intentionally make network requests, and its renderer is configured to deny network access and permissions. Projects and exports are saved locally to locations you choose; Windows and Electron may retain normal file-dialog, cache, or crash information in the user profile. The app does not upload, sync, or back up scenes.

## Roadmap, not current features

Further stages could add real editable meshes, richer materials and lights, import/export, more procedural operators, advanced animation, rendering, and eventually AI assistance. Those are design directions only, not commitments that they already exist or a claim of Blender feature parity.

## Build and test from source

On Windows with Node.js 22 and Git installed:

```powershell
cd forma-studio
npm ci --no-audit --no-fund
npm run start                 # build renderer and launch Electron
npm test                      # project model tests
npm run package:win           # creates out/FormaStudio-win32-x64
npm run test:desktop          # exercises the packaged Windows executable
```

`npm run build` bundles the local renderer. Browser tests require a compatible local Chrome/Chromium: set `CHROMIUM_PATH` to its executable, then run `npm run test:browser`. The browser preview uses downloads/file picking rather than the native desktop dialogs and is not a substitute for the Windows build. The packaged-desktop test uses software WebGL and stubs only the native dialog selections; it exercises the actual Electron bridge and local file I/O. `.github/workflows/forma-studio.yml` runs these tests and produces an unsigned Windows x64 artifact.

Forma Studio's source is GPL-2.0-or-later; see `LICENSE`. The distribution includes the app source under `SOURCE/`, the Three.js MIT license in `THREE-LICENSE.txt`, font licenses under `SOURCE/app/assets/fonts/`, and Electron/Chromium notices in the packaged folder. Three.js and build dependencies are pinned in `package-lock.json`.
