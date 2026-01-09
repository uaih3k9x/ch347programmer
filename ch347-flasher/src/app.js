// CH347 BIOS Flasher - Frontend JavaScript

const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;
const { open, save } = window.__TAURI__.dialog;
const { readFile } = window.__TAURI__.fs;

// State
let isConnected = false;
let chipDetected = false;
let currentFile = null;
let isBusy = false;

// HEX Viewer State
let fileData = null;
let hexCurrentPage = 0;
const BYTES_PER_ROW = 16;
const ROWS_PER_PAGE = 32;
const BYTES_PER_PAGE = BYTES_PER_ROW * ROWS_PER_PAGE;

// DOM Elements
const elements = {
    connectionStatus: document.getElementById('connectionStatus'),
    deviceInfo: document.getElementById('deviceInfo'),
    chipInfo: document.getElementById('chipInfo'),
    filePath: document.getElementById('filePath'),
    progressFill: document.getElementById('progressFill'),
    progressOperation: document.getElementById('progressOperation'),
    progressPercent: document.getElementById('progressPercent'),
    logContainer: document.getElementById('logContainer'),

    btnConnect: document.getElementById('btnConnect'),
    btnDisconnect: document.getElementById('btnDisconnect'),
    btnDetect: document.getElementById('btnDetect'),
    btnBrowse: document.getElementById('btnBrowse'),
    btnRead: document.getElementById('btnRead'),
    btnWrite: document.getElementById('btnWrite'),
    btnVerify: document.getElementById('btnVerify'),
    btnErase: document.getElementById('btnErase'),
    verifyAfterWrite: document.getElementById('verifyAfterWrite'),

    // HEX Viewer
    hexContent: document.getElementById('hexContent'),
    hexFileName: document.getElementById('hexFileName'),
    hexFileSize: document.getElementById('hexFileSize'),
    hexPageInfo: document.getElementById('hexPageInfo'),
    hexSelectionInfo: document.getElementById('hexSelectionInfo'),
    hexGotoAddress: document.getElementById('hexGotoAddress'),
    hexSearch: document.getElementById('hexSearch'),
    btnHexFirst: document.getElementById('btnHexFirst'),
    btnHexPrev: document.getElementById('btnHexPrev'),
    btnHexNext: document.getElementById('btnHexNext'),
    btnHexLast: document.getElementById('btnHexLast'),
    btnHexGoto: document.getElementById('btnHexGoto'),
    btnHexSearch: document.getElementById('btnHexSearch'),
};

// Initialize
async function init() {
    // Set up event listeners
    elements.btnConnect.addEventListener('click', connect);
    elements.btnDisconnect.addEventListener('click', disconnect);
    elements.btnDetect.addEventListener('click', detectChip);
    elements.btnBrowse.addEventListener('click', browseFile);
    elements.btnRead.addEventListener('click', readFlash);
    elements.btnWrite.addEventListener('click', writeFlash);
    elements.btnVerify.addEventListener('click', verifyFlash);
    elements.btnErase.addEventListener('click', eraseChip);

    // Tab switching
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', () => switchTab(btn.dataset.tab));
    });

    // HEX Viewer controls
    elements.btnHexFirst.addEventListener('click', () => hexGoToPage(0));
    elements.btnHexPrev.addEventListener('click', () => hexGoToPage(hexCurrentPage - 1));
    elements.btnHexNext.addEventListener('click', () => hexGoToPage(hexCurrentPage + 1));
    elements.btnHexLast.addEventListener('click', () => hexGoToPage(getHexTotalPages() - 1));
    elements.btnHexGoto.addEventListener('click', hexGotoAddress);
    elements.btnHexSearch.addEventListener('click', hexSearch);

    // Enter key for hex inputs
    elements.hexGotoAddress.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') hexGotoAddress();
    });
    elements.hexSearch.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') hexSearch();
    });

    // Listen for progress events from backend
    await listen('progress', (event) => {
        updateProgress(event.payload);
    });

    log('Ready. Click "Connect" to start.', 'info');
}

// Tab switching
function switchTab(tabName) {
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tab === tabName);
    });
    document.querySelectorAll('.tab-content').forEach(content => {
        content.classList.toggle('active', content.id === `tab-${tabName}`);
    });
}

// Logging
function log(message, type = 'info') {
    const entry = document.createElement('div');
    entry.className = `log-entry ${type}`;
    entry.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
    elements.logContainer.appendChild(entry);
    elements.logContainer.scrollTop = elements.logContainer.scrollHeight;
}

