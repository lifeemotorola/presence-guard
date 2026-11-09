# Export Kit

**One design, every size, one click.**

Photoshop's own export presets are per-size and per-save. This is the other way
round: tick the sizes you publish at, press once, and get a folder of files named
`my-design-yt-thumb-1280x720.jpg`, `my-design-ig-story-1080x1920.jpg`, and so on.

| | |
|---|---|
| Photoshop | **23.3 or newer** (2022+), 24+ recommended |
| Kind | UXP plugin — HTML + JavaScript, not a C++ SDK plugin |
| Needs | the free **UXP Developer Tool** for development loading |
| Sizes | 10 built in, 4 ticked by default |
| Tests | `npm test` — includes simulated UXP/host checks, no Photoshop needed |

## What it exports

| Slot | Size | How |
|---|---|---|
| YouTube thumbnail | 1280×720 | fill — cropped |
| YouTube end card | 1920×1080 | fit — padded |
| Instagram post | 1080×1080 | fill — cropped |
| Story, Shorts, TikTok | 1080×1920 | fill — cropped |
| X post | 1600×900 | fill — cropped |
| YouTube banner | 2560×1440 | fill — cropped |
| Twitch banner | 1200×480 | fill — cropped |
| X header | 1500×500 | fill — cropped |
| Twitch panel | 320×100 | fit — padded (PNG) |
| Discord emoji | 128×128 | fit — padded (PNG) |

**Fill** covers the slot and crops what does not fit — right for photos and
thumbnails. **Fit** keeps the whole picture and pads the leftover space — right
for logos, panels and anything with a transparent edge (those export as PNG).

## The smart crop

Cropping a 16:9 screenshot into a 1080×1920 story is where faces get cut in half.
With **Find the subject when cropping** ticked, the plugin reads the document at
about 96 pixels across, scores every cell for detail and colour, and slides the
crop window to wherever the picture is busiest — then reports whether it was
actually sure (`detail crop` or `centered crop` in the results). A flat sky, a
gradient or a studio backdrop scores near zero everywhere, so the crop stays centred instead of wandering off to a corner
because of noise.

No machine learning or face detection is involved: this is a detail-and-colour
map, and it may choose a busy background instead of the intended subject.

## Everything else it does right

- **The source document is not edited.** Each slot works on a temporary duplicate
  and attempts to close it without saving; Photoshop is asked to auto-close it on
  cancellation. Check for a leftover duplicate if Photoshop reports an error.
- **One slot failing does not stop the export.** You get "7 exported, 1 failed"
  and the reason, not silence.
- **It warns before it lies.** A 500×500 source blown up to 1080×1920 is called
  out in the panel as a 3.8× enlargement, before you press the button.
- **The folder is remembered** between sessions.
- **JPEG quality, sharpening and the smart crop** are three settings, not thirty.
  Sharpening is attempted on the active layer of each duplicate, not necessarily
  the whole composite; the results say `sharpen skipped` if Photoshop refuses.
- **Existing files with the same output names are overwritten without prompting.**
  Use an empty destination folder when testing.

## Install / update

This is a **development plugin folder**, not a double-click installer or a
`.ccx`. You do not need Node or a compiler to use it.

1. Download `photoshop-export-kit-plugin` from **Actions → Export Kit → a
   successful run → Artifacts**. Extract the ZIP to a local folder, such as
   Documents. Keep `manifest.json`, `index.js`, `src/` and `icons/` together.
2. In Photoshop enable **Edit → Preferences → Plug-ins → Enable Developer
   Mode** (Mac: **Photoshop → Preferences → Plug-ins**), then restart Photoshop.
3. Open Photoshop, then install/open **UXP Developer Tool** from Creative Cloud
   Desktop and check that the intended Photoshop is connected.
4. If an old Export Kit entry is present, **Unload** and remove it from the
   Developer Tool, so it cannot keep pointing at the old folder.
5. Click **Add Plugin**, select the **new extracted** `manifest.json`, then
   **Load**. Open **Plugins → Export Kit** in Photoshop. You may need to load
   it again after restarting Photoshop.
6. Test with a copy of a document and an empty destination folder first. Existing
   files with the same output names are overwritten.

You can instead add `photoshop-export-kit/manifest.json` from a current checkout.
If loading fails, click **Details** in the Developer Tool and share the exact
error or a screenshot. For a timeout, try the extracted folder in local Documents.
See the included `INSTALL.txt`. Packaging/distributing a `.ccx` is a separate
Adobe Developer Tool / Developer Console process, not part of this artifact.

## Reading the results

Each row shows a filename and available details, or a failure reason.

| What you might see | What it means |
|---|---|
| `my-design-ig-story-1080x1920.jpg · 412 KB` | done |
| `will be enlarged 3.8× from this document` | the source is smaller than the slot; it will look soft |
| `1 exported, 3 failed` | the failures each say why; usually the folder permission |
| Nothing at all, again | the document has no size — is something selected but hidden? no: usually no document is open |

## Honest limits

- **Rotated or transformed layer content is exported as it looks.** Hidden layers
  remain hidden in the export.
- **Smart objects are rasterised by the export**, exactly as Photoshop's own
  export does.
- **Fit slots expand the canvas.** Padding may be transparent when the document
  has no locked Background layer, or use Photoshop's background colour when it
  does. JPEG does not support transparency. Use *fill* to crop instead.
- **The smart crop is a heuristic.** It finds busy regions, not faces. With a
  subject on a busy background, trust your eyes over the panel.
- **The panel needs Photoshop 23.3+** because that is where UXP panels with
  manifest v5 start. On older versions it will refuse to load, by design.

## Build and test

There is nothing to build: the plugin is the folder. Run the automated tests:

```bash
cd photoshop-export-kit
npm test
```

The arithmetic tests cover every slot's
geometry for 16:9, 4:3, square and vertical documents, the rounding that must
never leave a one-pixel seam, blow-up detection, file naming, the crop moving
inside its slack and never outside it, the detail map, the centred fallback for
a featureless picture, and the confidence that decides whether to be clever at
all.

## Layout

```
photoshop-export-kit/
├─ manifest.json        the plugin's identity and entry points
├─ index.html           the panel
├─ index.js             plugin-root entry script
├─ INSTALL.txt          install and troubleshooting steps
├─ styles.css
├─ icons/
├─ src/
│ ├─ core.js            slots, geometry, file names (pure, tested)
│ ├─ smartcrop.js       detail map and crop chooser (pure, tested)
│ ├─ photoshop.js       the only file that talks to Photoshop
│ └─ panel.js           the widgets
└─ tests/               node --test suites
```

The tests include a simulated UXP loader, lifecycle and fake Photoshop adapter;
the assembled artifact is tested the same way in Actions. They **do not** run
inside Photoshop, so a real-host load and export test is still required.

Copyright © 2026 Emmanuel Suah. GPL-2.0-or-later; see the repository LICENSE.
