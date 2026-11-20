#!/usr/bin/env node
// Copy verified, successful main-branch Actions artifact ZIPs to a rolling
// public release. Artifact downloads and uploads happen only on GitHub's
// runner; no binaries are committed to the repository.
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Readable } from 'node:stream';
import { execFileSync } from 'node:child_process';
import { PRODUCTS, REPOSITORY, DOWNLOAD_TAG } from '../catalog.js';
import { isEligibleRun, matchesArtifact, sameDigest, publishedBuildIsNewer, writeVerifiedArchive, readPreviousMetadata, releaseBody } from './publish-lib.mjs';

const token = process.env.GH_TOKEN || process.env.GITHUB_TOKEN;
const dryRun = process.argv.includes('--dry-run');
if (!token) throw new Error('GH_TOKEN or GITHUB_TOKEN is required.');
if (process.env.GITHUB_REPOSITORY && process.env.GITHUB_REPOSITORY !== REPOSITORY) {
  throw new Error(`Refusing to publish from a different repository: ${process.env.GITHUB_REPOSITORY}`);
}
const apiRoot = `https://api.github.com/repos/${REPOSITORY}`;
const headers = {
  Accept: 'application/vnd.github+json',
  Authorization: `Bearer ${token}`,
  'X-GitHub-Api-Version': '2022-11-28',
  'User-Agent': 'creator-toolkit-download-publisher',
};

async function api(path, options = {}) {
  const response = await fetch(`${apiRoot}${path}`, {
    ...options,
    headers: { ...headers, ...options.headers },
  });
  if (!response.ok) {
    const error = new Error(`GitHub API ${path}: HTTP ${response.status}`);
    error.status = response.status;
    throw error;
  }
  return response.status === 204 ? null : response.json();
}

async function getRelease() {
  try { return await api(`/releases/tags/${DOWNLOAD_TAG}`); }
  catch (error) { if (error.status === 404) return null; throw error; }
}

async function createRelease() {
  return api('/releases', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      tag_name: DOWNLOAD_TAG,
      target_commitish: 'main',
      name: 'The Creator Toolkit — downloads',
      body: 'Verified copies of successful main-branch build artifacts are being published. See the individual product guides before installing.',
      draft: false,
      prerelease: false,
      make_latest: 'false',
    }),
  });
}

async function findCandidate(product) {
  const data = await api(`/actions/workflows/${encodeURIComponent(product.workflow)}/runs?branch=main&status=success&per_page=30`);
  if (!Array.isArray(data.workflow_runs)) throw new Error(`Invalid workflow response for ${product.name}`);
  for (const run of data.workflow_runs.filter((item) => isEligibleRun(item, REPOSITORY))) {
    let dataForRun;
    try { dataForRun = await api(`/actions/runs/${run.id}/artifacts?per_page=100`); }
    catch (error) { if (error.status === 404) continue; throw error; }
    const artifact = dataForRun.artifacts?.find((item) => matchesArtifact(product, item));
    if (artifact) return { run, artifact };
  }
  return null;
}

async function downloadAndVerify(artifact, destination) {
  // Node's fetch follows GitHub's signed, short-lived redirect; authorization
  // is stripped by fetch when the redirect crosses to the storage host.
  const response = await fetch(`${apiRoot}/actions/artifacts/${artifact.id}/zip`, { headers });
  if (!response.ok || !response.body) throw new Error(`Artifact ${artifact.id}: HTTP ${response.status}`);
  await writeVerifiedArchive(Readable.fromWeb(response.body), destination, artifact.digest);
}

async function refreshUploadedAsset(name, expectedDigest) {
  // The release API is normally immediate, but allow for brief propagation.
  for (let attempt = 0; attempt < 4; attempt++) {
    const refreshed = await getRelease();
    const asset = refreshed?.assets?.find((item) => item.name === name && item.state === 'uploaded');
    if (asset && (!asset.digest || asset.digest.toLowerCase() === expectedDigest.toLowerCase())) return refreshed;
    await new Promise((resolve) => setTimeout(resolve, 1500));
  }
  throw new Error(`Uploaded asset ${name} was not confirmed by the release API`);
}

async function main() {
  let release = await getRelease();
  const metadata = readPreviousMetadata(release?.body);
  const failures = [];
  const scratch = await mkdtemp(join(tmpdir(), 'creator-toolkit-'));
  try {
    for (const product of PRODUCTS) {
      try {
        const candidate = await findCandidate(product);
        const currentAsset = release?.assets?.find((item) => item.name === product.asset && item.state === 'uploaded');
        if (!candidate) {
          if (currentAsset) {
            console.log(`${product.name}: no unexpired artifact; keeping existing public ZIP`);
            continue;
          }
          throw new Error('No unexpired artifact found in a successful main-branch run. Re-run the product workflow.');
        }
        const { run, artifact } = candidate;
        if (publishedBuildIsNewer(currentAsset, metadata[product.id], run)) {
          console.log(`${product.name}: the published ZIP came from a newer run; keeping it`);
          continue;
        }
        if (dryRun) {
          console.log(`${product.name}: run ${run.id}, artifact ${artifact.name}, digest ${artifact.digest}`);
          continue;
        }
        if (!sameDigest(currentAsset, artifact)) {
          const destination = join(scratch, product.asset);
          console.log(`${product.name}: downloading and verifying artifact ${artifact.id}`);
          await downloadAndVerify(artifact, destination);
          if (!release) release = await createRelease();
          // --clobber updates just this named asset; unrelated assets are untouched.
          execFileSync('gh', ['release', 'upload', DOWNLOAD_TAG, destination, '--repo', REPOSITORY, '--clobber'], { stdio: 'inherit' });
          release = await refreshUploadedAsset(product.asset, artifact.digest);
          await rm(destination, { force: true });
        } else {
          console.log(`${product.name}: public ZIP already matches artifact ${artifact.id}`);
        }
        metadata[product.id] = {
          asset: product.asset,
          runId: run.id,
          runCreatedAt: run.created_at,
          artifactId: artifact.id,
          artifactName: artifact.name,
          commit: run.head_sha,
          sha256: artifact.digest.slice('sha256:'.length).toLowerCase(),
        };
      } catch (error) {
        failures.push(`${product.name}: ${error.message}`);
        console.error(`::error::${product.name}: ${error.message}`);
      }
    }
    if (!dryRun && release) {
      // Re-read assets so an interrupted replacement cannot be listed as present.
      release = await getRelease();
      await api(`/releases/${release.id}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ body: releaseBody(PRODUCTS, release.assets, metadata), make_latest: 'false' }),
      });
    }
  } finally {
    await rm(scratch, { recursive: true, force: true });
  }
  if (failures.length) {
    console.error(`\n${failures.length} product(s) could not be published:\n${failures.join('\n')}`);
    process.exitCode = 1;
  } else {
    console.log(dryRun ? '\nDry run complete; no release was changed.' : '\nCreator Toolkit downloads are up to date.');
  }
}

await main();
