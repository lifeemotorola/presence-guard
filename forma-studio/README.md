# Forma Studio 0.3

Forma Studio is an **early, offline Windows 3D scene editor** by Emmanuel Suah. This is one stage of a larger, easier-to-use creation suite—not a replacement for Blender. Assemble shapes, edit basic mesh geometry, style a scene, make non-destructive arrays, animate object transforms, and export a still or a 3D snapshot. There is **no AI in this version**. This document describes the 0.3 source; a published rolling download may still contain an earlier version.

## Install and start

The target is 64-bit Windows 10 or 11 with a graphics driver that supports **WebGL 2**, a mouse, and room for the extracted folder. Performance depends on the graphics hardware and scene complexity. The Windows build is a portable folder, not a single executable or a conventional installer.

1. Download the `forma-studio-windows-x64` ZIP from a successful **Forma Studio** GitHub Actions run in this repository. If it is published on the Creator Toolkit downloads page, you can get the ZIP there instead. Actions artifacts may require GitHub sign-in and expire. Check the included `INSTALL.txt` or the app's footer for the build version; a rolling download can lag behind the source.
2. Right-click the ZIP → **Extract All**. Put the extracted files in a writable local folder, for example `Documents\Forma Studio`. Do not run the app inside the ZIP.
3. Double-click **FormaStudio.exe**. Keep `resources` and every other packaged file and folder beside the executable. To move the app, move the whole folder.
4. The starter scene opens. Select an object in the list or viewport; add a shape from the left, then use the gizmo and inspector. Choose **File → New scene** for a blank project.
5. **Save project** creates a `.forma` file wherever you choose. Save often. There is no autosave, recovery service, or cloud backup. Reopen with **File → Open project**.

The executable is **unsigned**. Windows may display an unknown-publisher/reputation warning; inspect downloads before deciding whether to run them. Compare the SHA-256 values in `SHA256SUMS.txt` with the matching Actions run. A matching checksum identifies a build; it does **not** establish the publisher's identity or prove the software is safe. Do not turn off system security globally.

## Making a scene

- **Build:** Add rounded cubes, cubes, spheres, cylinders, cones, toruses, planes, and low-poly icospheres. Select, move, rotate, scale, duplicate, hide, delete, and rename objects. Numeric position, rotation in degrees, and scale are in the Object inspector. Undo/redo retains up to 40 scene edits in the current session.
- **Model:** Select a visible shape, open **Model** in the workspace or inspector, and choose **Edit mesh**. This converts the primitive into editable geometry as an undoable scene change; its transform, material, animation and array settings remain. Cubes and planes start with quad faces; curved shapes are converted to triangles. Converting can change surface shading. Select **Face** or **Vertex**, then click a component in the viewport, or enter its 1-based number in the inspector. One component is selected at a time. Edges and points are shown through the mesh, so a click near overlapping vertices picks the nearest point on screen (and the nearer one if exactly tied). Orbit the view or use the number field when a point is hard to pick.
  - With a face selected, **Extrude face** moves a new face along its local normal and creates side walls. Enter a signed distance with magnitude from 0.001 to 20. **Inset face** creates an inner face and a border using a fraction from 0.01 to 0.9 toward the face centroid; it is not a constant-width inset.
  - With a vertex selected, drag the colored local-axis gizmo or enter its local X, Y and Z coordinates. Invalid changes that would create a degenerate or self-intersecting face are rejected. Meshes can still overlap or intersect elsewhere; the app is not a solid-modeling validator.
  - Turn on **Proportional editing** in the vertex panel to move nearby vertices along with the selected point. Set an influence radius from 0.01 to 1,000 local units and choose **Smooth**, **Linear**, or **Sharp** falloff. The selected point moves fully; points farther away move less, and points on or beyond the radius do not move. Orange points and three rings preview the influence on the selected copy. Both the gizmo and numeric coordinates work; each drag uses distances from the start of that drag and is one undo step. Snapping applies to the selected point, not individually to its neighbors. Distances are Euclidean in object-local space, not along edges: disconnected or obscured vertices within the radius also move. A non-uniform object scale can make the local-space radius look elliptical in the scene. These tool settings are session-only.
  - **Subdivide mesh** splits all faces into connected smaller faces. It is linear subdivision, not a smoothing modifier. **Smooth mesh shading** averages normals across shared vertices; it changes shading, not topology, and can soften edges. It can be toggled back to flat shading.
  - Mesh edits affect the source geometry and therefore every array copy, at every animation frame. Transform keyframes do not animate the mesh itself. Click **Done** (or press **Esc** when not typing) to return to object mode. Undo can reverse mesh operations and the initial conversion.
