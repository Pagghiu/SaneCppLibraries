import { buildSingleLibraryHeaderUsingContext } from './amalgamationCore.js';

/*
  app.js — Sane C++ Libraries (browser UI)

  High-level summary
  - Browser UI that uses amalgamationCore.js to build single-file headers in-browser.
  - Mirrors the Python script output. amalgamationCore.js is the single source of truth.

  Data sources
  - Local: Support/SingleFileLibs/Dependencies.json (lists direct/all dependencies per library)
  - GitHub (selected ref = branch/tag/SHA):
      * Raw files via raw.githubusercontent.com (sources and Documentation/Libraries/*.md for @brief)
      * REST API for releases list, commit SHA resolution, and a recursive tree listing
  - Optional GitHub token can be provided to increase API rate limits

  Caching and rate limits
  - commitShaCache: ref -> commit SHA
  - repoTreeByRef: ref -> { sha, tree } from the Git trees API (recursive=1)
  - fileContentCache: (ref:path) -> file text (raw)
  - API calls include a clear rate-limit error message (403 or remaining=0) with reset ETA
  - 401 surfaces a helpful token message; optional token is stored locally

  Amalgamation
  - Delegated to amalgamationCore.js (see that file for precise rules and formatting).

  Versioning and ordering
  - Version line uses the latest release tag (if available) or the selected ref, plus the short commit SHA
  - If no explicit order file is present, both tools use deterministic ordering. Python sorts by basename;
    JS resolves from the Git tree and performs DFS over the provided order list. Both now match effectively.

  UI/UX
  - Release selector (latest branch or a tag), with link to the tag/tree page
  - Emoji theme toggle (🌙/☀️), persisted in localStorage, overrides system theme
  - Filter box for libraries
  - Library column links to public docs page (lower_snake_case mapping)
  - Action column per library: Download button, dependencies toggle (hidden by default)
  - Global HUD progress with Cancel for “Download All” (AbortController)
  - Inline progress with Cancel for single-library Download

  Packaging
  - “Download All” generates a ZIP with all headers and includes README.md, LICENSE.txt
  - JSZip is loaded via CDN with a runtime fallback if not present; errors are surfaced clearly
*/

