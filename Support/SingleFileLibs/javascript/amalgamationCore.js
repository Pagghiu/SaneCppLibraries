/*
  amalgamationCore.js — Core amalgamation logic (runtime-agnostic)

  Purpose
  - Single source of truth for building single-file headers matching Python output.
  - Used by both the browser app (see app.js) and the local CLI (see cli.js).

  Context interface
  - ctx.ref: selected ref (branch/tag/sha); informational for filesystem
  - ctx.listTree(ref): Promise<Array<{ type: 'blob', path: string }>> — repository tree listing (subset ok)
  - ctx.readFile(ref, path): Promise<string> — reads file text for a repo-relative path
  - ctx.getVersionString(ref): Promise<string> — version line (e.g. "release_xxx (abc1234)")
  - ctx.dependencies: object parsed from Support/SingleFileLibs/Dependencies.json
  - ctx.orderDir: optional, defaults to 'Support/SingleFileLibs' (where SaneCpp<Lib>.json lives)
  - ctx.lineEndings: optional, 'lf' or 'crlf', defaults to 'lf' (or system default for CLI)

  Behavior
  - Honors per-library order file includeOrder/implementationOrder when present
  - Otherwise derives default order: public headers (.h not in Internal/) then .cpp files
  - Shares a processed set across header+implementation to avoid duplicate inlining
  - Inlines only includes from the same library; strips cross-library includes
  - Strips #pragma once and aggregates Copyright/SPDX
  - Exact newline policy per inlined section: rstrip + "\n\n" (matches Python)

  Exported API
  - buildSingleLibraryHeaderUsingContext(libraryName, ctx): Promise<string>
*/

function getLineEnding(ctx) {
  if (ctx.lineEndings === 'crlf') return '\r\n';
  if (ctx.lineEndings === 'lf') return '\n';
  // For CLI: use OS default
  if (typeof process !== 'undefined') {
    return process.platform === 'win32' ? '\r\n' : '\n';
  }
  return '\n'; // Default to LF for browser
}

export function divider() {
  return '//' + '-'.repeat(118) + '\n';
}

export function countLoc(content) {
  let code = 0, comments = 0, inBlock = false;
  const lines = content.split(/\r?\n/);
  for (const line of lines) {
    const s = line.trim();
    if (!s) { comments++; continue; }
    if (inBlock) { comments++; if (s.includes('*/')) inBlock = false; continue; }
    if (s.startsWith('//')) { comments++; continue; }
    if (s.startsWith('/*')) { comments++; if (!s.includes('*/')) inBlock = true; continue; }
    code++;
  }
  comments--; // end of file newline
  return { code, comments };
}

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

function libraryNameFromPath(path) {
  const parts = path.split('/');
  if ((parts[0] === 'Libraries' || parts[0] === 'LibrariesExtra') && parts.length > 1) return parts[1];
  return null;
}

function isPublicHeader(path, libraryName) {
  if (!path.endsWith('.h')) return false;
  if (!path.startsWith(`Libraries/${libraryName}/`)) return false;
  return !path.includes('/Internal/');
}

