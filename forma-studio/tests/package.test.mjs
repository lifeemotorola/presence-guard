/* Forma Studio release staging test. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm, writeFile, access } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { stageRelease } from '../scripts/stage-release.cjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
test('portable staging preserves Electron notices and includes the source needed to rebuild', async () => {
  const folder = await mkdtemp(join(tmpdir(), 'forma-stage-test-'));
  try {
    await writeFile(join(folder, 'LICENSE'), 'Existing Electron license');
    await stageRelease(root, folder);
    assert.equal(await readFile(join(folder, 'LICENSE'), 'utf8'), 'Existing Electron license');
    assert.deepEqual(await readFile(join(folder, 'FORMA-LICENSE.txt')), await readFile(join(root, 'LICENSE')));
    for (const file of ['INSTALL.txt', 'README.md', 'THREE-LICENSE.txt', 'SOURCE/INSTALL.txt', 'SOURCE/LICENSE',
      'SOURCE/package.json', 'SOURCE/package-lock.json', 'SOURCE/app/editor.js', 'SOURCE/app/mesh.js', 'SOURCE/app/model.js',
      'SOURCE/app/viewport.js', 'SOURCE/desktop/main.cjs', 'SOURCE/scripts/stage-release.cjs',
      'SOURCE/app/assets/fonts/DM-SANS-LICENSE.txt', 'SOURCE/app/assets/fonts/PLUS-JAKARTA-SANS-LICENSE.txt']) {
      await access(join(folder, file));
    }
    await assert.rejects(access(join(folder, 'SOURCE/app/bundle.js')), { code: 'ENOENT' });
    await assert.rejects(access(join(folder, 'SOURCE/node_modules')), { code: 'ENOENT' });
  } finally { await rm(folder, { recursive: true, force: true }); }
});