(() => {
  const OWNER = 'pagghiu';
  const REPO = 'SaneCppLibraries';

  // DOM
  const releaseSelect = document.getElementById('releaseSelect');
  const releaseLink = document.getElementById('releaseLink');
  const versionInfo = document.getElementById('versionInfo');
  const libsTbody = document.getElementById('libsTbody');
  const filterInput = document.getElementById('filterInput');
  const downloadAllBtn = document.getElementById('downloadAllBtn');
  const themeToggleBtn = document.getElementById('themeToggleBtn');
  const ghTokenInput = document.getElementById('ghToken');
  // HUD
  const hudOverlay = document.getElementById('hudOverlay');
  const hudTitle = document.getElementById('hudTitle');
  const hudStatus = document.getElementById('hudStatus');
  const hudBar = document.getElementById('hudBar');
  const hudCancel = document.getElementById('hudCancel');
  // Modal
  const exampleModalOpen = document.getElementById('exampleModalOpen');
  const exampleModal = document.getElementById('exampleModal');

  // State
  const state = {
    defaultBranch: 'main',
    currentRef: 'main', // branch or tag name or commit SHA
    currentRefType: 'branch', // 'branch' | 'tag' | 'commit'
    latestReleaseTag: null,
    latestCommitSha: null,
    dependencyGraph: {},
    libraryNames: [],
    libraryBriefByName: {},
    repoTreeByRef: new Map(), // ref -> {sha, tree}
    fileContentCache: new Map(), // key: `${ref}:${path}` -> text
    cancelController: null,
  };

  // Utils
  const sleep = (ms) => new Promise(r => setTimeout(r, ms));

  function setTheme(dark) {
    document.documentElement.setAttribute('data-theme', dark ? 'dark' : 'light');
    if (themeToggleBtn) themeToggleBtn.textContent = dark ? '☀️' : '🌙';
    localStorage.setItem('scl_theme', dark ? 'dark' : 'light');
  }

  // HUD helpers
  function hudShow(title) {
    if (!hudOverlay) return;
    hudTitle.textContent = title || 'Working…';
    hudStatus.textContent = 'Preparing…';
    hudBar.style.width = '0%';
    hudOverlay.style.display = 'block';
  }
  function hudHide() { if (hudOverlay) hudOverlay.style.display = 'none'; }
  function hudUpdate(status, pct) {
    if (hudOverlay && status !== undefined) hudStatus.textContent = status;
    if (hudOverlay && typeof pct === 'number') hudBar.style.width = `${Math.max(0, Math.min(100, pct))}%`;
  }

  function initTheme() {
    const stored = localStorage.getItem('scl_theme');
    if (stored) return setTheme(stored === 'dark');
    const prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
    setTheme(prefersDark);
  }

  // Network helpers
  function authHeaders() {
    const headers = { 'Accept': 'application/vnd.github+json' };
    const tok = (ghTokenInput && ghTokenInput.value) || localStorage.getItem('scl_gh_token') || '';
    if (tok) headers['Authorization'] = `Bearer ${tok}`;
    return headers;
  }

  function buildRateLimitMessage(res, payloadMessage) {
    const remaining = res.headers.get('x-ratelimit-remaining');
    const reset = res.headers.get('x-ratelimit-reset');
    let resetInfo = '';
    if (reset) {
      const resetMs = parseInt(reset, 10) * 1000;
      const etaMin = Math.max(0, Math.round((resetMs - Date.now()) / 60000));
      const date = new Date(resetMs);
      resetInfo = ` (resets around ${date.toLocaleTimeString()} in ~${etaMin}m)`;
    }
    const hasToken = !!((ghTokenInput && ghTokenInput.value) || localStorage.getItem('scl_gh_token'));
    const tokenHint = hasToken ? 'Please wait for the limit to reset or use fewer bulk operations.' : 'Add a GitHub token (no scopes needed) in the header to raise limits.';
    const baseMsg = 'GitHub API rate limit exceeded';
    const payload = payloadMessage ? `: ${payloadMessage}` : '';
    const rem = remaining !== null ? ` (remaining: ${remaining})` : '';
    return `${baseMsg}${payload}${rem}${resetInfo}. ${tokenHint}`;
  }

  async function fetchJSON(url, opts = {}) {
    const res = await fetch(url, { headers: { ...authHeaders() }, ...opts });
    if (!res.ok) {
      let message = `HTTP ${res.status} for ${url}`;
      let bodyText = '';
      try { bodyText = await res.text(); } catch {}
      let payload;
      try { payload = JSON.parse(bodyText); } catch { payload = null; }
      const payloadMsg = payload && payload.message ? payload.message : '';
      if (res.status === 403 && (payloadMsg.toLowerCase().includes('rate limit') || (res.headers.get('x-ratelimit-remaining') === '0'))) {
        throw new Error(buildRateLimitMessage(res, payloadMsg));
      }
      if (res.status === 401) {
        throw new Error('GitHub API authentication failed. Please check your token.');
      }
      throw new Error(message + (payloadMsg ? ` — ${payloadMsg}` : ''));
    }
    return res.json();
  }

  async function fetchText(url, opts = {}) {
    const res = await fetch(url, opts);
    if (!res.ok) throw new Error(`HTTP ${res.status} for ${url}`);
    return res.text();
  }

  function rawUrl(ref, path) {
    // ref can be branch, tag or SHA
    return `https://raw.githubusercontent.com/${OWNER}/${REPO}/${encodeURIComponent(ref)}/${path}`;
  }

  // GitHub API
  async function getRepoMeta() {
    const data = await fetchJSON(`https://api.github.com/repos/${OWNER}/${REPO}`);
    return { default_branch: data.default_branch || 'main' };
  }

  async function getReleases() {
    // Return latest 100 releases
    const releases = await fetchJSON(`https://api.github.com/repos/${OWNER}/${REPO}/releases?per_page=100`);
    return releases.map(r => ({ tag_name: r.tag_name, html_url: r.html_url, draft: r.draft, prerelease: r.prerelease }))
                   .filter(r => !r.draft);
  }

  const commitShaCache = new Map(); // ref -> sha
  async function getCommitShaForRef(ref) {
    if (commitShaCache.has(ref)) return commitShaCache.get(ref);
    // Works for branch or tag or SHA itself
    // GET /commits/{ref}
    const data = await fetchJSON(`https://api.github.com/repos/${OWNER}/${REPO}/commits/${encodeURIComponent(ref)}`);
    commitShaCache.set(ref, data.sha);
    return data.sha;
  }

  async function getRepoTree(ref) {
    // Use trees API for a single call listing all files
    // Resolve ref to SHA first
    const sha = await getCommitShaForRef(ref);
    const cached = state.repoTreeByRef.get(ref);
    if (cached && cached.sha === sha) return cached;
    const data = await fetchJSON(`https://api.github.com/repos/${OWNER}/${REPO}/git/trees/${sha}?recursive=1`);
    const tree = Array.isArray(data.tree) ? data.tree : [];
    const obj = { sha, tree };
    state.repoTreeByRef.set(ref, obj);
    return obj;
  }

  // Local resources
  async function loadDependenciesJson() {
    let deps;
    try {
      deps = await fetchJSON('Dependencies.json');
    } catch (e) {
      deps = await fetchJSON('../../Dependencies/Dependencies.json');
    }
    state.dependencyGraph = deps;
    state.libraryNames = Object.keys(deps).sort();
  }

  // Brief extraction
  async function fetchBriefForLibrary(libraryName, ref) {
    const path = `Documentation/Libraries/${libraryName}.md`;
    const key = `${ref}:${path}`;
    if (state.fileContentCache.has(key)) {
      return extractBrief(state.fileContentCache.get(key));
    }
    try {
      const text = await fetchText(rawUrl(ref, path));
      state.fileContentCache.set(key, text);
      return extractBrief(text);
    } catch {
      return '';
    }
  }

  function extractBrief(markdown) {
    // Look for line starting with @brief and return the rest of the line
    const lines = markdown.split(/\r?\n/);
    for (const line of lines) {
      const trimmed = line.trim();
      if (trimmed.startsWith('@brief')) {
        return trimmed.replace(/^@brief\s*/i, '').trim();
      }
    }
    return '';
  }

  // Amalgamation
 
  function joinPath(dir, rel) {
    if (rel.startsWith('/')) return rel.slice(1);
    const parts = (dir + '/' + rel).split('/');
    const stack = [];
    for (const part of parts) {
      if (part === '' || part === '.') continue;
      if (part === '..') { stack.pop(); continue; }
      stack.push(part);
    }
    return stack.join('/');
  }

  function divider() {
    return '//' + '-'.repeat(118) + '\n';
  }

  function countLoc(content) {
    let code = 0, comments = 0, inBlock = false;
    const lines = content.split(/\r?\n/);
    for (const line of lines) {
      const s = line.trim();
      if (!s) continue;
      if (inBlock) {
        comments++;
        if (s.includes('*/')) inBlock = false;
        continue;
      }
      if (s.startsWith('//')) { comments++; continue; }
      if (s.startsWith('/*')) { comments++; if (!s.includes('*/')) inBlock = true; continue; }
      code++;
    }
    return { code, comments };
  }

  async function fetchFileCached(ref, path) {
    const key = `${ref}:${path}`;
    if (state.fileContentCache.has(key)) return state.fileContentCache.get(key);
    const text = await fetchText(rawUrl(ref, path));
    state.fileContentCache.set(key, text);
    return text;
  }

  async function buildSingleLibraryHeader(libraryName, ref, tree) {
    const ctx = {
      ref,
      orderDir: 'Support/SingleFileLibs',
      dependencies: state.dependencyGraph,
      async listTree() { return tree.tree; },
      async readFile(r, path) { return await fetchFileCached(r, path); },
      async getVersionString(r) {
        const sha = await getCommitShaForRef(r);
        const shortSha = sha.slice(0,7);
        const tag = state.latestReleaseTag || r;
        return `${tag} (${shortSha})`;
      },
    };
    return await buildSingleLibraryHeaderUsingContext(libraryName, ctx);
  }

  // UI helpers
  function renderTableRows() {
    const filterText = (filterInput.value || '').toLowerCase();
    libsTbody.innerHTML = '';
    for (const name of state.libraryNames) {
      const deps = state.dependencyGraph[name]?.all_dependencies || [];
      const brief = state.libraryBriefByName[name] || '';
      if (filterText) {
        const hay = `${name} ${brief} ${deps.join(' ')}`.toLowerCase();
        if (!hay.includes(filterText)) continue;
      }
      const tr = document.createElement('tr');
      const tdName = document.createElement('td');
      const docUrl = `https://pagghiu.github.io/SaneCppLibraries/library_${name.replace(/[A-Z]/g, (m, i) => (i? '_' : '') + m.toLowerCase()).replace(/^_/, '').replace(/__+/g, '_')}.html`;
      tdName.innerHTML = `<strong><a href="${docUrl}" target="_blank" rel="noopener noreferrer">${name}</a></strong>`;
      const tdDesc = document.createElement('td');
      tdDesc.textContent = brief;
      // Dependencies moved below action button in action cell
      const tdAction = document.createElement('td');
      tdAction.className = 'fit';
      const actions = document.createElement('div');
      actions.className = 'row-actions';
      const btn = document.createElement('button');
      btn.className = 'ghost';
      btn.textContent = 'Generate / Download';
      btn.addEventListener('click', async () => {
        btn.disabled = true;
        // Inline progress UI
        const inline = document.createElement('div');
        inline.className = 'inline-progress';
        const row = document.createElement('div'); row.className = 'row';
        const label = document.createElement('div'); label.className = 'label'; label.textContent = 'Preparing…';
        const cancel = document.createElement('button'); cancel.className = 'cancel'; cancel.textContent = 'Cancel';
        const prog = document.createElement('div'); prog.className = 'progress';
        const bar = document.createElement('div'); bar.className = 'progress-bar'; prog.appendChild(bar);
        row.appendChild(label); row.appendChild(cancel);
        inline.appendChild(row); inline.appendChild(prog);
        tdAction.appendChild(inline);
        const controller = new AbortController();
        const signal = controller.signal;
        cancel.addEventListener('click', () => controller.abort());
        try {
          label.textContent = 'Fetching tree…';
          bar.style.width = '8%';
          const tree = await getRepoTree(state.currentRef);
          if (signal.aborted) throw new Error('Cancelled');
          label.textContent = 'Amalgamating…';
          bar.style.width = '60%';
          const content = await buildSingleLibraryHeader(name, state.currentRef, tree);
          if (signal.aborted) throw new Error('Cancelled');
          label.textContent = 'Saving…';
          bar.style.width = '90%';
          triggerDownload(`SaneCpp${name}.h`, content);
          bar.style.width = '100%';
          label.textContent = 'Done';
          setTimeout(() => inline.remove(), 700);
        } catch (e) {
          console.error(e);
          alert(`Failed to build ${name}: ${e.message}`);
        } finally {
          if (inline.parentNode) inline.remove();
          btn.disabled = false;
        }
      });
      actions.appendChild(btn);
      tdAction.appendChild(actions);

      // Dependencies toggle placed below the actions
      if (deps.length > 0) {
        const depToggle = document.createElement('div');
        depToggle.className = 'row-actions';
        const toggleBtn = document.createElement('button');
        toggleBtn.className = 'ghost';
        toggleBtn.textContent = 'Show Dependencies';
        depToggle.appendChild(toggleBtn);

        const depWrap = document.createElement('div');
        depWrap.className = 'dep-badges';
        depWrap.style.display = 'none';
        deps.forEach(d => {
          const span = document.createElement('span');
          span.className = 'dep-badge mono';
          span.textContent = `SaneCpp${d}.h`;
          depWrap.appendChild(span);
        });

        toggleBtn.addEventListener('click', () => {
          const isHidden = depWrap.style.display === 'none';
          depWrap.style.display = isHidden ? '' : 'none';
          toggleBtn.textContent = isHidden ? 'Hide Dependencies' : 'Show Dependencies';
        });

        tdAction.appendChild(depToggle);
        tdAction.appendChild(depWrap);
      }

      tr.appendChild(tdName);
      tr.appendChild(tdDesc);
      tr.appendChild(tdAction);
      libsTbody.appendChild(tr);
    }
    if (!libsTbody.children.length) {
      const tr = document.createElement('tr');
      const td = document.createElement('td');
      td.colSpan = 3;
      td.className = 'center';
      td.textContent = 'No libraries match your filter.';
      tr.appendChild(td);
      libsTbody.appendChild(tr);
    }
  }

  function triggerDownload(filename, text) {
    const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    setTimeout(() => {
      URL.revokeObjectURL(url);
      a.remove();
    }, 0);
  }

  async function downloadAllZip() {
    downloadAllBtn.disabled = true;
    try {
      // Setup cancellation
      if (state.cancelController) state.cancelController.abort();
      state.cancelController = new AbortController();
      const signal = state.cancelController.signal;
      hudShow('Generating all single-file libraries…');

      // Ensure JSZip is available
      let JSZipCtor = window.JSZip;
      if (!JSZipCtor) {
        // Load dynamically as a fallback
        await loadExternalScript('https://cdn.jsdelivr.net/npm/jszip@3.10.1/dist/jszip.min.js');
        JSZipCtor = window.JSZip;
      }
      if (!JSZipCtor) throw new Error('JSZip not available');
      const zip = new JSZipCtor();
      hudUpdate('Fetching repository tree…', 2);
      const tree = await getRepoTree(state.currentRef);
      if (signal.aborted) throw new Error('Cancelled');
      const total = state.libraryNames.length;
      let done = 0;
      for (const name of state.libraryNames) {
        hudUpdate(`Amalgamating ${name}…`, 5 + Math.floor((done / Math.max(1, total)) * 80));
        if (signal.aborted) throw new Error('Cancelled');
        const content = await buildSingleLibraryHeader(name, state.currentRef, tree);
        zip.file(`SaneCpp${name}.h`, content);
        // Throttle slightly to be gentle
        await sleep(50);
        done++;
      }
      // Add README and LICENSE
      hudUpdate('Adding README and LICENSE…', 90);
      try {
        const readme = await fetchText(rawUrl(state.currentRef, 'README.md'));
        zip.file('README.md', readme);
      } catch {}
      try {
        const license = await fetchText(rawUrl(state.currentRef, 'LICENSE.txt'));
        zip.file('LICENSE.txt', license);
      } catch {}

      hudUpdate('Packaging zip…', 96);
      const blob = await zip.generateAsync({ type: 'blob' });
      const nameTag = state.currentRefType === 'tag' ? state.currentRef : (state.latestReleaseTag || state.currentRef);
      triggerDownload(`SaneCppLibraries_SingleFiles_${nameTag}.zip`, blob);
      hudUpdate('Done!', 100);
      setTimeout(hudHide, 600);
    } catch (e) {
      console.error(e);
      alert(`Failed to Download All: ${e.message}`);
    } finally {
      if (state.cancelController) { state.cancelController.abort(); state.cancelController = null; }
      hudHide();
      downloadAllBtn.disabled = false;
    }
  }

  // HUD cancel
  if (hudCancel) {
    hudCancel.addEventListener('click', () => {
      if (state.cancelController) state.cancelController.abort();
      hudHide();
    });
  }

  // Example modal handlers
  if (exampleModalOpen && exampleModal) {
    const show = () => { exampleModal.classList.add('is-open'); };
    const hide = () => { exampleModal.classList.remove('is-open'); };
    exampleModalOpen.addEventListener('click', (e) => {
      e.preventDefault();
      show();
    });
    exampleModal.addEventListener('click', (e) => {
      if (e.target && e.target.dataset && e.target.dataset.close) {
        hide();
      }
    });
    document.addEventListener('keydown', (e) => { if (e.key === 'Escape') hide(); });
  }

  function loadExternalScript(src) {
    return new Promise((resolve, reject) => {
      const s = document.createElement('script');
      s.src = src;
      s.async = true;
      s.onload = () => resolve();
      s.onerror = () => reject(new Error(`Failed to load script: ${src}`));
      document.head.appendChild(s);
    });
  }

  async function populateReleases() {
    releaseSelect.innerHTML = '';
    // Default branch option
    const optLatest = document.createElement('option');
    optLatest.value = `branch:${state.defaultBranch}`;
    optLatest.textContent = `Latest (${state.defaultBranch})`;
    releaseSelect.appendChild(optLatest);

    let releases = [];
    try {
      releases = await getReleases();
    } catch (e) {
      console.warn('Failed to fetch releases', e);
    }
    if (releases.length > 0) {
      state.latestReleaseTag = releases[0].tag_name;
    }
    for (const r of releases) {
      const opt = document.createElement('option');
      opt.value = `tag:${r.tag_name}`;
      opt.textContent = r.tag_name;
      releaseSelect.appendChild(opt);
    }
    releaseSelect.value = `branch:${state.defaultBranch}`;
    releaseLink.href = `https://github.com/${OWNER}/${REPO}/releases`;
  }

  async function updateVersionInfo() {
    try {
      const sha = await getCommitShaForRef(state.currentRef);
      state.latestCommitSha = sha;
      const shortSha = sha.slice(0, 7);
      const tag = state.currentRefType === 'tag' ? state.currentRef : (state.latestReleaseTag || 'unknown');
      versionInfo.textContent = `Version: ${tag} (${shortSha})`;
    } catch {
      versionInfo.textContent = 'Version: unknown';
    }
  }

  async function refreshBriefsForRef(ref) {
    state.libraryBriefByName = {};
    // Fetch all briefs in parallel with a cap
    const concurrency = 6;
    let idx = 0;
    const runNext = async () => {
      while (idx < state.libraryNames.length) {
        const i = idx++;
        const name = state.libraryNames[i];
        try {
          state.libraryBriefByName[name] = await fetchBriefForLibrary(name, ref);
        } catch {
          state.libraryBriefByName[name] = '';
        }
      }
    };
    await Promise.all(Array.from({ length: concurrency }, runNext));
  }

  async function onRefChanged(refType, refName) {
    state.currentRefType = refType;
    state.currentRef = refName;
    await updateVersionInfo();
    await refreshBriefsForRef(refName);
    renderTableRows();
  }

  // Event wiring
  if (themeToggleBtn) {
    themeToggleBtn.addEventListener('click', () => {
      const isDark = document.documentElement.getAttribute('data-theme') !== 'light';
      setTheme(!isDark);
    });
  }
  filterInput.addEventListener('input', () => renderTableRows());
  downloadAllBtn.addEventListener('click', () => downloadAllZip());
  releaseSelect.addEventListener('change', async (e) => {
    const val = e.target.value;
    const [type, name] = val.split(':');
    if (type === 'tag') {
      releaseLink.href = `https://github.com/${OWNER}/${REPO}/releases/tag/${encodeURIComponent(name)}`;
    } else {
      releaseLink.href = `https://github.com/${OWNER}/${REPO}/tree/${encodeURIComponent(name)}`;
    }
    await onRefChanged(type, name);
  });

  if (ghTokenInput) {
    ghTokenInput.value = localStorage.getItem('scl_gh_token') || '';
    ghTokenInput.addEventListener('change', () => {
      const v = ghTokenInput.value.trim();
      if (v) localStorage.setItem('scl_gh_token', v); else localStorage.removeItem('scl_gh_token');
      // Clear caches so subsequent requests use token
      state.repoTreeByRef.clear();
      commitShaCache.clear();
      state.fileContentCache.clear();
      // Force refresh version info
      updateVersionInfo();
    });
  }

  // Boot
  (async function init() {
    initTheme();
    try {
      const meta = await getRepoMeta();
      state.defaultBranch = meta.default_branch || 'main';
      state.currentRef = state.defaultBranch;
    } catch {
      // ignore
    }
    await loadDependenciesJson();
    await populateReleases();
    await onRefChanged('branch', state.defaultBranch);
  })();
})();


