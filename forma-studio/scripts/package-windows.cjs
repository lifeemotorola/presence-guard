/* Forma Studio Windows packager. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const path = require('node:path');
const { packager } = require('@electron/packager');
const { stageRelease } = require('./stage-release.cjs');
const { version } = require('../package.json');
(async () => {
  const root = path.resolve(__dirname, '..');
  const output = await packager({
    dir: root, out: path.join(root, 'out'), overwrite: true,
    name: 'FormaStudio', executableName: 'FormaStudio', platform: 'win32', arch: 'x64',
    asar: true, prune: true, icon: path.join(root, 'app', 'assets', 'icon.ico'),
    appVersion: version, appCopyright: 'Copyright © 2026 Emmanuel Suah',
    win32metadata: {
      CompanyName: 'Emmanuel Suah', ProductName: 'Forma Studio',
      FileDescription: 'Forma Studio — offline 3D scene and motion editor',
      InternalName: 'FormaStudio', OriginalFilename: 'FormaStudio.exe',
    },
    ignore: [/^\/(out|test-results|tests|scripts)(\/|$)/, /^\/app\/(editor|mesh|model|viewport)\.js$/],
  });
  for (const folder of output) await stageRelease(root, folder);
  console.log('Packaged:', output.join(', '));
})().catch((error) => { console.error(error); process.exitCode = 1; });