// Update UI state
function updateUI() {
    // Connection status
    const statusDot = elements.connectionStatus.querySelector('.status-dot');
    const statusText = elements.connectionStatus.querySelector('.status-text');

    if (isConnected) {
        statusDot.classList.remove('disconnected');
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';
    } else {
        statusDot.classList.remove('connected');
        statusDot.classList.add('disconnected');
        statusText.textContent = 'Disconnected';
    }

    // Buttons
    elements.btnConnect.disabled = isConnected || isBusy;
    elements.btnDisconnect.disabled = !isConnected || isBusy;
    elements.btnDetect.disabled = !isConnected || isBusy;

    const opEnabled = isConnected && chipDetected && !isBusy;
    elements.btnRead.disabled = !opEnabled;
    elements.btnWrite.disabled = !opEnabled || !currentFile;
    elements.btnVerify.disabled = !opEnabled || !currentFile;
    elements.btnErase.disabled = !opEnabled;
}

// Update progress bar
function updateProgress(info) {
    const percent = Math.round(info.percent);
    elements.progressFill.style.width = `${percent}%`;
    elements.progressOperation.textContent = info.operation;
    elements.progressPercent.textContent = `${percent}%`;
}

// Reset progress
function resetProgress() {
    elements.progressFill.style.width = '0%';
    elements.progressOperation.textContent = 'Idle';
    elements.progressPercent.textContent = '0%';
}

// Connect to device
async function connect() {
    log('Connecting to CH347...', 'info');
    isBusy = true;
    updateUI();

    try {
        const result = await invoke('connect');

        if (result.success) {
            isConnected = true;
            const data = result.data;

            elements.deviceInfo.innerHTML = `
                <div class="info-row">
                    <span class="info-label">Status:</span>
                    <span class="info-value">Connected</span>
                </div>
                <div class="info-row">
                    <span class="info-label">VID:PID:</span>
                    <span class="info-value">${data.vid.toString(16).toUpperCase()}:${data.pid.toString(16).toUpperCase()}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Device:</span>
                    <span class="info-value">${data.name || 'CH347'}</span>
                </div>
            `;

            log('Connected successfully!', 'success');
        } else {
            log(`Connection failed: ${result.error}`, 'error');
        }
    } catch (e) {
        log(`Error: ${e}`, 'error');
    }

    isBusy = false;
    updateUI();
}

// Disconnect from device
async function disconnect() {
    log('Disconnecting...', 'info');

    try {
        await invoke('disconnect');
        isConnected = false;
        chipDetected = false;

        elements.deviceInfo.innerHTML = '<p class="placeholder">No device connected</p>';
        elements.chipInfo.innerHTML = '<p class="placeholder">No chip detected</p>';

        log('Disconnected', 'info');
    } catch (e) {
        log(`Error: ${e}`, 'error');
    }

    updateUI();
}

// Detect flash chip
async function detectChip() {
    log('Detecting flash chip...', 'info');
    isBusy = true;
    updateUI();

    try {
        const result = await invoke('detect_chip');

        if (result.success && result.data.detected) {
            chipDetected = true;
            const chip = result.data;

            elements.chipInfo.innerHTML = `
                <div class="info-row">
                    <span class="info-label">Chip:</span>
                    <span class="info-value">${chip.name}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Manufacturer:</span>
                    <span class="info-value">${chip.manufacturer}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">JEDEC ID:</span>
                    <span class="info-value">${chip.jedec_id}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Size:</span>
                    <span class="info-value">${chip.size_str}</span>
                </div>
            `;

            log(`Detected: ${chip.manufacturer} ${chip.name} (${chip.size_str})`, 'success');
        } else {
            log(`Detection failed: ${result.error || 'Unknown error'}`, 'error');
        }
    } catch (e) {
        log(`Error: ${e}`, 'error');
    }

    isBusy = false;
    updateUI();
}

// Browse for file
async function browseFile() {
    try {
        const selected = await open({
            multiple: false,
            filters: [{
                name: 'BIOS Files',
                extensions: ['bin', 'rom', 'fd', 'cap']
            }, {
                name: 'All Files',
                extensions: ['*']
            }]
        });

        if (selected) {
            currentFile = selected;
            elements.filePath.value = selected;
            log(`Selected file: ${selected}`, 'info');
            updateUI();

            // Load file for HEX viewer
            await loadFileForHex(selected);
        }
    } catch (e) {
        log(`Error selecting file: ${e}`, 'error');
    }
}

// Load file for HEX viewer
async function loadFileForHex(filePath) {
    try {
        fileData = await readFile(filePath);
        hexCurrentPage = 0;

        const fileName = filePath.split('/').pop().split('\\').pop();
        elements.hexFileName.textContent = fileName;
        elements.hexFileSize.textContent = formatFileSize(fileData.length);

        renderHexView();
        updateHexPagination();

        log(`Loaded ${fileName} (${formatFileSize(fileData.length)}) for HEX view`, 'info');
    } catch (e) {
        log(`Error loading file for HEX view: ${e}`, 'error');
        fileData = null;
        elements.hexFileName.textContent = 'Error loading file';
        elements.hexFileSize.textContent = '';
        elements.hexContent.innerHTML = '<div class="hex-placeholder">Failed to load file</div>';
    }
}

