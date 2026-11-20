import { PRODUCTS, CATEGORIES, GITHUB, DOWNLOAD_TAG, REPOSITORY, fallbackUrl, workflowUrl, artifactName, releaseAssetUrl } from './catalog.js';

const $ = (selector) => document.querySelector(selector);
const grid = $('#tool-grid');
const filters = $('#filters');
const search = $('#search');
const dialog = $('#tool-dialog');
const dialogContent = $('#dialog-content');
const status = $('#download-status');
let category = 'all';
let publicAssets = new Set();
let availability = 'checking';
let activeProduct = null;
let lastFocused = null;

const categoryLabels = { obs: 'OBS PLUGIN', apps: 'WINDOWS APP', photoshop: 'PHOTOSHOP' };
const iconPaths = {
  grid: '<rect x="3.5" y="3.5" width="7" height="7" rx="1.5"/><rect x="13.5" y="3.5" width="7" height="7" rx="1.5"/><rect x="3.5" y="13.5" width="7" height="7" rx="1.5"/><rect x="13.5" y="13.5" width="7" height="7" rx="1.5"/>',
  sliders: '<path d="M5 4v16M12 4v16M19 4v16"/><path d="M2.5 8h5M9.5 15h5M16.5 9h5"/><circle cx="5" cy="8" r="2" fill="currentColor" stroke="none"/><circle cx="12" cy="15" r="2" fill="currentColor" stroke="none"/><circle cx="19" cy="9" r="2" fill="currentColor" stroke="none"/>',
  keyboard: '<rect x="2.5" y="5" width="19" height="14" rx="3"/><path d="M6 9h.01M10 9h.01M14 9h.01M18 9h.01M6 12h.01M10 12h.01M14 12h.01M18 12h.01M7.5 15h9"/>',
  record: '<rect x="3" y="4.5" width="18" height="13" rx="2.5"/><circle cx="12" cy="11" r="3"/><path d="M9 20h6"/>',
  shield: '<path d="M12 2.8 20 6v5.3c0 4.5-2.8 7.5-8 9.9-5.2-2.4-8-5.4-8-9.9V6l8-3.2Z"/><path d="m8.8 12 2.1 2.1 4.5-4.5"/>',
  frame: '<path d="M4 9V6a2 2 0 0 1 2-2h3M15 4h3a2 2 0 0 1 2 2v3M20 15v3a2 2 0 0 1-2 2h-3M9 20H6a2 2 0 0 1-2-2v-3"/><circle cx="12" cy="10" r="2.5"/><path d="M8 16c.6-1.5 2-2.2 4-2.2s3.4.7 4 2.2"/>',
  director: '<path d="M5 4h14v16H5zM2 7h3M2 12h3M2 17h3M19 7h3M19 12h3M19 17h3"/><path d="m10 8 5 4-5 4V8Z"/>',
  layout: '<rect x="3" y="4" width="18" height="16" rx="2.5"/><path d="M3 9h18M10 9v11M14 13h3M14 16h3"/>',
  scissors: '<circle cx="6.5" cy="6.5" r="2.5"/><circle cx="6.5" cy="17.5" r="2.5"/><path d="m8.5 8 11 10M8.5 16 20 5M11.5 11.5l1 1"/>',
  sparkles: '<path d="M12 3c.5 4.5 2.5 6.5 7 7-4.5.5-6.5 2.5-7 7-.5-4.5-2.5-6.5-7-7 4.5-.5 6.5-2.5 7-7ZM19 15c.2 1.8 1.2 2.8 3 3-1.8.2-2.8 1.2-3 3-.2-1.8-1.2-2.8-3-3 1.8-.2 2.8-1.2 3-3Z"/>',
  presence: '<circle cx="12" cy="8" r="3.5"/><path d="M5.5 19c.7-3.2 3-5 6.5-5s5.8 1.8 6.5 5M18 5l1-1M5 5 4 4"/>',
  portrait: '<path d="M7 9.5c0-3.2 2-5.5 5-5.5s5 2.3 5 5.5c0 3.8-2.1 6.5-5 6.5S7 13.3 7 9.5ZM5 20c1.1-2.1 3.7-3.2 7-3.2s5.9 1.1 7 3.2"/><path d="M9.8 10h.01M14.2 10h.01M10.5 13c.9.6 2.1.6 3 0"/>',
  export: '<path d="M5 7H4a1.5 1.5 0 0 0-1.5 1.5v10A1.5 1.5 0 0 0 4 20h13a1.5 1.5 0 0 0 1.5-1.5v-1"/><rect x="7" y="3" width="14" height="11" rx="1.8"/><path d="m11 9 2-2 2 2 2-2 2 2"/>',
};
const downloadIcon = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3v12m-4-4 4 4 4-4M5 17v3h14v-3"/></svg>';
const escapeHtml = (value) => String(value).replace(/[&<>"']/g, (char) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[char]);
const icon = (product) => `<span class="tool-icon color-${product.color}" aria-hidden="true"><svg viewBox="0 0 24 24">${iconPaths[product.icon]}</svg></span>`;
const guideUrl = (product) => `${GITHUB}/blob/main/${product.guide}`;
const shortPlatform = (product) => product.category === 'photoshop' ? 'Photoshop · UXP' : product.category === 'apps' ? 'Windows x64' : product.id === 'presence-guard' ? 'Windows · OBS 31.1' : 'Windows · OBS 33';

function getDownload(product) {
  if (publicAssets.has(product.asset)) {
    return { direct: true, href: releaseAssetUrl(product), label: 'Download', title: `Download ${product.name} ZIP` };
  }
  return { direct: false, href: fallbackUrl(product), label: 'Get build', title: `Open the ${product.name} build on GitHub` };
}

function downloadAttributes(product) {
  const info = getDownload(product);
  const newTab = info.direct ? '' : ' target="_blank" rel="noopener noreferrer"';
  return `href="${escapeHtml(info.href)}"${newTab} aria-label="${escapeHtml(info.title)}"`;
}

function createFilters() {
  for (const item of CATEGORIES) {
    const count = item.id === 'all' ? PRODUCTS.length : PRODUCTS.filter((product) => product.category === item.id).length;
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'filter-button';
    button.dataset.category = item.id;
    button.innerHTML = `${escapeHtml(item.label)} <span class="filter-count">${count}</span>`;
    filters.append(button);
  }
  updateFilters();
}

function updateFilters() {
  for (const button of filters.querySelectorAll('button')) {
    button.setAttribute('aria-pressed', String(button.dataset.category === category));
  }
}

function filteredProducts() {
  const query = search.value.trim().toLocaleLowerCase();
  return PRODUCTS.filter((product) => {
    const matchesCategory = category === 'all' || product.category === category;
    const haystack = [product.name, product.description, product.detail, product.keywords, product.compatibility, categoryLabels[product.category]].join(' ').toLocaleLowerCase();
    return matchesCategory && (!query || haystack.includes(query));
  });
}

function renderGrid() {
  const visible = filteredProducts();
  $('#result-count').textContent = `${visible.length} ${visible.length === 1 ? 'tool' : 'tools'} ${visible.length === PRODUCTS.length ? 'in the collection' : 'found'}`;
  $('#empty-state').hidden = visible.length > 0;
  grid.innerHTML = visible.map((product) => {
    const download = getDownload(product);
    return `<article class="tool-card" data-product="${escapeHtml(product.id)}">
      <div class="card-top">${icon(product)}<span class="card-category">${categoryLabels[product.category]}</span></div>
      <h3>${escapeHtml(product.name)}</h3><p>${escapeHtml(product.description)}</p>
      <div class="card-bottom"><span class="card-platform">${escapeHtml(shortPlatform(product))}</span>
        <div class="card-actions"><button class="details-button" type="button" data-details="${escapeHtml(product.id)}" aria-label="View ${escapeHtml(product.name)} details">Details</button>
          <a class="card-download" ${downloadAttributes(product)}>${downloadIcon}<span>${download.label}</span></a></div>
      </div></article>`;
  }).join('');
}

function sourceText(product) {
  if (getDownload(product).direct) {
    return `The ZIP is hosted on GitHub Releases. <a href="${GITHUB}/releases/tag/${DOWNLOAD_TAG}" target="_blank" rel="noopener noreferrer">See build provenance and checksums ↗</a>`;
  }
  return `On GitHub, open a successful <strong>main</strong> run and scroll to <strong>Artifacts</strong>. Choose <code>${escapeHtml(artifactName(product))}</code>. Sign-in may be required and artifacts can expire. <a href="${workflowUrl(product)}" target="_blank" rel="noopener noreferrer">See all builds ↗</a>`;
}

function renderDialog(product) {
  const download = getDownload(product);
  dialogContent.innerHTML = `<div class="dialog-inner">
    <div class="dialog-top">${icon(product)}<div class="dialog-heading"><p class="dialog-kicker">${categoryLabels[product.category]} &nbsp;·&nbsp; VERSION ${escapeHtml(product.version)}</p><h2 id="dialog-title">${escapeHtml(product.name)}</h2></div><button class="dialog-close" type="button" aria-label="Close details">×</button></div>
    <p id="dialog-description" class="dialog-description">${escapeHtml(product.detail)}</p>
    <div class="dialog-compat"><span aria-hidden="true">✳</span><span><strong>Works with</strong><br>${escapeHtml(product.compatibility)}</span></div>
    <h3 class="dialog-section-title">A few steps to get started</h3>
    <ol class="dialog-steps">${product.steps.map((step) => `<li>${escapeHtml(step)}</li>`).join('')}</ol>
    <div class="dialog-note"><strong>Good to know.</strong> ${escapeHtml(product.note)}</div>
    <div class="dialog-actions"><a class="button button-dark dialog-download" ${downloadAttributes(product)}>${download.direct ? 'Download ZIP' : 'Open GitHub build'} <span aria-hidden="true">↗</span></a><a class="dialog-guide" href="${escapeHtml(guideUrl(product))}" target="_blank" rel="noopener noreferrer">Read the full guide ↗</a></div>
    <p class="dialog-source">${sourceText(product)}</p>
  </div>`;
}

function openProduct(product, trigger) {
  activeProduct = product;
  lastFocused = trigger;
  renderDialog(product);
  dialog.showModal();
  dialog.querySelector('.dialog-close').focus();
}

function refreshDialogDownload() {
  if (!dialog.open || !activeProduct) return;
  const download = getDownload(activeProduct);
  const link = dialog.querySelector('.dialog-download');
  link.href = download.href;
  link.setAttribute('aria-label', download.title);
  if (download.direct) {
    link.removeAttribute('target');
    link.removeAttribute('rel');
  } else {
    link.target = '_blank';
    link.rel = 'noopener noreferrer';
  }
  link.innerHTML = `${download.direct ? 'Download ZIP' : 'Open GitHub build'} <span aria-hidden="true">↗</span>`;
  dialog.querySelector('.dialog-source').innerHTML = sourceText(activeProduct);
}

function updateStatus() {
  const message = status.querySelector('p');
  if (availability === 'checking') return;
  if (publicAssets.size === PRODUCTS.length) {
    message.textContent = 'Direct ZIPs are hosted on GitHub Releases. Some Windows builds are unsigned; check the requirements, guide, and published checksums before running them.';
  } else if (publicAssets.size > 0) {
    message.textContent = `${publicAssets.size} of ${PRODUCTS.length} ZIPs are available directly. Other links open GitHub Actions, where sign-in may be required and artifacts can expire.`;
  } else {
    message.textContent = 'Direct downloads could not be checked or are not yet available. Build links open GitHub Actions; sign-in may be required and artifacts can expire.';
  }
}

async function checkDownloads() {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 8000);
  try {
    const response = await fetch(`https://api.github.com/repos/${REPOSITORY}/releases/tags/${DOWNLOAD_TAG}`, {
      headers: { Accept: 'application/vnd.github+json' }, signal: controller.signal,
    });
    if (!response.ok) throw new Error(`GitHub returned ${response.status}`);
    const release = await response.json();
    if (!Array.isArray(release.assets) || release.draft) throw new Error('Invalid release response');
    const expected = new Set(PRODUCTS.map((product) => product.asset));
    publicAssets = new Set(release.assets.filter((asset) => asset.state === 'uploaded' && expected.has(asset.name)).map((asset) => asset.name));
    availability = 'ready';
  } catch {
    availability = 'unavailable';
    publicAssets = new Set();
  } finally {
    clearTimeout(timer);
    // Do not destroy keyboard focus in a card if the request completes mid-use.
    const focusedCard = document.activeElement?.closest('.tool-card');
    const focusedId = focusedCard?.dataset.product;
    const wasDetails = document.activeElement?.matches('.details-button');
    renderGrid();
    if (focusedId) {
      const card = [...grid.querySelectorAll('.tool-card')].find((item) => item.dataset.product === focusedId);
      card?.querySelector(wasDetails ? '.details-button' : '.card-download')?.focus();
    }
    updateStatus();
    refreshDialogDownload();
  }
}

filters.addEventListener('click', (event) => {
  const button = event.target.closest('button[data-category]');
  if (!button) return;
  category = button.dataset.category;
  updateFilters();
  renderGrid();
});
search.addEventListener('input', renderGrid);
$('#clear-filters').addEventListener('click', () => {
  category = 'all';
  search.value = '';
  updateFilters();
  renderGrid();
  search.focus();
});
grid.addEventListener('click', (event) => {
  const button = event.target.closest('button[data-details]');
  if (!button) return;
  const product = PRODUCTS.find((item) => item.id === button.dataset.details);
  if (product) openProduct(product, button);
});
dialog.addEventListener('click', (event) => {
  if (event.target === dialog || event.target.closest('.dialog-close')) dialog.close();
});
dialog.addEventListener('close', () => {
  const productId = activeProduct?.id;
  activeProduct = null;
  // The grid can be refreshed while the dialog is open; fall back to its card.
  if (lastFocused?.isConnected) lastFocused.focus();
  else {
    const card = [...grid.querySelectorAll('.tool-card')].find((item) => item.dataset.product === productId);
    (card?.querySelector('.details-button') || search).focus();
  }
});

createFilters();
renderGrid();
checkDownloads();
