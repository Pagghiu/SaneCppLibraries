/* Offline response files: no server, library dependency, or implicit authorization. */
'use strict';
const SCResponseFiles = (() => {
    const actions = ['', 'implement', 'explore', 'opinion', 'defer'];
    function entries(value) {
        if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Invalid responses');
        const result = {};
        for (const [id, entry] of Object.entries(value)) {
            if (!/^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$/.test(id) || ['__proto__', 'constructor', 'prototype'].includes(id) || !entry || !actions.includes(entry.action ?? '') || typeof (entry.note ?? '') !== 'string') throw new Error('Invalid response: ' + id);
            result[id] = {action: entry.action ?? '', note: entry.note ?? '', updated_at: typeof entry.updated_at === 'string' ? entry.updated_at : ''};
        }
        return result;
    }
    function normalize(document, project) {
        if (document?.schema !== 1 || document.project !== project) throw new Error('Response file belongs to another project or has an unsupported format');
        return entries(document.responses);
    }
    function merge(disk, draft, migrateBrowserDraft = false) {
        const result = entries(disk);
        for (const [id, entry] of Object.entries(entries(draft))) {
            // Stamp legacy browser drafts only at startup, not old embedded disk records.
            if (migrateBrowserDraft && !entry.updated_at) entry.updated_at = new Date().toISOString();
            if (!result[id] || entry.updated_at >= (result[id].updated_at || '')) result[id] = entry;
        }
        return result;
    }
    function connect(options) {
        let handle = null, timer, chain = Promise.resolve(), version = 0;
        const direct = typeof window.showSaveFilePicker === 'function' && typeof window.showOpenFilePicker === 'function';
        const types = [{description: 'Simplification responses', accept: {'application/json': ['.json']}}];
        const snapshot = () => JSON.stringify({schema: 1, project: options.project, revision: options.revision, saved_at: new Date().toISOString(), responses: entries(options.get())}, null, 2) + '\n';
        const status = options.status;
        status(direct ? 'Choose a response file once per session to enable autosave. No webserver needed.' : 'Direct saving is unavailable in this browser. Download responses into _Plans/Simplification and load them when reopening. No webserver needed.');
        function download() {
            const url = URL.createObjectURL(new Blob([snapshot()], {type: 'application/json'}));
            const link = document.createElement('a');
            link.href = url; link.download = 'simplification-responses.json'; link.click();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
            status('Download requested. Save as _Plans/Simplification/simplification-responses.json; this browser cannot confirm its destination.');
        }
        function write() {
            clearTimeout(timer);
            chain = chain.then(async () => {
                if (!handle) return;
                const target = handle, writingVersion = version, content = snapshot();
                try {
                    const stream = await target.createWritable();
                    await stream.write(content); await stream.close();
                    status(writingVersion === version ? 'Saved to ' + target.name + ' · Agents can read this file. Actions still require chat authorization.' : 'Newer edits pending file save…');
                } catch (error) {
                    // Stop autosaving after failure; browser draft remains recoverable.
                    if (handle === target) handle = null;
                    status('File save failed: ' + error.message + '. Your browser draft remains; reconnect or download responses.');
                }
            });
            return chain;
        }
        function changed() {
            version++;
            if (handle) { status('Saving responses…'); clearTimeout(timer); timer = setTimeout(write, 400); }
            else status('Browser draft only — save or download responses to share them with agents.');
        }
        async function importFile(file) {
            try {
                const loaded = normalize(JSON.parse(await file.text()), options.project);
                options.set(merge(loaded, options.get()));
                status('Loaded ' + file.name + '; newer browser edits retained. Save responses to share the merged result.');
                return true;
            } catch (error) { status('Could not load responses: ' + error.message); return false; }
        }
        async function open() {
            if (!direct) { status('Use Load responses to choose a JSON file, then Download responses to save updates.'); return; }
            try {
                const [chosen] = await window.showOpenFilePicker({types, multiple: false});
                clearTimeout(timer); await chain; handle = null;
                if (!await importFile(await chosen.getFile())) return;
                handle = chosen;
                await write();
            } catch (error) { if (error.name !== 'AbortError') status('Could not connect file: ' + error.message); }
        }
        async function save() {
            if (!direct) { download(); return; }
            try {
                if (!handle) handle = await window.showSaveFilePicker({suggestedName: 'simplification-responses.json', types});
                await write();
            } catch (error) { if (error.name !== 'AbortError') status('Could not save file: ' + error.message); }
        }
        return {changed, open, save, download, importFile, flush: write};
    }
    return {normalize, merge, connect};
})();
