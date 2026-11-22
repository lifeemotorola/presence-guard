/* Forma Studio bundler. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import { build } from 'esbuild';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
await build({
  entryPoints: [resolve(root, 'app/editor.js')],
  outfile: resolve(root, 'app/bundle.js'),
  bundle: true,
  format: 'iife',
  platform: 'browser',
  target: ['chrome125'],
  minify: true,
  legalComments: 'none',
});
console.log('Built the offline Forma Studio renderer.');
