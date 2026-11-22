/* Forma Studio Windows packager. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const path = require('node:path');
const fs = require('node:fs/promises');
const { packager } = require('@electron/packager');
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
    ignore: [/^\/(out|test-results|tests|scripts)(\/|$)/, /^\/app\/(editor|model|viewport)\.js$/],
  });
  for (const folder of output) {
    // The portable download contains readable instructions, third-party
    // notices, and the app's preferred source form alongside the executable.
    for (const file of ['INSTALL.txt', 'README.md', 'THREE-LICENSE.txt']) {
      await fs.copyFile(path.join(root, file), path.join(folder, file));
    }
    // Keep Electron's own LICENSE intact; give the app license a distinct name.
    await fs.copyFile(path.join(root, 'LICENSE'), path.join(folder, 'FORMA-LICENSE.txt'));
    const source = path.join(folder, 'SOURCE');
    await fs.mkdir(source, { recursive: true });
    for (const directory of ['app', 'desktop', 'scripts', 'tests']) {
      await fs.cp(path.join(root, directory), path.join(source, directory), {
        recursive: true, filter: (src) => !src.endsWith(`${path.sep}bundle.js`),
      });
    }
    for (const file of ['package.json', 'package-lock.json', 'INSTALL.txt', 'README.md', 'LICENSE', 'THREE-LICENSE.txt']) {
      await fs.copyFile(path.join(root, file), path.join(source, file));
    }
  }
  console.log('Packaged:', output.join(', '));
})().catch((error) => { console.error(error); process.exitCode = 1; });
