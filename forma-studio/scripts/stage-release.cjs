/* Stage portable Forma Studio files. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const path = require('node:path');
const fs = require('node:fs/promises');

async function stageRelease(root, folder) {
  // The portable download contains readable instructions, third-party notices,
  // and the app's preferred source form alongside the executable.
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
module.exports = { stageRelease };
