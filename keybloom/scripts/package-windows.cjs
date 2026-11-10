/* KeyBloom package script. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const path = require('node:path');
const { packager } = require('@electron/packager');

(async () => {
  const root = path.resolve(__dirname, '..');
  const output = await packager({
    dir: root,
    out: path.join(root, 'out'),
    overwrite: true,
    name: 'KeyBloom',
    executableName: 'KeyBloom',
    platform: 'win32',
    arch: 'x64',
    asar: true,
    prune: true,
    icon: path.join(root, 'app', 'assets', 'icon.ico'),
    appVersion: '1.0.0',
    appCopyright: 'Copyright © 2026 Emmanuel Suah',
    win32metadata: {
      CompanyName: 'Emmanuel Suah',
      FileDescription: 'KeyBloom — an offline typing tutor',
      ProductName: 'KeyBloom',
      InternalName: 'KeyBloom',
      OriginalFilename: 'KeyBloom.exe',
    },
    ignore: [/^\/(out|test-results|tests|scripts)(\/|$)/],
  });
  console.log('Packaged:', output.join(', '));
})().catch((error) => { console.error(error); process.exitCode = 1; });
