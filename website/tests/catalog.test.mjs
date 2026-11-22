import test from 'node:test';
import assert from 'node:assert/strict';
import { access, mkdtemp, rm, readFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { createHash } from 'node:crypto';
import { Readable } from 'node:stream';
import { PRODUCTS, CATEGORIES, GITHUB, fallbackUrl, workflowUrl, releaseAssetUrl, artifactName } from '../catalog.js';
import { isEligibleRun, matchesArtifact, sameDigest, publishedBuildIsNewer, writeVerifiedArchive, readPreviousMetadata, releaseBody } from '../scripts/publish-lib.mjs';

const digest = `sha256:${'a'.repeat(64)}`;
const goodRun = {
  id: 123, status: 'completed', conclusion: 'success', head_branch: 'main',
  head_repository: { full_name: 'lifeemotorola/presence-guard' }, event: 'push', head_sha: 'b'.repeat(40),
};
const goodArtifact = { id: 456, name: 'obs-dockbloom-windows-x64', expired: false, expires_at: '2030-01-01T00:00:00Z', digest };

test('the complete catalog has unique products, assets and valid source workflows', async () => {
  assert.equal(PRODUCTS.length, 14);
  assert.deepEqual(CATEGORIES.map((category) => category.id), ['all', 'obs', 'apps', 'photoshop']);
  assert.deepEqual(Object.fromEntries(['obs', 'apps', 'photoshop'].map((category) => [category, PRODUCTS.filter((product) => product.category === category).length])), { obs: 9, apps: 3, photoshop: 2 });
  for (const field of ['id', 'name', 'asset']) assert.equal(new Set(PRODUCTS.map((product) => product[field])).size, PRODUCTS.length, `${field} must be unique`);
  for (const product of PRODUCTS) {
    assert.match(product.id, /^[a-z0-9-]+$/);
    assert.match(product.asset, /^[a-z0-9-]+\.zip$/);
    assert.ok(product.artifact || (product.artifactPattern && product.artifactLabel));
    assert.ok(product.compatibility && product.note && product.description && product.detail);
    assert.ok(product.steps.length >= 3);
    // A new product may not yet have a successful main-branch build; its
    // fallback goes to the workflow until a public ZIP has been published.
    if (product.fallbackRun) assert.match(product.fallbackExpires, /^\d{4}-\d\d-\d\dT/);
    else assert.equal(fallbackUrl(product), workflowUrl(product));
    assert.ok(artifactName(product));
    await access(new URL(`../../.github/workflows/${product.workflow}`, import.meta.url));
    await access(new URL(`../../${product.guide}`, import.meta.url));
  }
});

test('download destinations stay on this repository and expire safely to the workflow', () => {
  const product = PRODUCTS.find((item) => item.id === 'dockbloom');
  assert.equal(fallbackUrl(product, Date.parse('2026-01-01T00:00:00Z')), `${GITHUB}/actions/runs/44787973114`);
  assert.equal(fallbackUrl(product, Date.parse('2030-01-01T00:00:00Z')), workflowUrl(product));
  assert.equal(releaseAssetUrl(product), `${GITHUB}/releases/download/creator-downloads/dockbloom-windows-x64.zip`);
});

test('only completed successful main runs from this repository are eligible', () => {
  const repository = 'lifeemotorola/presence-guard';
  assert.equal(isEligibleRun(goodRun, repository), true);
  for (const bad of [
    { ...goodRun, status: 'in_progress' }, { ...goodRun, conclusion: 'failure' },
    { ...goodRun, head_branch: 'feature' }, { ...goodRun, event: 'pull_request' },
    { ...goodRun, head_repository: { full_name: 'someone/fork' } }, { ...goodRun, head_sha: 'not-a-sha' },
  ]) assert.equal(isEligibleRun(bad, repository), false);
});

test('only exact, live, digest-bearing artifacts are candidates', () => {
  const product = PRODUCTS.find((item) => item.id === 'dockbloom');
  const now = Date.parse('2026-01-01');
  assert.equal(matchesArtifact(product, goodArtifact, now), true);
  assert.equal(matchesArtifact(product, { ...goodArtifact, name: 'dockbloom-ui-test' }, now), false);
  assert.equal(matchesArtifact(product, { ...goodArtifact, expired: true }, now), false);
  assert.equal(matchesArtifact(product, { ...goodArtifact, expires_at: '2025-01-01T00:00:00Z' }, now), false);
  assert.equal(matchesArtifact(product, { ...goodArtifact, digest: null }, now), false);
  const presence = PRODUCTS.find((item) => item.id === 'presence-guard');
  assert.equal(matchesArtifact(presence, { ...goodArtifact, name: 'presence-guard-0.1.0-windows-x64-94cce7252' }, now), true);
  assert.equal(matchesArtifact(presence, { ...goodArtifact, name: 'presence-guard-0.1.0-macos-universal-94cce7252' }, now), false);
  assert.equal(matchesArtifact(presence, { ...goodArtifact, name: 'presence-guard-0.1.0-windows-x64-94cce7252-dbgsym' }, now), false);
});

test('archive streaming accepts only bytes matching the published SHA-256', async () => {
  const directory = await mkdtemp(join(tmpdir(), 'creator-test-'));
  const contents = Buffer.from('pretend this is a ZIP');
  const expected = `sha256:${createHash('sha256').update(contents).digest('hex')}`;
  try {
    const path = join(directory, 'good.zip');
    await writeVerifiedArchive(Readable.from([contents]), path, expected);
    assert.deepEqual(await readFile(path), contents);
    await assert.rejects(writeVerifiedArchive(Readable.from([Buffer.from('altered')]), join(directory, 'bad.zip'), expected), /digest check/);
    await assert.rejects(writeVerifiedArchive(Readable.from([contents]), join(directory, 'missing.zip'), 'invalid'), /valid SHA-256/);
    await assert.rejects(writeVerifiedArchive(Readable.from([]), join(directory, 'empty.zip'), `sha256:${createHash('sha256').update('').digest('hex')}`), /digest check/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('unchanged releases are not replaced and provenance is preserved safely', () => {
  const product = PRODUCTS[0];
  const asset = { name: product.asset, state: 'uploaded', digest };
  assert.equal(sameDigest(asset, goodArtifact), true);
  assert.equal(sameDigest({ ...asset, digest: `sha256:${'c'.repeat(64)}` }, goodArtifact), false);
  const newer = { asset: product.asset, sha256: 'a'.repeat(64), runCreatedAt: '2026-02-01T00:00:00Z' };
  assert.equal(publishedBuildIsNewer(asset, newer, { created_at: '2026-01-01T00:00:00Z' }), true);
  assert.equal(publishedBuildIsNewer(asset, newer, { created_at: '2026-03-01T00:00:00Z' }), false);
  assert.equal(publishedBuildIsNewer({ ...asset, digest: `sha256:${'c'.repeat(64)}` }, newer, { created_at: '2026-01-01T00:00:00Z' }), false);
  const metadata = { [product.id]: { asset: product.asset, runId: 123, sha256: 'a'.repeat(64) } };
  const body = releaseBody([product], [asset], metadata);
  assert.match(body, /Actions run 123/);
  assert.match(body, /DockBloom/);
  assert.deepEqual(readPreviousMetadata(body), metadata);
  assert.deepEqual(readPreviousMetadata('not metadata'), {});
  assert.doesNotMatch(releaseBody([product], [{ ...asset, digest: `sha256:${'c'.repeat(64)}` }], metadata), /Actions run 123/);
  assert.doesNotMatch(releaseBody([product], [], metadata), /Download ZIP\]/);
});

test('important compatibility and installation limits remain visible', () => {
  const get = (id) => PRODUCTS.find((product) => product.id === id);
  assert.match(get('presence-guard').note, /OBS 33 has not been verified/i);
  assert.match(get('screenforge').note, /ffmpeg/i);
  assert.match(get('keybloom').note, /portable folder/i);
  assert.match(get('forma-studio').note, /no AI, mesh editing, or rigging/i);
  assert.match(get('forma-studio').note, /current-frame/i);
  assert.match(get('forma-studio').note, /unsigned/i);
  assert.match(get('onair-twin').note, /does not perform failover/i);
  for (const id of ['face-retouch', 'export-kit']) {
    assert.match(get(id).compatibility, /UXP Developer Tool/);
    assert.match(get(id).note, /not.*\.ccx/i);
  }
});
