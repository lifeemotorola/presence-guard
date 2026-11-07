# Export Kit

**One design, every size, one click.**

Photoshop's own export presets are per-size and per-save. This is the other way
round: tick the sizes you publish at, press once, and get a folder of files named
`my-design-yt-thumb-1280x720.jpg`, `my-design-ig-story-1080x1920.jpg`, and so on.

| | |
|---|---|
| Photoshop | **23.3 or newer** (2022+), 24+ recommended |
| Kind | UXP plugin — HTML + JavaScript, not a C++ SDK plugin |
| Needs | the free **UXP Developer Tool** to load it once |
| Sizes | 10 built in, 4 ticked by default |
| Tests | `npm test` — 28 checks, no Photoshop needed |

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
actually sure. A flat sky, a gradient or a studio backdrop scores near zero
everywhere, so the crop stays centred instead of wandering off to a corner
because of noise.

No machine learning is involved and none is claimed: it is a detail-and-colour
map, and it is what makes the difference between "cropped" and "cropped through
somebody's ear".

## Everything else it does right

- **Your document is never touched.** Each slot works on a throwaway duplicate
  and closes it without saving.
- **One slot failing does not stop the export.** You get "7 exported, 1 failed"
  and the reason, not silence.
- **It warns before it lies.** A 500×500 source blown up to 1080×1920 is called
  out in the panel as a 3.8× enlargement, before you press the button.
- **The folder is remembered** between sessions.
- **JPEG quality, sharpening and the smart crop** are three settings, not thirty.
- **Rewriting the same export is safe**: files are overwritten by name.

## Install it (5 minutes, once)

Photoshop plugins are not double-clicked like OBS ones. This is Adobe's own
developer flow:

1. Install the **UXP Developer Tool** (free) from the Creative Cloud desktop app
   → *Marketplace → search "UXP Developer Tool"*.
2. Get the plugin folder: **Actions → Export Kit → newest green run →
   Artifacts → `photoshop-export-kit-plugin`**, unzip it (it is a zip inside a
   zip), or just use this folder from the repository.
3. Open the UXP Developer Tool → **Add Plugin…** → choose
   `photoshop-export-kit/manifest.json`.
4. Press **Load** next to *Export Kit*.
5. In Photoshop: **Plugins → Export Kit**. The panel opens. Drag it anywhere.
6. Tick the sizes you use, choose a folder, press **Export everything**.

The plugin stays loaded across Photoshop restarts once added in the Developer
Tool; to keep it after uninstalling the tool you would need to package and sign
it as a `.ccx` through Adobe's developer console, which is a separate (free)
process and deliberately not automated here.

## Reading the results

Each row is a file: `✓` and its size, or `✗` and the reason.

| What you might see | What it means |
|---|---|
| `my-design-ig-story-1080x1920.jpg · 412 KB` | done |
| `will be enlarged 3.8× from this document` | the source is smaller than the slot; it will look soft |
| `1 exported, 3 failed` | the failures each say why; usually the folder permission |
| Nothing at all, again | the document has no size — is something selected but hidden? no: usually no document is open |

## Honest limits

- **Rotated or transformed layer content is exported as it looks.** This works on
  the document canvas, so what you see is what you get — hidden layers included.
- **Smart objects are rasterised by the export**, exactly as Photoshop's own
  export does.
- **Fit slots pad with transparency**, which becomes white in a JPEG. If you
  wanted the picture to fill the frame instead, that is what the *fill* slots do.
- **The smart crop is a heuristic.** It finds busy regions, not faces. With a
  subject on a busy background, trust your eyes over the panel.
- **The panel needs Photoshop 23.3+** because that is where UXP panels with
  manifest v5 start. On older versions it will refuse to load, by design.

## Build and test

There is nothing to build: the plugin is the folder. The logic is tested:

```bash
cd photoshop-export-kit
npm test
```

28 checks over the export arithmetic and the crop chooser: every slot's
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
├─ styles.css
├─ icons/
├─ src/
│ ├─ core.js            slots, geometry, file names (pure, tested)
│ ├─ smartcrop.js       detail map and crop chooser (pure, tested)
│ ├─ photoshop.js       the only file that talks to Photoshop
│ └─ panel.js           the widgets
└─ tests/               node --test suites
```
