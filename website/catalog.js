// The website and the release publisher both use this catalog. Keep the
// workflow/artifact names in sync with the corresponding GitHub Actions files.
export const REPOSITORY = 'lifeemotorola/presence-guard';
export const GITHUB = `https://github.com/${REPOSITORY}`;
export const DOWNLOAD_TAG = 'creator-downloads';

const obs33 = 'Windows x64 · Built against OBS Studio 33.0.1';
const batInstall = (name) => [
  'Extract the downloaded ZIP. Keep the included files and folders together.',
  'Close OBS. Right-click Install.bat and choose Run as administrator. Review the package before approving the installer.',
  `Restart OBS and open Docks → ${name}.`,
];
const psInstall = (name, where = `Docks → ${name}`) => [
  'Extract the downloaded ZIP. If it contains another ZIP, extract that too.',
  'Close OBS. In the extracted folder, run: powershell -ExecutionPolicy Bypass -File .\\Install.ps1',
  `Restart OBS and open ${where}.`,
];
const uxpInstall = (name) => [
  'Extract the ZIP to a local folder. Keep manifest.json, index.js, src and icons together.',
  'In Photoshop, enable Developer Mode in Preferences → Plug-ins and restart Photoshop.',
  `Open the UXP Developer Tool, choose Add Plugin, select the extracted manifest.json and click Load. In Photoshop, open Plugins → ${name}.`,
];