// Format file size
function formatFileSize(bytes) {
    if (bytes >= 1024 * 1024) {
        return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
    } else if (bytes >= 1024) {
        return `${(bytes / 1024).toFixed(2)} KB`;
    }
    return `${bytes} B`;
}

// Get total HEX pages
function getHexTotalPages() {
    if (!fileData) return 0;
    return Math.ceil(fileData.length / BYTES_PER_PAGE);
}

// Render HEX view
function renderHexView() {
    if (!fileData) {
        elements.hexContent.innerHTML = '<div class="hex-placeholder">Select a file to view its contents</div>';
        return;
    }

    const startOffset = hexCurrentPage * BYTES_PER_PAGE;
    const endOffset = Math.min(startOffset + BYTES_PER_PAGE, fileData.length);

    let html = '';

    for (let offset = startOffset; offset < endOffset; offset += BYTES_PER_ROW) {
        const rowEnd = Math.min(offset + BYTES_PER_ROW, fileData.length);
        const rowBytes = fileData.slice(offset, rowEnd);

        // Address
        html += `<div class="hex-row">`;
        html += `<span class="hex-addr">${offset.toString(16).toUpperCase().padStart(8, '0')}</span>`;

        // Bytes
        html += `<span class="hex-bytes">`;
        for (let i = 0; i < BYTES_PER_ROW; i++) {
            if (i < rowBytes.length) {
                const byte = rowBytes[i];
                const byteHex = byte.toString(16).toUpperCase().padStart(2, '0');
                let className = 'hex-byte';
                if (byte === 0x00) className += ' zero';
                else if (byte === 0xFF) className += ' ff';
                html += `<span class="${className}" data-offset="${offset + i}">${byteHex}</span>`;
            } else {
                html += `<span class="hex-byte">  </span>`;
            }
        }
        html += `</span>`;

        // ASCII
        html += `<span class="hex-ascii">`;
        for (let i = 0; i < rowBytes.length; i++) {
            const byte = rowBytes[i];
            if (byte >= 0x20 && byte <= 0x7E) {
                // Escape HTML special chars
                const char = String.fromCharCode(byte);
                const escaped = char.replace(/[<>&"']/g, c => ({
                    '<': '&lt;', '>': '&gt;', '&': '&amp;', '"': '&quot;', "'": '&#39;'
                }[c]));
                html += escaped;
            } else {
                html += `<span class="non-printable">.</span>`;
            }
        }
        html += `</span>`;

        html += `</div>`;
    }

    elements.hexContent.innerHTML = html;

    // Add click handlers for bytes
    elements.hexContent.querySelectorAll('.hex-byte[data-offset]').forEach(el => {
        el.addEventListener('click', () => {
            const offset = parseInt(el.dataset.offset);
            selectHexByte(offset);
        });
    });
}

// Select a byte in HEX view
function selectHexByte(offset) {
    // Remove previous selection
    elements.hexContent.querySelectorAll('.hex-byte.selected').forEach(el => {
        el.classList.remove('selected');
    });

    // Add new selection
    const el = elements.hexContent.querySelector(`.hex-byte[data-offset="${offset}"]`);
    if (el) {
        el.classList.add('selected');
    }

    // Update selection info
    if (fileData && offset < fileData.length) {
        const byte = fileData[offset];
        elements.hexSelectionInfo.textContent =
            `Offset: 0x${offset.toString(16).toUpperCase().padStart(8, '0')} | ` +
            `Value: 0x${byte.toString(16).toUpperCase().padStart(2, '0')} (${byte})`;
    }
}

// Update HEX pagination
function updateHexPagination() {
    const totalPages = getHexTotalPages();

    elements.hexPageInfo.textContent = `Page ${hexCurrentPage + 1} / ${totalPages || 1}`;

    elements.btnHexFirst.disabled = !fileData || hexCurrentPage === 0;
    elements.btnHexPrev.disabled = !fileData || hexCurrentPage === 0;
    elements.btnHexNext.disabled = !fileData || hexCurrentPage >= totalPages - 1;
    elements.btnHexLast.disabled = !fileData || hexCurrentPage >= totalPages - 1;
}

// Go to HEX page
function hexGoToPage(page) {
    const totalPages = getHexTotalPages();
    if (page < 0) page = 0;
    if (page >= totalPages) page = totalPages - 1;

    hexCurrentPage = page;
    renderHexView();
    updateHexPagination();
}

