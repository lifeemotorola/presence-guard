import { createWriteStream } from 'node:fs';
import { stat } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { Transform } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { DOWNLOAD_TAG, GITHUB } from '../catalog.js';

const DIGEST = /^sha256:[a-f0-9]{64}$/i;

export function isEligibleRun(run, repository) {
  return Number.isSafeInteger(run.id) && run.id > 0 &&
    run.status === 'completed' && run.conclusion === 'success' &&
    run.head_branch === 'main' && run.head_repository?.full_name === repository &&
    ['push', 'workflow_dispatch', 'schedule'].includes(run.event) &&
    /^[a-f0-9]{40}$/i.test(run.head_sha || '');
}

export function matchesArtifact(product, artifact, now = Date.now()) {
  const nameMatches = product.artifact
    ? artifact.name === product.artifact
    : new RegExp(product.artifactPattern).test(artifact.name || '');
  return nameMatches && artifact.expired === false &&
    Date.parse(artifact.expires_at) > now &&
    Number.isSafeInteger(artifact.id) && artifact.id > 0 &&
    DIGEST.test(artifact.digest || '');
}

export function sameDigest(asset, artifact) {
  return Boolean(asset && DIGEST.test(asset.digest || '') &&
    asset.digest.toLowerCase() === artifact.digest.toLowerCase());
}

export function publishedBuildIsNewer(asset, previous, candidateRun) {
  if (!asset || !previous || previous.asset !== asset.name ||
      asset.digest?.toLowerCase() !== `sha256:${String(previous.sha256 || '').toLowerCase()}`) return false;
  const publishedAt = Date.parse(previous.runCreatedAt);
  const candidateAt = Date.parse(candidateRun.created_at);
  return Number.isFinite(publishedAt) && Number.isFinite(candidateAt) && publishedAt > candidateAt;
}

export async function writeVerifiedArchive(stream, destination, expectedDigest) {
  if (!DIGEST.test(expectedDigest || '')) throw new Error('A valid SHA-256 artifact digest is required');
  const hash = createHash('sha256');
  const counter = new Transform({
    transform(chunk, _encoding, callback) { hash.update(chunk); callback(null, chunk); },
  });
  await pipeline(stream, counter, createWriteStream(destination, { flags: 'wx' }));
  const digest = `sha256:${hash.digest('hex')}`;
  if ((await stat(destination)).size === 0 || digest.toLowerCase() !== expectedDigest.toLowerCase()) {
    throw new Error('Downloaded artifact failed the GitHub SHA-256 digest check');
  }
}

export function readPreviousMetadata(body) {
  const match = (body || '').match(/<!-- creator-toolkit-metadata:([A-Za-z0-9+/=]+) -->/);
  if (!match || match[1].length > 100_000) return {};
  try {
    const value = JSON.parse(Buffer.from(match[1], 'base64').toString('utf8'));
    return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
  } catch {
    return {};
  }
}

export function releaseBody(products, assets, metadata) {
  const lines = [
    'Prebuilt ZIPs from successful main-branch GitHub Actions runs. Download the ZIP for your tool and follow its installation guide.',
    '',
    '**Please read:** Some Windows builds are unsigned. A checksum verifies that a file matches a build; it does not establish publisher identity or safety. ScreenForge requires a separate ffmpeg download. Photoshop panels require the UXP Developer Tool and have not been verified in a real Photoshop host. Presence Guard was built against OBS 31.1.1; compatibility with OBS 33 is unverified.',
    '',
    'This is a rolling collection: individual ZIPs update when their corresponding main-branch build succeeds. Existing assets remain available if their original Actions artifacts expire.',
    '',
    '| Tool | Download | Source build | ZIP SHA-256 |',
    '| --- | --- | --- | --- |',
  ];
  for (const product of products) {
    const asset = assets.find((item) => item.name === product.asset && item.state === 'uploaded');
    if (!asset) continue;
    const item = metadata[product.id];
    const trustedMetadata = item && item.asset === product.asset &&
      typeof item.sha256 === 'string' && /^[a-f0-9]{64}$/i.test(item.sha256) &&
      Number.isSafeInteger(item.runId) && item.runId > 0 &&
      (!asset.digest || asset.digest.toLowerCase() === `sha256:${item.sha256.toLowerCase()}`);
    const url = `${GITHUB}/releases/download/${DOWNLOAD_TAG}/${product.asset}`;
    const build = trustedMetadata ? `[Actions run ${item.runId}](${GITHUB}/actions/runs/${item.runId})` : 'See workflow';
    const sha = trustedMetadata ? `\`${item.sha256}\`` : 'See asset digest';
    lines.push(`| ${product.name} | [Download ZIP](${url}) | ${build} | ${sha} |`);
  }
  // Preserve provenance for already-published assets even after Actions expires
  // their original artifacts. The visible table remains useful without this.
  lines.push('', `<!-- creator-toolkit-metadata:${Buffer.from(JSON.stringify(metadata)).toString('base64')} -->`, '');
  return lines.join('\n');
}