export const PRODUCTS = [
  {
    id: 'dockbloom', name: 'DockBloom', version: '0.1.0', category: 'obs',
    eyebrow: 'YOUR WORKSPACE, REIMAGINED', icon: 'grid', color: 'lilac',
    description: 'Bring your favorite OBS, plugin, and web docks together in one beautifully organized space.',
    detail: 'Switch between a focused single dock and a paged grid of up to four. Add trusted web docks and restore your previous OBS layout when you need it.',
    compatibility: obs33,
    note: 'Requires OBS Browser for new web docks. Not every third-party dock or website is guaranteed compatible. Test your workspace before going live.',
    steps: batInstall('DockBloom'),
    workflow: 'dockbloom-windows.yml', artifact: 'obs-dockbloom-windows-x64', asset: 'dockbloom-windows-x64.zip',
    fallbackRun: 44787973114, fallbackExpires: '2027-02-18T17:56:01Z', guide: 'dockbloom/README.md',
    keywords: 'workspace browser tabs grid focus organize stream',
  },
  {
    id: 'mixbloom', name: 'MixBloom', version: '0.2.0', category: 'obs',
    eyebrow: 'SOUND, SIMPLIFIED', icon: 'sliders', color: 'mint',
    description: 'A calmer way to mix, monitor, and route your OBS audio sources—all in one dock.',
    detail: 'Control source levels, mute, monitoring, and tracks 1–6 from a compact mixer. No iRig hardware is required.',
    compatibility: obs33,
    note: 'Changes affect actual OBS audio immediately, including live streams and recordings. Rehearse with a private recording first.',
    steps: batInstall('MixBloom'),
    workflow: 'mixbloom-windows.yml', artifact: 'obs-mixbloom-windows-x64', asset: 'mixbloom-windows-x64.zip',
    fallbackRun: 44693615973, fallbackExpires: '2027-02-18T07:15:24Z', guide: 'mixbloom/README.md',
    keywords: 'audio mixer routing tracks sound music mic microphone',
  },
  {
    id: 'keybloom', name: 'KeyBloom', version: '1.3.0', category: 'apps',
    eyebrow: 'A LITTLE BETTER EVERY DAY', icon: 'keyboard', color: 'peach',
    description: 'Build confidence at the keyboard with friendly lessons, stories, and typing practice.',
    detail: 'An offline typing companion with guided lessons, tests, stories, and progress tracking. It uses an English / US QWERTY layout.',
    compatibility: 'Windows 10 or 11 x64 · Physical keyboard',
    note: 'This is a portable folder, not a single-file app. The executable is unsigned; a checksum does not establish publisher identity.',
    steps: [
      'Extract the ZIP to a local folder such as Documents\\KeyBloom.',
      'Open that folder and double-click KeyBloom.exe. Keep all the other files and folders beside it.',
      'Start a lesson or explore a story. Your progress is stored locally.',
    ],
    workflow: 'keybloom.yml', artifact: 'keybloom-windows-x64', asset: 'keybloom-windows-x64.zip',
    fallbackRun: 44479285796, fallbackExpires: '2027-02-17T07:23:51Z', guide: 'keybloom/README.md',
    keywords: 'type typing tutor lessons education keyboard practice',
  },
  {
    id: 'screenforge', name: 'ScreenForge', version: '1.1.0', category: 'apps',
    eyebrow: 'PRESS RECORD AND GO', icon: 'record', color: 'sky',
    description: 'Record your screen, your voice, and the moments worth keeping in a simple Windows app.',
    detail: 'Capture a display, window, or region with microphone and system audio. Export to an MP4 without an account.',
    compatibility: 'Windows 11 x64 · ffmpeg required separately',
    note: 'The download does not include ffmpeg. Place ffmpeg.exe beside ScreenForge.exe before recording. The executable is unsigned.',
    steps: [
      'Extract the ZIP. If it contains another ZIP, extract that too.',
      'Get a trusted Windows build of ffmpeg and place ffmpeg.exe next to ScreenForge.exe. See the full guide for options.',
      'Open ScreenForge.exe, select what to capture, and make a test recording.',
    ],
    workflow: 'screenforge.yml', artifact: 'screenforge-windows-x64', asset: 'screenforge-windows-x64.zip',
    fallbackRun: 42500573205, fallbackExpires: '2027-02-06T16:34:22Z', guide: 'screen-forge/README.md',
    keywords: 'screen recorder video capture desktop windows ffmpeg',
  },
  {
    id: 'onair-twin', name: 'OnAirTwin', version: '0.1.0', category: 'obs',
    eyebrow: 'REHEARSE WITH CONFIDENCE', icon: 'shield', color: 'butter',
    description: 'Rehearse what happens when a source fails, without changing your live broadcast.',
    detail: 'Scan an OBS setup, save a baseline, and simulate a source failure to see which scenes reference a source or your chosen backup.',
    compatibility: obs33,
    note: 'Early beta and read-only: it does not perform failover or prove that video and audio are working. Test your real setup separately.',
    steps: batInstall('OnAirTwin'),
    workflow: 'onair-twin-windows.yml', artifact: 'obs-onair-twin-windows-x64', asset: 'onair-twin-windows-x64.zip',
    fallbackRun: 42581872697, fallbackExpires: '2027-02-07T07:50:43Z', guide: 'onair-twin/README.md',
    keywords: 'backup broadcast safety rehearsal simulation source scene',
  },
  {
    id: 'local-ai-auto-frame', name: 'Local AI Auto Frame', version: '0.1.0', category: 'obs',
    eyebrow: 'STAY IN THE FRAME', icon: 'frame', color: 'rose',
    description: 'Keep your camera centered on the action with face detection that runs locally.',
    detail: 'A camera filter that detects faces and smoothly crops toward the largest one. The model and CPU runtime are included; no cloud or API key is needed.',
    compatibility: obs33,
    note: 'The included OpenCV build uses the CPU. CUDA needs a separately built CUDA-enabled OpenCV. Use it on a compatible camera/video capture source.',
    steps: psInstall('Local AI Auto Frame', 'a camera source → Filters → + → Local AI Auto Frame'),
    workflow: 'local-ai-windows.yml', artifact: 'obs-local-ai-windows-x64', asset: 'local-ai-auto-frame-windows-x64.zip',
    fallbackRun: 41508826598, fallbackExpires: '2027-01-31T19:19:14Z', guide: 'local-ai-auto-frame/README.md',
    keywords: 'camera face webcam ai auto framing crop zoom filter',
  },
  {
    id: 'auto-director', name: 'Auto Director', version: '1.0.0', category: 'obs',
    eyebrow: 'YOUR SECOND SET OF EYES', icon: 'director', color: 'lilac',
    description: 'Let your scenes follow the conversation with intelligent speaker-based camera cuts.',
    detail: 'Associate each camera scene with a microphone and let the dock suggest or make cuts. Dry run and operator hold help you rehearse first.',
    compatibility: obs33,
    note: 'Automatic cuts affect the active OBS scene. Set up the microphones and use Dry run before enabling live direction.',
    steps: psInstall('Auto Director'),
    workflow: 'auto-director-windows.yml', artifact: 'obs-auto-director-windows-x64', asset: 'auto-director-windows-x64.zip',
    fallbackRun: 41918715254, fallbackExpires: '2027-02-02T21:43:20Z', guide: 'auto-director/README.md',
    keywords: 'camera cuts switching scene speakers microphone director',
  },
  {
    id: 'auto-scene-adjust', name: 'Auto Scene Adjust', version: '1.0.0', category: 'obs',
    eyebrow: 'EVERYTHING IN ITS PLACE', icon: 'layout', color: 'mint',
    description: 'Find misplaced sources and give your scenes a fresh layout in just a few clicks.',
    detail: 'Find sources that are off canvas, tiny, or stretched; fit or fill them and try grid, side-by-side, and main-plus-camera presets.',
    compatibility: obs33,
    note: 'The plugin changes ordinary OBS transforms for the sources you choose. Use Undo and review the result before going live.',
    steps: psInstall('Auto Scene Adjust'),
    workflow: 'auto-scene-windows.yml', artifact: 'obs-auto-scene-windows-x64', asset: 'auto-scene-adjust-windows-x64.zip',
    fallbackRun: 42014343045, fallbackExpires: '2027-02-03T10:52:49Z', guide: 'auto-scene/README.md',
    keywords: 'layout scenes sources fit fix fill crop grid presets',
  },
  {
    id: 'hype-clip', name: 'Hype Clip', version: '1.0.0', category: 'obs',
    eyebrow: 'KEEP THE GOOD PARTS', icon: 'scissors', color: 'peach',
    description: 'Catch your biggest moments automatically by listening for peaks in the action.',
    detail: 'Watch audio for sustained, standout peaks and ask OBS to save the replay buffer when a highlight happens.',
    compatibility: obs33,
    note: 'Requires the OBS replay buffer to be enabled and running. Tune the detection thresholds for your own show.',
    steps: psInstall('Hype Clip'),
    workflow: 'hype-clip-windows.yml', artifact: 'obs-hype-clip-windows-x64', asset: 'hype-clip-windows-x64.zip',
    fallbackRun: 41935526179, fallbackExpires: '2027-02-02T23:40:09Z', guide: 'hype-clip/README.md',
    keywords: 'clip replay buffer highlights audio stream moments',
  },
  {
    id: 'obs-beautifier', name: 'OBS Beautifier', version: '1.0.0', category: 'obs',
    eyebrow: 'MAKE OBS FEEL LIKE YOU', icon: 'sparkles', color: 'sky',
    description: 'Make your OBS workspace your own with thoughtful colors, spacing, and style.',
    detail: 'Adjust accent colors, corners, density, and font size on top of your current OBS theme. Revert restores the previous style.',
    compatibility: obs33,
    note: 'Theme and plugin combinations can differ. Revert the styling if it does not suit your setup.',
    steps: psInstall('OBS Beautifier'),
    workflow: 'obs-beautifier-windows.yml', artifact: 'obs-beautifier-windows-x64', asset: 'obs-beautifier-windows-x64.zip',
    fallbackRun: 41925098843, fallbackExpires: '2027-02-02T22:25:08Z', guide: 'obs-beautifier/README.md',
    keywords: 'theme style ui color design appearance customize',
  },
  {
    id: 'presence-guard', name: 'Presence Guard', version: '0.1.0', category: 'obs',
    eyebrow: 'A LITTLE PEACE OF MIND', icon: 'presence', color: 'butter',
    description: 'Keep your stream looking cared for when you step away or the mic goes quiet.',
    detail: 'Set a safe scene for keyboard/mouse inactivity or dead air, and let Presence Guard return to your previous scene when you are back.',
    compatibility: 'Windows 10/11 x64 · Built against OBS Studio 31.1.1',
    note: 'Compatibility with OBS 33 has not been verified. The dead-air rule needs an active microphone; it does not guess when audio data is missing.',
    steps: [
      'Extract the downloaded artifact ZIP, then extract the inner presence-guard Windows ZIP.',
      'Close OBS. Move the entire presence-guard folder into C:\\ProgramData\\obs-studio\\plugins, keeping bin and data together.',
      'Restart OBS and open Docks → Presence Guard.',
    ],
    workflow: 'push.yaml', artifactPattern: '^presence-guard-.+-windows-x64-[a-f0-9]+$', artifactLabel: 'presence-guard-<version>-windows-x64-<commit>', asset: 'presence-guard-windows-x64.zip',
    fallbackRun: 44787973206, fallbackExpires: '2027-02-18T17:56:01Z', guide: 'docs/INSTALL.md',
    keywords: 'afk brb idle away dead air silence microphone safe scene',
  },
  {
    id: 'face-retouch', name: 'Face Retouch', version: '1.0.1', category: 'photoshop',
    eyebrow: 'A GENTLER TOUCH', icon: 'portrait', color: 'rose',
    description: 'Give portraits a polished finish with an editable stack of retouching layers.',
    detail: 'A Photoshop UXP panel that builds editable layers for tone, texture, warmth, and light. It does not automatically detect faces or create a mask.',
    compatibility: 'Photoshop 2023 / 24.0+ · UXP Developer Tool',
    note: 'Development-loaded UXP panel, not a .ccx installer. Tests simulate Photoshop operations; real Photoshop host testing is still needed.',
    steps: uxpInstall('Face Retouch'),
    workflow: 'face-retouch.yml', artifact: 'photoshop-face-retouch-plugin', asset: 'face-retouch-photoshop-plugin.zip',
    fallbackRun: 42587645942, fallbackExpires: '2027-02-07T08:40:21Z', guide: 'photoshop-face-retouch/README.md',
    keywords: 'photoshop portrait photo skin retouch beauty uxp editing',
  },
  {
    id: 'export-kit', name: 'Export Kit', version: '1.0.1', category: 'photoshop',
    eyebrow: 'ONE DESIGN, EVERY SIZE', icon: 'export', color: 'lilac',
    description: 'Turn one Photoshop design into the sizes you need for all your favorite platforms.',
    detail: 'Export multiple preset sizes from a design, with fit or fill options and an optional detail-based crop. The original document is not edited.',
    compatibility: 'Photoshop 23.3+ (24+ recommended) · UXP Developer Tool',
    note: 'Development-loaded UXP panel, not a .ccx installer. Host testing is still needed. Existing files with matching names are overwritten.',
    steps: uxpInstall('Export Kit'),
    workflow: 'photoshop-export-kit.yml', artifact: 'photoshop-export-kit-plugin', asset: 'export-kit-photoshop-plugin.zip',
    fallbackRun: 42587645920, fallbackExpires: '2027-02-07T08:40:21Z', guide: 'photoshop-export-kit/README.md',
    keywords: 'photoshop export resize social media thumbnails design uxp',
  },
];

export const CATEGORIES = [
  { id: 'all', label: 'All tools' },
  { id: 'obs', label: 'OBS plugins' },
  { id: 'apps', label: 'Windows apps' },
  { id: 'photoshop', label: 'Photoshop' },
];

export function workflowUrl(product) {
  return `${GITHUB}/actions/workflows/${product.workflow}`;
}

export function fallbackUrl(product, now = Date.now()) {
  return product.fallbackRun && Date.parse(product.fallbackExpires) > now
    ? `${GITHUB}/actions/runs/${product.fallbackRun}`
    : workflowUrl(product);
}

export function artifactName(product) {
  return product.artifact || product.artifactLabel;
}

export function releaseAssetUrl(product) {
  return `${GITHUB}/releases/download/${DOWNLOAD_TAG}/${product.asset}`;
}