- **Look:** The Material inspector controls one base color, roughness, and metalness per object. Scene controls the background, ambient/key light intensities, and exposure. The fixed studio floor and rim light are viewport helpers, not editable light objects.
- **Repeat:** Enable **Array** in the Object inspector. Choose a centered linear array along the object's local X axis, or a radial array in its local XZ plane. Change count and spacing/radius. Transforming the object also transforms its copies. Arrays remain editable in `.forma`; they are baked as individual meshes in GLB.
- **Navigate:** Drag to orbit, right-drag to pan, and scroll to zoom. The view menu has front, back, right, left, top and bottom **orthographic** axis views; choose **Angled** for a perspective view. The **ORTHO/PERSP** button switches projection independently, keeping the target's apparent size when the zoom limits permit. The menu shows **Custom** after orbiting away from a preset. Frame the selected object with **F**; in orthographic mode it fits the object to the viewport's aspect ratio. Grid and wireframe affect the editor view only. The magnet button toggles gizmo snapping: object translation 0.5 units, rotation 15°, and scale 0.1; vertex gizmos snap to 0.5 local units. Snapping does not change numeric input, extrude or inset values.
- **Animate:** Select an object, set a frame, and choose **Add keyframe**. Once an object has a keyframe, transform edits automatically create/update a key at the current frame. Add another key at another frame, scrub or press Play. Position, Euler rotation, and scale interpolate linearly. Delete a key at the current frame using the trash button. The default rate is 24 fps; the end frame is editable. The current frame and view toggles are not saved as project settings; projects reopen at frame 0.
- **Export:** **3D model** exports a binary glTF (`.glb`) snapshot of **visible objects at the current frame**, including edited mesh geometry, basic materials and baked array copies. It does not include animation, the camera, lights, grid, or studio floor. **Viewport image** saves a PNG of the realtime view, with background, fixed floor, lighting and shadows but without the grid, selection gizmos or mesh overlays. The image size follows the viewport; this is not an offline or ray-traced renderer.

Shortcuts when not typing: **Q** select, **G** move, **R** rotate, **S** scale, **F** frame selected, **Space** play/pause, **Ctrl+D** duplicate, **Delete** delete, **Ctrl+Z** undo, **Ctrl+Shift+Z** or **Ctrl+Y** redo. **Numpad 1/3/7** chooses front/right/top; hold **Ctrl** for back/left/bottom; **Numpad 5** toggles projection. In mesh edit mode, the regular number-row **1** selects vertex mode, **3** selects face mode, **O** toggles proportional editing in vertex mode, **E** extrudes the selected face, **I** insets it, and **Esc** exits. E and I immediately apply the values in the inspector; they do not start an interactive drag. Object tool and delete shortcuts are disabled in mesh mode. **Ctrl+S**, **Ctrl+Shift+S**, **Ctrl+O**, and **Ctrl+N** work from property fields too. The `?` button in the app shows the quick reference. You can use the view menu and projection button if your keyboard has no number pad.

### Try a simple terrain shape

1. Add a **Plane**, frame it, open **Model → Edit mesh**, and choose **Subdivide mesh** three times. The plane will have 81 vertices and 64 faces.
2. Switch to **Top** view and **Vertex** mode. Select the center point, or enter vertex number **5**. Turn on **Proportional editing**, use a radius of **1.2** and **Smooth** falloff, and set the vertex's local **Y** coordinate to **0.8**. Forma uses Y as the vertical axis.
3. Choose **Angled** view and enable **Smooth mesh shading** to inspect the result. Try different radii and falloffs, or use **Front** view and the green Y gizmo to pull the point. Undo reverts the whole move. This is a basic height-field workflow, not a terrain generator or a sculpting brush.

