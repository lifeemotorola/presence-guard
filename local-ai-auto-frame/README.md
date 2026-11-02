# Local AI Auto Frame

An OBS Studio filter that keeps a talking head in frame: it detects faces with
[YuNet](https://github.com/opencv/opencv_zoo/tree/main/models/face_detection_yunet)
through OpenCV's DNN module and crops/zooms the video so the largest face stays
where you want it. Everything runs locally — no cloud service, no API key.

| | |
|---|---|
| OBS source type | video filter (`Local AI Auto Frame`) |
| OBS version | built against **33.0.1** headers |
| Runtime deps | OpenCV 4.10 (`opencv_world4100.dll`, shipped in the package) |
| Model | `data/face_detection_yunet_2023mar.onnx` (included, ~230 KB) |
| Backend | CPU by default; NVIDIA CUDA optional (needs a CUDA-enabled OpenCV) |

## Build it

This plugin lives next to Presence Guard in the same repository but builds
completely independently, from its own `buildspec.json` and `CMakePresets.json`.

**In the cloud (no compiler needed)** — push, then **Actions → Local AI Auto
Frame**. The `local-ai-windows.yml` workflow builds it on a Windows runner,
fetches OpenCV 4.10, and uploads the `obs-local-ai-windows-x64` artifact. Unlike
Presence Guard it is not part of the push/PR release pipeline, so nothing is
published as a release asset.

**Locally, on Windows** — from this folder, in a Visual Studio 2022 prompt:

```powershell
cmake --preset windows-x64 -DOpenCV_DIR=C:\path\to\opencv\build
cmake --build --preset windows-x64 --config RelWithDebInfo
cmake --install build_x64 --config RelWithDebInfo --prefix build_x64\install
```

You need CMake 3.28+ and an OpenCV 4.8+ Windows package (the official
`opencv-4.10.0-windows.exe` self-extractor works).

## Install it

1. Download the `obs-local-ai-windows-x64` artifact from the Actions run and
   extract it.
2. Close OBS, then in the extracted folder:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install.ps1
   ```

   It copies `bin\` and `data\` to
   `%APPDATA%\obs-studio\plugins\obs-local-ai\` and refuses to run while OBS is
   open or if that folder already exists. `Uninstall.ps1` removes it again.
3. Restart OBS and add the filter: right-click a camera source → **Filters** →
   **+** → **Local AI Auto Frame**.

## Settings

| Property | Range | Default | Meaning |
|---|---|---|---|
| Zoom | 1.0 – 3.0 | 1.6 | how far to crop in around the face |
| Follow speed | 0.01 – 1.0 | 0.12 | per-frame smoothing; higher follows faster, lower glides |
| Detection interval | 50 – 1000 ms | 100 | how often a frame is sent to the detector |
| Inference backend | CPU / CUDA | CPU | CUDA needs a CUDA-enabled OpenCV build; on failure it logs a warning and falls back to CPU |

If no face has been seen for two seconds the crop drifts back to the centre of
the frame.

## Limitations

* The filter only transforms **BGRA, BGRX and RGBA** frames; anything else is
  passed through untouched. Check the source's *Video Format* if nothing
  happens.
* Detection runs on a downscaled copy (max 320 px wide) on a worker thread, so
  the render path never blocks — but a slow machine will lag the framing rather
  than the video.
* The bundled OpenCV is the CPU build: the CUDA option is wired up, but selecting
  it without a CUDA-enabled OpenCV just logs a warning and stays on CPU.
* Model licence: see [`data/YUNET-LICENSE.txt`](data/YUNET-LICENSE.txt). OpenCV is
  Apache-2.0 and OBS is GPL-2.0-or-later; review both before redistributing a
  build. This plugin itself is GPL-2.0-or-later, like the rest of the repository.
