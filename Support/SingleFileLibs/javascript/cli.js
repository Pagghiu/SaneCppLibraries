#!/usr/bin/env node
/*
  cli.js — Filesystem CLI using amalgamationCore.js

  Purpose
  - Runs the same core logic locally to generate single-file headers from the repo on disk
  - Enables CI diffing against the Python amalgamator outputs

  Usage examples:
    node Support/SingleFileLibs/cli.js --repo-root . --ref HEAD --library Time --out _Build/_SingleFileLibraries
    bun  Support/SingleFileLibs/cli.js --repo-root . --ref release_2025_07 --all --out _Build/_SingleFileLibraries

  Notes:
  - Node: CommonJS wrapper dynamically imports the ESM core module (Node >=16)
  - Bun: works out of the box
  - Outputs single-file headers only (no test cpp)
  - Scans only Libraries/, LibrariesExtra/ and Support/SingleFileLibs (for order files)
*/

'use strict';
const { readFile: fsReadFile, writeFile: fsWriteFile } = require('node:fs/promises');
const { existsSync, mkdirSync, readdirSync, statSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { pathToFileURL } = require('node:url');

function parseArgs(argv) {
  const args = { repoRoot: '.', ref: 'HEAD', outDir: '_Build/_SingleFileLibraries', all: false, library: null };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--repo-root') args.repoRoot = argv[++i];
    else if (a === '--ref') args.ref = argv[++i];
    else if (a === '--out') args.outDir = argv[++i];
    else if (a === '--all') args.all = true;
    else if (a === '--library') args.library = argv[++i];
    else if (a === '-h' || a === '--help') {
      console.log('Usage: cli.js --repo-root <path> --ref <branch|tag|sha> (--all | --library <name>) --out <dir>');
      process.exit(0);
    }
  }
  if (!args.all && !args.library) {
    console.error('Error: specify --all or --library <name>');
    process.exit(1);
  }
  return args;
}

async function readText(path) { return await fsReadFile(path, 'utf8'); }
async function writeText(path, content) { await fsWriteFile(path, content); }
async function loadJSON(path) { return JSON.parse(await readText(path)); }
function listFilesRecursive(root) {
  const results = [];
  function walk(dir) {
    let names;
    try { names = readdirSync(dir).sort(); } catch { return; }
    for (const name of names) {
      const full = join(dir, name);
      let st;
      try { st = statSync(full); } catch { continue; }
      if (st.isDirectory()) walk(full); else results.push(full);
    }
  }
  walk(root);
  return results;
}

async function main() {
  const args = parseArgs(process.argv);
  const repoRoot = resolve(args.repoRoot);
  const ref = args.ref; // for filesystem backend, ref is informational only
  const outDir = resolve(args.outDir);
  if (!existsSync(outDir)) mkdirSync(outDir, { recursive: true });

  const dependencies = await loadJSON(join(repoRoot, 'Support', 'Dependencies', 'Dependencies.json'));

  // Dynamic import of ESM core
  const coreUrl = pathToFileURL(join(__dirname, 'amalgamationCore.js')).href;
  const { buildSingleLibraryHeaderUsingContext } = await import(coreUrl);

  // Filesystem backend context
  const ctx = {
    ref,
    orderDir: 'Support/SingleFileLibs',
    dependencies,
    // Use system-appropriate line endings
    lineEndings: process.platform === 'win32' ? 'crlf' : 'lf',
    async listTree() {
      const roots = [
        join(repoRoot, 'Libraries'),
        join(repoRoot, 'LibrariesExtra'),
        join(repoRoot, 'Support', 'SingleFileLibs'), // for per-library order JSON files
      ];
      const files = [];
      for (const r of roots) {
        if (!existsSync(r)) continue;
        files.push(...listFilesRecursive(r));
      }
      const rel = files.map(f => ({ type: 'blob', path: f.substring(repoRoot.length + 1).replaceAll('\\', '/') }));
      return rel;
    },
    async readFile(_ref, path) {
      return await readText(join(repoRoot, path));
    },
    async getVersionString(_ref) {
      // Try to mimic Python version: <latest tag> (<short sha>)
      // For simplicity here, return just the short HEAD sha via git, falling back to 'unknown'
      try {
        const { execSync } = require('node:child_process');
        const tag = execSync('git describe --tags --abbrev=0', { cwd: repoRoot, stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim();
        const sha = execSync('git rev-parse --short HEAD', { cwd: repoRoot, stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim();
        return `${tag} (${sha})`;
      } catch { return 'unknown'; }
    },
  };

  const libraries = args.all ? Object.keys(dependencies) : [args.library];
  for (const lib of libraries) {
    process.stdout.write(`Amalgamating ${lib}... `);
    const content = await buildSingleLibraryHeaderUsingContext(lib, ctx);
    const outPath = join(outDir, `SaneCpp${lib}.h`);
    await writeText(outPath, content);
    console.log('done');
  }
}

main().catch(err => {
  console.error(err);
  process.exit(1);
});