For more general modeling concepts, the user-supplied [Blender Basics course (2017)](https://archive.org/download/2017BlenderBasics/2017_blender-basics_classroom-tutorial-book.pdf) is an optional third-party reference with its own noncommercial license; it is not bundled with Forma. It teaches Blender 2.7x, not Forma: its interface and instructions differ, and Blender uses Z as up while Forma uses Y.

## Project compatibility

Forma Studio 0.3 and 0.2 both read and write version-2 `.forma` projects. Geometry deformed with proportional editing is stored as ordinary mesh coordinates, so 0.2 can reopen it, but 0.2 has no proportional-editing tool or orthographic projection. Neither the proportional tool settings nor the projection mode is stored in the project. Opening or creating a scene starts in perspective; for an orthographic view, the saved camera position approximates its framing at the target plane within the perspective zoom limits, so the reopened view is not an exact orthographic view.

Forma Studio 0.3 also opens version-1 projects from 0.1 and reads them into version 2. The first desktop save of an opened version-1 project prompts for a new path and suggests a `-v2` filename. Choose a **different** filename to preserve the original; deliberately selecting the original path can still overwrite it. A browser preview downloads a separate `-v2.forma` file instead.

**Forma Studio 0.1 cannot open version-2 projects**, even if the scene has no edited meshes. Keep an independent copy of any version-1 project you may need to open in 0.1. `.forma` is Forma Studio's versioned JSON format, **not** `.blend`; Forma cannot open Blender files or import GLB/other 3D formats. GLB is a one-way current-frame interchange, so keep the `.forma` project to retain editable geometry, arrays and animation.

## Important limits

- Mesh editing is deliberately basic: one face or vertex at a time, no edge selection, multiselect, connected-only proportional falloff, bevel, loop cuts, sculpting or Blender-compatible modifiers. Polygon triangulation supports simple faces; extremely distorted, crossing or degenerate faces are refused. Extrusion/inset and proportional movement can still create intersections with other faces or objects.
- At most 4,096 vertices and 8,192 faces per editable mesh, 24,000 vertices and 48,000 faces in the whole project, and 8 corners per face. Dense primitives may reach a limit on the first subdivision. Up to 120 objects, 20 array copies per object, 300 transform keys per object, and 600 end frames. Dense scenes can be slow well before those limits. Project files must be at most 10 MiB; exports are limited to 100 MiB in the desktop app.
- No UVs, textures, imported models, modifiers beyond the simple array, rigging, particles, physics, video editor, compositing, or ray tracing are implemented. There is no AI assistant, generative model, prompt interface, or automatic retopology.
- No autosave or cloud sync. On New, Open, or Close, unsaved scene changes prompt you to save or discard; a crash or power loss can still lose unsaved work. Project files are written through a temporary file in the selected directory; an I/O error can prevent saving. Keep independent backups of important work.
- On a machine without working WebGL 2, the project controls can appear but the 3D view and exports cannot run. Update the graphics driver or use a supported GPU. The automated software-rendering tests do not replace testing on your hardware.

## Privacy

No account is needed. The app does not intentionally make network requests, and its renderer is configured to deny network access and permissions. Projects and exports are saved locally to locations you choose; Windows and Electron may retain normal file-dialog, cache, or crash information in the user profile. The app does not upload, sync, or back up scenes.

## Roadmap, not current features

Further stages could add more advanced modeling, richer materials and lights, import/export, more procedural operators, advanced animation, rendering, and eventually AI assistance. Those are design directions only, not commitments that they already exist or a claim of Blender feature parity.

## Build and test from source

On Windows with Node.js 22 and Git installed:

```powershell
cd forma-studio
npm ci --no-audit --no-fund
npm run start                 # build renderer and launch Electron
npm test                      # project and mesh model tests
npm run package:win           # creates out/FormaStudio-win32-x64
npm run test:desktop          # exercises the packaged Windows executable
```

`npm run build` bundles the local renderer. Browser tests require a compatible local Chrome/Chromium: set `CHROMIUM_PATH` to its executable, then run `npm run test:browser`. The browser preview uses downloads/file picking rather than the native desktop dialogs and is not a substitute for the Windows build. The packaged-desktop test uses software WebGL and stubs only the native dialog selections; it exercises the actual Electron bridge and local file I/O. `.github/workflows/forma-studio.yml` runs these tests and produces an unsigned Windows x64 artifact.

Forma Studio's source is GPL-2.0-or-later; see `LICENSE` in the source or `FORMA-LICENSE.txt` in the portable app. The distribution includes the app source under `SOURCE/`, the Three.js MIT license in `THREE-LICENSE.txt`, font licenses under `SOURCE/app/assets/fonts/`, and Electron/Chromium notices in the packaged folder. Three.js and build dependencies are pinned in `package-lock.json`.