// Go to address
function hexGotoAddress() {
    if (!fileData) return;

    let addr = elements.hexGotoAddress.value.trim();

    // Parse hex or decimal
    let offset;
    if (addr.toLowerCase().startsWith('0x')) {
        offset = parseInt(addr, 16);
    } else {
        offset = parseInt(addr, 16); // Default to hex
    }

    if (isNaN(offset) || offset < 0 || offset >= fileData.length) {
        log(`Invalid address: ${addr}`, 'warning');
        return;
    }

    // Calculate page
    const page = Math.floor(offset / BYTES_PER_PAGE);
    hexGoToPage(page);

    // Select the byte
    setTimeout(() => selectHexByte(offset), 50);
}

// Search in HEX
function hexSearch() {
    if (!fileData) return;

    const query = elements.hexSearch.value.trim();
    if (!query) return;

    // Try to parse as hex bytes first
    let searchBytes = null;

    // Check if it looks like hex (contains only hex chars and spaces)
    if (/^[0-9A-Fa-f\s]+$/.test(query)) {
        const hexParts = query.split(/\s+/).filter(p => p.length > 0);
        if (hexParts.every(p => p.length <= 2)) {
            searchBytes = new Uint8Array(hexParts.map(p => parseInt(p, 16)));
        }
    }

    // If not hex, treat as ASCII text
    if (!searchBytes) {
        searchBytes = new TextEncoder().encode(query);
    }

    // Search from current position
    const startPos = hexCurrentPage * BYTES_PER_PAGE;

    for (let i = startPos; i < fileData.length - searchBytes.length + 1; i++) {
        let found = true;
        for (let j = 0; j < searchBytes.length; j++) {
            if (fileData[i + j] !== searchBytes[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            // Go to page containing this offset
            const page = Math.floor(i / BYTES_PER_PAGE);
            hexGoToPage(page);
            setTimeout(() => selectHexByte(i), 50);
            log(`Found at offset 0x${i.toString(16).toUpperCase()}`, 'success');
            return;
        }
    }

    log('Pattern not found', 'warning');
}

// Read flash to file
async function readFlash() {
    try {
        const savePath = await save({
            filters: [{
                name: 'Binary Files',
                extensions: ['bin']
            }]
        });

        if (!savePath) return;

        log('Reading flash...', 'info');
        isBusy = true;
        updateUI();
        resetProgress();

        const result = await invoke('read_flash', { path: savePath });

        if (result.success) {
            log(`Read complete! Saved to: ${savePath}`, 'success');

            // Load the read file into HEX viewer
            await loadFileForHex(savePath);
            currentFile = savePath;
            elements.filePath.value = savePath;
        } else {
            log(`Read failed: ${result.error}`, 'error');
        }
    } catch (e) {
        log(`Error: ${e}`, 'error');
    }

    isBusy = false;
    updateUI();
}

// Write flash from file
async function writeFlash() {
    if (!currentFile) {
        log('Please select a file first', 'warning');
        return;
    }

    const confirm = window.confirm(
        'WARNING: This will erase and overwrite the flash chip!\n\n' +
        'Make sure you have a backup of the original content.\n\n' +
        'Continue?'
    );

    if (!confirm) return;

    log('Writing flash...', 'info');
    isBusy = true;
    updateUI();
    resetProgress();

    try {
        const verify = elements.verifyAfterWrite.checked;
        const result = await invoke('write_flash', {
            path: currentFile,
            verify: verify
        });

        if (result.success) {
            log('Write complete!' + (verify ? ' Verification passed.' : ''), 'success');
        } else {
            log(`Write failed: ${result.error}`, 'error');
        }
    } catch (e) {
        log(`Error: ${e}`, 'error');
    }

    isBusy = false;
    updateUI();
}

// Verify flash against file
async function verifyFlash() {
    if (!currentFile) {
        log('Please select a file first', 'warning');
        return;
    }

    log('Verifying flash...', 'info');
    isBusy = true;
    updateUI();
    resetProgress();

    try {
        const result = await invoke('verify_flash', { path: currentFile });

        if (result.success) {
            if (result.data === true) {
                log('Verification PASSED!', 'success');
            } else {
                log('Verification FAILED! Content does not match.', 'error');
            }
        } else {
            log(`Verify failed: ${result.error}`, 'error');
        }
    } catch (e) {
        log(`Error: ${e}`, 'error');
    }

    isBusy = false;
    updateUI();
}

// Erase entire chip
async function eraseChip() {
    const confirm = window.confirm(
        'WARNING: This will ERASE the entire flash chip!\n\n' +
        'All data will be lost.\n\n' +
        'Continue?'
    );

    if (!confirm) return;

    log('Erasing chip...', 'info');
    isBusy = true;
    updateUI();
    resetProgress();

    try {
        const result = await invoke('erase_chip');

        if (result.success) {
            log('Erase complete!', 'success');
        } else {
            log(`Erase failed: ${result.error}`, 'error');
        }
    } catch (e) {
        log(`Error: ${e}`, 'error');
    }

    isBusy = false;
    updateUI();
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', init);
