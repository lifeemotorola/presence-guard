# The Creator Toolkit website

A static download hub for all 15 products in this repository, created for Emmanuel Suah. It includes search, category filters, product-specific requirements and installation steps. The site has no backend, analytics, or framework; JavaScript is required for the catalog.

## Run locally

From the repository root:

```sh
python3 -m http.server 4173 --bind 0.0.0.0 --directory website
```

Open `http://localhost:4173/` on the machine running the server. Serve it over HTTP rather than opening `index.html` directly, because it uses JavaScript modules.

## Publish the website

GitHub Pages must be enabled **once** by a repository administrator:

1. On GitHub, open **Settings → Pages** for this repository.
2. Under **Build and deployment → Source**, choose **GitHub Actions**.
3. Open **Actions → Creator Toolkit website → Run workflow**, choose `main`, and run it. Once the deployment succeeds, the workflow displays the site URL (normally `https://lifeemotorola.github.io/presence-guard/`).

Subsequent changes to `website/` on `main` automatically test and deploy. If Pages is not enabled or is set to deploy from a branch, the workflow reports a notice and skips deployment rather than pretending the site is live. Enabling Pages requires repository administration and Pages write permission; the normal workflow token cannot enable it.

## Downloads and provenance

`.github/workflows/publish-creator-downloads.yml` maintains the public **creator-downloads** GitHub Release. On relevant successful main-branch builds, a weekly schedule, or manual dispatch, the publisher:

- considers only successful, completed runs on this repository's `main` branch;
- matches the exact product artifact (or Presence Guard's versioned Windows name), rejects expired or digest-less artifacts, and checks the downloaded ZIP's SHA-256 against GitHub's artifact digest before uploading;
- preserves existing release assets if the original Actions artifacts have expired, and records the build run and ZIP digest in the release notes;
- uploads binaries to the release, **not to Git**. It never runs code from a downloaded artifact.

The site checks the GitHub release API and links directly to available ZIPs. If the release or GitHub API is unavailable, it links to a known unexpired Actions run or, after its known expiration, to the product's workflow. Actions downloads may require GitHub sign-in, and artifacts expire. The fallback is deliberately labeled **Get build**, not a guaranteed direct download.

Release assets are unsigned where the underlying build is unsigned. A checksum only proves that a ZIP matches the referenced build; it is not a digital signature or guarantee of safety. Read the product's guide and compatibility notes before installing.

The publisher requires the workflow's `contents: write` and `actions: read` permissions. If a product has neither a public asset nor a retained successful artifact, rerun that product's workflow on `main`, then run **Publish Creator Toolkit downloads**. A failure leaves unrelated assets untouched and is reported in Actions.

## Maintain and test

The catalog's single source of truth is `catalog.js`, shared by the site and publisher. When adding a product, supply its real workflow and artifact name, a unique release asset name, accurate requirements, instructions, caveats, and a valid fallback. Add its workflow name to the publisher's `workflow_run` list as well. Do not claim a host or platform has been tested unless it has been.

```sh
npm ci --prefix website
npm test --prefix website
CHROMIUM_PATH=/path/to/chromium npm run test:browser --prefix website
```

The browser tests exercise public downloads, unavailable and partial releases, expired fallback links, search, filters, dialogs, mobile widths, keyboard focus, and automated accessibility checks. The publisher also supports `--dry-run` in an already authenticated GitHub environment; it only reports candidate artifacts and does not modify the release.