async function processFileRecursively({ path, processedSet, ctx, targetLibrary, treePathsSet, authorsInfo, spdxSet }) {
  const normalizedPath = path;
  if (processedSet.has(normalizedPath)) return '';
  processedSet.add(normalizedPath);

  let content = '';
  const pathParts = normalizedPath.split('/');
  if ((pathParts[0] === 'Libraries' || pathParts[0] === 'LibrariesExtra') && pathParts.length > 2) {
    const relativeToLibs = pathParts.slice(1).join('/');
    const eol = getLineEnding(ctx);
    content += divider().replace(/\n$/, eol);
    content += `// ${relativeToLibs}${eol}`;
    content += divider().replace(/\n$/, eol);
  }

  let fileText;
  try {
    fileText = await ctx.readFile(ctx.ref, normalizedPath);
  } catch (e) {
    return '';
  }
  const eol = getLineEnding(ctx);

  const lines = fileText.split(/\r?\n/);
  for (let idx = 0; idx < lines.length; idx++) {
    const line = lines[idx];
    const stripped = line.trim();

    const cMatch = stripped.match(/^\/\/\s*Copyright \(c\)\s*(.*)$/);
    if (cMatch) {
      const author = cMatch[1].trim();
      authorsInfo.set(author, (authorsInfo.get(author) || 0) + 1);
    }
    const spdxMatch = stripped.match(/^\/\/\s*SPDX-License-Identifier:\s*(.*)$/);
    if (spdxMatch) spdxSet.add(spdxMatch[1].trim());

    if (stripped.startsWith('#pragma once')) continue;

    const inc = stripped.match(/^#include\s+"(.*)"/);
    if (inc) {
      const incToken = inc[1];
      const currentDir = normalizedPath.substring(0, normalizedPath.lastIndexOf('/'));
      const includedPath = joinPath(currentDir, incToken);
      if (treePathsSet.has(includedPath) && (includedPath.startsWith('Libraries/') || includedPath.startsWith('LibrariesExtra/'))) {
        const includedLib = libraryNameFromPath(includedPath);
        if (includedLib === targetLibrary) {
          content += await processFileRecursively({ path: includedPath, processedSet, ctx, targetLibrary, treePathsSet, authorsInfo, spdxSet });
        }
        continue;
      }
    }

    content += line + eol;
  }
  // Match Python: trim trailing whitespace and append exactly two newlines
  content = content.replace(/\s+$/, '') + eol + eol;
  return content;
}

async function amalgamateFilesRecursively({ orderList, ctx, targetLibrary, treePathsSet, authorsInfo, spdxSet, processedSet }) {
  const processed = processedSet || new Set();
  let result = '';
  function resolveToPath(filename) {
    for (const p of treePathsSet) {
      if (!p.startsWith(`Libraries/${targetLibrary}/`)) continue;
      if (p.endsWith(filename) || p === filename) return p;
    }
    return null;
  }
  for (const entry of orderList) {
    const resolved = resolveToPath(entry);
    if (!resolved) continue;
    result += await processFileRecursively({ path: resolved, processedSet: processed, ctx, targetLibrary, treePathsSet, authorsInfo, spdxSet });
  }
  return result;
}

export async function buildSingleLibraryHeaderUsingContext(libraryName, ctx) {
  const treeEntries = await ctx.listTree(ctx.ref);
  const treePaths = treeEntries.filter(e => e.type === 'blob').map(e => e.path);
  const treePathsSet = new Set(treePaths);

  // Order file (optional)
  const orderPath = `${ctx.orderDir || 'Support/SingleFileLibs'}/SaneCpp${libraryName}.json`;
  let includeOrder = null;
  let implementationOrder = null;
  if (treePathsSet.has(orderPath)) {
    try {
      const orderText = await ctx.readFile(ctx.ref, orderPath);
      const order = JSON.parse(orderText);
      includeOrder = Array.isArray(order.includeOrder) ? order.includeOrder : [];
      implementationOrder = Array.isArray(order.implementationOrder) ? order.implementationOrder : [];
    } catch {}
  }

  // Derive default orders when order file is missing
  const libFiles = treePaths.filter(p => p.startsWith(`Libraries/${libraryName}/`) && (p.endsWith('.h') || p.endsWith('.cpp') || p.endsWith('.inl')));
  const headerFiles = includeOrder ? includeOrder : libFiles.filter(p => isPublicHeader(p, libraryName)).map(p => p.substring(p.lastIndexOf('/')+1));
  const sourceFiles = implementationOrder ? implementationOrder : libFiles.filter(p => p.endsWith('.cpp')).map(p => p.substring(p.lastIndexOf('/')+1));

  const authorsInfo = new Map();
  const spdxSet = new Set();
  const processed = new Set();
  const headers = await amalgamateFilesRecursively({ orderList: headerFiles, ctx, targetLibrary: libraryName, treePathsSet, authorsInfo, spdxSet, processedSet: processed });
  const sources = await amalgamateFilesRecursively({ orderList: sourceFiles, ctx, targetLibrary: libraryName, treePathsSet, authorsInfo, spdxSet, processedSet: processed });

  const headerLoc = countLoc(headers);
  const sourcesLoc = countLoc(sources);

  const versionString = (await ctx.getVersionString(ctx.ref)) || 'unknown';
  const DIV = divider();
  const deps = (ctx.dependencies && ctx.dependencies[libraryName] && ctx.dependencies[libraryName].all_dependencies) || [];

  const eol = getLineEnding(ctx);
  const lines = [];
  lines.push(DIV.replace(/\n$/, eol));
  lines.push(`// SaneCpp${libraryName}.h - Sane C++ ${libraryName} Library (single file build)${eol}`);
  lines.push(DIV.replace(/\n$/, eol));
  if (deps.length) lines.push(`// Dependencies:       ${deps.map(d => `SaneCpp${d}.h`).join(', ')}${eol}`); else lines.push(`// Dependencies:       None${eol}`);
  lines.push(`// Version:            ${versionString}${eol}`);
  lines.push(`// LOC header:         ${headerLoc.code} (code) + ${headerLoc.comments} (comments)${eol}`);
  lines.push(`// LOC implementation: ${sourcesLoc.code} (code) + ${sourcesLoc.comments} (comments)${eol}`);
  lines.push(`// Documentation:      https://pagghiu.github.io/SaneCppLibraries${eol}`);
  lines.push(`// Source Code:        https://github.com/pagghiu/SaneCppLibraries${eol}`);
  lines.push(DIV.replace(/\n$/, eol));
  lines.push(`// All copyrights and SPDX information for this library (each amalgamated section has its own copyright attributions):${eol}`);
  const sortedAuthors = Array.from(authorsInfo.entries()).sort((a, b) => b[1] - a[1]);
  for (const [author] of sortedAuthors) lines.push(`// Copyright (c) ${author}${eol}`);
  if (spdxSet.size > 0) lines.push(`// SPDX-License-Identifier: ${Array.from(spdxSet).sort().join(', ')}${eol}`);
  lines.push(DIV.replace(/\n$/, eol));
  for (const dep of deps) lines.push(`#include "SaneCpp${dep}.h"${eol}`);
  if (deps.length) lines.push(eol);

  const defineGuard = `SANE_CPP_${libraryName.toUpperCase()}_HEADER`;
  lines.push(`#if !defined(${defineGuard})${eol}`);
  lines.push(`#define ${defineGuard} 1${eol}`);
  lines.push(headers);
  lines.push(`${eol}#endif // ${defineGuard}${eol}`);

  if (sources.trim().length > 0) {
    const implGuard = `SANE_CPP_${libraryName.toUpperCase()}_IMPLEMENTATION`;
    lines.push(`#if defined(SANE_CPP_IMPLEMENTATION) && !defined(${implGuard})${eol}`);
    lines.push(`#define ${implGuard} 1${eol}`);
    lines.push(sources);
    lines.push(`${eol}#endif // ${implGuard}${eol}`);
  }

  return lines.join('');
}


