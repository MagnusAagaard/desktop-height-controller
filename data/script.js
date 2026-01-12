/**
 * Desk Height Controller - Frontend JavaScript
 * Handles SSE connection, UI updates, and user interactions
 */

// Configuration
const CONFIG = {
    SSE_RECONNECT_DELAY: 3000,      // ms to wait before reconnecting SSE
    STATUS_POLL_INTERVAL: 5000,      // ms between status polls (fallback)
    TOAST_DURATION: 3000,            // ms to show toast notifications
    LARGE_MOVE_THRESHOLD: 30,        // cm - show confirmation for moves > this
};

// State
let eventSource = null;
let currentHeight = null;
let targetHeight = null;
let movementState = 'IDLE';
let isConnected = false;
let presets = [];
let systemConfig = {};

// DOM Elements (cached on load)
let elements = {};

/**
 * Initialize the application on page load
 */
document.addEventListener('DOMContentLoaded', () => {
    cacheElements();
    setupEventListeners();
    initializeDiagnostics();
    connectSSE();
    fetchInitialData();
});

/**
 * Initialize diagnostics display with default values
 */
function initializeDiagnostics() {
    elements.diagError.textContent = 'None';
    elements.diagError.style.color = 'var(--color-success)';
}

/**
 * Cache DOM element references for performance
 */
function cacheElements() {
    elements = {
        // Connection status
        connectionStatus: document.getElementById('connection-status'),
        statusText: document.querySelector('.status-text'),
        
        // Height display
        currentHeight: document.getElementById('current-height'),
        movementStatus: document.getElementById('movement-status'),
        
        // Target form
        targetForm: document.getElementById('target-form'),
        targetInput: document.getElementById('target-height'),
        moveBtn: document.getElementById('move-btn'),
        minHeight: document.getElementById('min-height'),
        maxHeight: document.getElementById('max-height'),
        
        // Emergency stop
        stopBtn: document.getElementById('stop-btn'),
        
        // Presets
        presetButtons: document.getElementById('preset-buttons'),
        presetConfigForm: document.getElementById('preset-config-form'),
        
        // Diagnostics
        diagRaw: document.getElementById('diag-raw'),
        diagFiltered: document.getElementById('diag-filtered'),
        diagHeight: document.getElementById('diag-height'),
        diagValid: document.getElementById('diag-valid'),
        diagState: document.getElementById('diag-state'),
        diagTarget: document.getElementById('diag-target'),
        diagClients: document.getElementById('diag-clients'),
        diagUptime: document.getElementById('diag-uptime'),
        diagHeap: document.getElementById('diag-heap'),
        diagError: document.getElementById('diag-error'),
        
        // Calibration
        calibrationForm: document.getElementById('calibration-form'),
        calibrationStatus: document.getElementById('calibration-status'),
        
        // Toast
        toastContainer: document.getElementById('toast-container'),
        
        // Modal
        confirmModal: document.getElementById('confirm-modal'),
        modalTitle: document.getElementById('modal-title'),
        modalMessage: document.getElementById('modal-message'),
        modalCancel: document.getElementById('modal-cancel'),
        modalConfirm: document.getElementById('modal-confirm'),
    };
}

/**
 * Setup event listeners for user interactions
 */
function setupEventListeners() {
    // Target form submission
    elements.targetForm.addEventListener('submit', handleTargetSubmit);
    
    // Emergency stop button
    elements.stopBtn.addEventListener('click', handleEmergencyStop);
    
    // Preset buttons (using event delegation)
    elements.presetButtons.addEventListener('click', handlePresetClick);
    
    // Calibration form
    elements.calibrationForm.addEventListener('submit', handleCalibrationSubmit);
    
    // Modal buttons
    elements.modalCancel.addEventListener('click', hideModal);
    elements.modalConfirm.addEventListener('click', confirmModalAction);
    
    // Close modal on background click
    elements.confirmModal.addEventListener('click', (e) => {
        if (e.target === elements.confirmModal) {
            hideModal();
        }
    });
    
    // Keyboard escape to close modal
    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape' && elements.confirmModal.getAttribute('aria-hidden') === 'false') {
            hideModal();
        }
    });
}

// ===========================================
// SSE (Server-Sent Events) Connection
// ===========================================

/**
 * Establish SSE connection to the server
 */
function connectSSE() {
    if (eventSource) {
        eventSource.close();
    }
    
    eventSource = new EventSource('/events');
    
    eventSource.onopen = () => {
        console.log('SSE connected');
        setConnectionStatus(true);
    };
    
    eventSource.onerror = (e) => {
        console.error('SSE error:', e);
        setConnectionStatus(false);
        eventSource.close();
        
        // Reconnect after delay
        setTimeout(connectSSE, CONFIG.SSE_RECONNECT_DELAY);
    };
    
    // Height update events
    eventSource.addEventListener('height_update', (e) => {
        try {
            const data = JSON.parse(e.data);
            handleHeightUpdate(data);
        } catch (err) {
            console.error('Failed to parse height_update:', err);
        }
    });
    
    // Status change events
    eventSource.addEventListener('status_change', (e) => {
        try {
            const data = JSON.parse(e.data);
            handleStatusChange(data);
        } catch (err) {
            console.error('Failed to parse status_change:', err);
        }
    });
    
    // Error events
    eventSource.addEventListener('error', (e) => {
        try {
            const data = JSON.parse(e.data);
            handleErrorEvent(data);
        } catch (err) {
            console.error('Failed to parse error event:', err);
        }
    });
    
    // Preset updated events
    eventSource.addEventListener('preset_updated', (e) => {
        try {
            const data = JSON.parse(e.data);
            handlePresetUpdated(data);
        } catch (err) {
            console.error('Failed to parse preset_updated:', err);
        }
    });
    
    // WiFi status events
    eventSource.addEventListener('wifi_status', (e) => {
        try {
            const data = JSON.parse(e.data);
            handleWiFiStatus(data);
        } catch (err) {
            console.error('Failed to parse wifi_status:', err);
        }
    });
}

/**
 * Update connection status indicator
 */
function setConnectionStatus(connected) {
    isConnected = connected;
    elements.connectionStatus.classList.toggle('connected', connected);
    elements.connectionStatus.classList.toggle('disconnected', !connected);
    elements.statusText.textContent = connected ? 'Connected' : 'Disconnected';
}

// ===========================================
// SSE Event Handlers
// ===========================================

/**
 * Handle height update events from SSE
 */
function handleHeightUpdate(data) {
    currentHeight = data.height;
    
    // Update main height display
    // Show "--" if uncalibrated (height is 0) or invalid
    if (currentHeight === 0 || !data.valid) {
        elements.currentHeight.textContent = '--';
    } else {
        elements.currentHeight.textContent = currentHeight.toFixed(1);
    }
    
    // Update diagnostics - always show raw sensor data
    if (data.rawDistance !== undefined) {
        elements.diagRaw.textContent = `${data.rawDistance} mm`;
    }
    if (data.filteredDistance !== undefined) {
        elements.diagFiltered.textContent = `${data.filteredDistance} mm`;
    }
    elements.diagHeight.textContent = currentHeight === 0 ? 'Uncalibrated' : `${currentHeight.toFixed(1)} cm`;
    
    if (data.valid !== undefined) {
        elements.diagValid.textContent = data.valid ? 'Yes' : 'No';
        elements.diagValid.style.color = data.valid ? 'var(--color-success)' : 'var(--color-danger)';
    }
    
    // Update system diagnostics (live from SSE)
    if (data.uptime !== undefined) {
        elements.diagUptime.textContent = formatUptime(data.uptime);
    }
    if (data.freeHeap !== undefined) {
        elements.diagHeap.textContent = `${data.freeHeap} bytes`;
    }
    if (data.sseClients !== undefined) {
        elements.diagClients.textContent = data.sseClients;
    }
    
    // Update target height
    if (data.targetHeight !== undefined) {
        if (data.targetActive) {
            elements.diagTarget.textContent = `${data.targetHeight} cm`;
            targetHeight = data.targetHeight;
        } else {
            elements.diagTarget.textContent = 'None';
            targetHeight = null;
        }
    }
}

/**
 * Handle status change events from SSE
 */
function handleStatusChange(data) {
    movementState = data.state;
    
    // Update movement status display
    const statusEl = elements.movementStatus;
    statusEl.textContent = formatStateName(movementState);
    statusEl.className = 'movement-status ' + getStateClass(movementState);
    
    // Update diagnostics
    elements.diagState.textContent = movementState;
    
    if (data.target_cm !== undefined) {
        targetHeight = data.target_cm;
        elements.diagTarget.textContent = `${data.target_cm} cm`;
    }
    
    // Update button states based on movement state
    updateButtonStates();
    
    // Show notification on state change
    if (movementState === 'ERROR') {
        showToast('Movement error occurred', 'error');
    } else if (movementState === 'IDLE' && targetHeight !== null) {
        showToast('Movement complete', 'success');
        targetHeight = null;
    }
}

/**
 * Handle error events from SSE
 */
function handleErrorEvent(data) {
    console.error('Server error:', data);
    const errorMsg = data.message || data.code || 'Unknown error';
    elements.diagError.textContent = errorMsg;
    elements.diagError.style.color = 'var(--color-danger)';
    showToast(errorMsg, 'error');
}

/**
 * Handle preset updated events from SSE
 */
function handlePresetUpdated(data) {
    // Refresh presets
    fetchPresets();
    showToast(`Preset ${data.slot} updated`, 'success');
}

/**
 * Handle WiFi status events from SSE
 */
function handleWiFiStatus(data) {
    console.log('WiFi status:', data);
    // Could update a WiFi indicator in the UI if needed
}

// ===========================================
// HTTP API Calls
// ===========================================

/**
 * Fetch initial data from the server
 */
async function fetchInitialData() {
    try {
        await Promise.all([
            fetchStatus(),
            fetchConfig(),
            fetchPresets(),
        ]);
    } catch (err) {
        console.error('Failed to fetch initial data:', err);
        showToast('Failed to load initial data', 'error');
    }
}

/**
 * Fetch current status from the server
 */
async function fetchStatus() {
    try {
        const response = await fetch('/status');
        if (!response.ok) throw new Error('Status fetch failed');
        
        const data = await response.json();
        
        // Update height
        if (data.height_cm !== undefined) {
            currentHeight = data.height_cm;
            elements.currentHeight.textContent = currentHeight.toFixed(1);
        }
        
        // Update state
        if (data.state) {
            movementState = data.state;
            elements.movementStatus.textContent = formatStateName(movementState);
            elements.movementStatus.className = 'movement-status ' + getStateClass(movementState);
        }
        
        // Update diagnostics
        if (data.raw_mm !== undefined) elements.diagRaw.textContent = `${data.raw_mm} mm`;
        if (data.filtered_mm !== undefined) elements.diagFiltered.textContent = `${data.filtered_mm} mm`;
        if (data.valid !== undefined) elements.diagValid.textContent = data.valid ? 'Yes' : 'No';
        if (data.target_cm !== undefined) elements.diagTarget.textContent = `${data.target_cm} cm`;
        if (data.sseClients !== undefined) elements.diagClients.textContent = data.sseClients;
        if (data.uptime !== undefined) elements.diagUptime.textContent = formatUptime(data.uptime);
        if (data.freeHeap !== undefined) elements.diagHeap.textContent = `${data.freeHeap} bytes`;
        
        updateButtonStates();
    } catch (err) {
        console.error('Failed to fetch status:', err);
    }
}

/**
 * Fetch system configuration
 */
async function fetchConfig() {
    try {
        const response = await fetch('/config');
        if (!response.ok) throw new Error('Config fetch failed');
        
        systemConfig = await response.json();
        
        // Update UI with config values
        if (systemConfig.min_height !== undefined) {
            elements.minHeight.textContent = systemConfig.min_height;
            elements.targetInput.min = systemConfig.min_height;
        }
        if (systemConfig.max_height !== undefined) {
            elements.maxHeight.textContent = systemConfig.max_height;
            elements.targetInput.max = systemConfig.max_height;
        }
        
        // Update calibration status
        if (systemConfig.calibration_constant !== undefined) {
            elements.calibrationStatus.textContent = 
                `Current calibration constant: ${systemConfig.calibration_constant} cm`;
        }
    } catch (err) {
        console.error('Failed to fetch config:', err);
    }
}

/**
 * Fetch presets from the server
 */
async function fetchPresets() {
    try {
        const response = await fetch('/presets');
        if (!response.ok) throw new Error('Presets fetch failed');
        
        presets = await response.json();
        updatePresetButtons();
        updatePresetConfigForm();
    } catch (err) {
        console.error('Failed to fetch presets:', err);
    }
}

/**
 * Send target height to the server
 */
async function sendTargetHeight(height) {
    try {
        const response = await fetch('/target', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ height: height }),
        });
        
        if (!response.ok) {
            const data = await response.json();
            throw new Error(data.error || 'Failed to set target');
        }
        
        const data = await response.json();
        showToast(`Moving to ${height} cm`, 'success');
        return data;
    } catch (err) {
        console.error('Failed to set target:', err);
        showToast(err.message, 'error');
        throw err;
    }
}

/**
 * Send emergency stop command to the server
 */
async function sendEmergencyStop() {
    try {
        const response = await fetch('/stop', {
            method: 'POST',
        });
        
        if (!response.ok) {
            throw new Error('Failed to stop');
        }
        
        const data = await response.json();
        showToast('Emergency stop activated', 'warning');
        return data;
    } catch (err) {
        console.error('Failed to send stop:', err);
        showToast('Failed to stop - please disconnect power!', 'error');
        throw err;
    }
}

/**
 * Save a preset to the server
 */
async function savePreset(slot, name, height) {
    try {
        console.log('savePreset called:', { slot, name, height });
        const response = await fetch('/preset/save', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ slot: slot, name: name, height: height }),
        });
        
        console.log('savePreset response status:', response.status);
        const responseText = await response.text();
        console.log('savePreset response body:', responseText);
        
        if (!response.ok) {
            let errorMsg = 'Failed to save preset';
            try {
                const data = JSON.parse(responseText);
                errorMsg = data.error || errorMsg;
            } catch (e) {
                errorMsg = responseText || errorMsg;
            }
            throw new Error(errorMsg);
        }
        
        // Parse response to confirm save
        const data = JSON.parse(responseText);
        if (data.success) {
            showToast(`Preset ${slot} saved successfully`, 'success');
        } else {
            showToast(`Preset ${slot} saved`, 'success');
        }
        fetchPresets();
    } catch (err) {
        console.error('Failed to save preset:', err);
        showToast(err.message, 'error');
    }
}

/**
 * Activate a preset
 */
async function activatePreset(slot) {
    try {
        const response = await fetch('/preset', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ slot: slot }),
        });
        
        if (!response.ok) {
            const data = await response.json();
            throw new Error(data.error || 'Failed to activate preset');
        }
        
        const data = await response.json();
        showToast(`Moving to preset ${slot}`, 'success');
    } catch (err) {
        console.error('Failed to activate preset:', err);
        showToast(err.message, 'error');
    }
}

/**
 * Send calibration to the server
 */
async function sendCalibration(knownHeight) {
    try {
        const response = await fetch('/calibrate', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ height: knownHeight }),
        });
        
        if (!response.ok) {
            const data = await response.json();
            throw new Error(data.error || 'Calibration failed');
        }
        
        showToast('Calibration successful', 'success');
        fetchConfig();
    } catch (err) {
        console.error('Calibration failed:', err);
        showToast(err.message, 'error');
    }
}

// ===========================================
// Event Handlers
// ===========================================

/**
 * Handle target form submission
 */
function handleTargetSubmit(e) {
    e.preventDefault();
    
    const height = parseFloat(elements.targetInput.value);
    if (isNaN(height)) {
        showToast('Please enter a valid height', 'error');
        return;
    }
    
    // Check if this is a large move that needs confirmation
    if (currentHeight !== null && Math.abs(height - currentHeight) > CONFIG.LARGE_MOVE_THRESHOLD) {
        showModal(
            'Confirm Large Move',
            `You are about to move the desk ${Math.abs(height - currentHeight).toFixed(1)} cm. Are you sure?`,
            () => sendTargetHeight(height)
        );
    } else {
        sendTargetHeight(height);
    }
}

/**
 * Handle emergency stop button click
 */
function handleEmergencyStop() {
    sendEmergencyStop();
}

/**
 * Handle preset button click (using event delegation)
 */
function handlePresetClick(e) {
    const presetBtn = e.target.closest('.btn-preset');
    if (!presetBtn || presetBtn.disabled) return;
    
    const slot = parseInt(presetBtn.dataset.slot, 10);
    activatePreset(slot);
}

/**
 * Handle calibration form submission
 */
function handleCalibrationSubmit(e) {
    e.preventDefault();
    
    const heightInput = document.getElementById('calibration-height');
    const height = parseFloat(heightInput.value);
    
    if (isNaN(height) || height < 30 || height > 200) {
        showToast('Please enter a valid height (30-200 cm)', 'error');
        return;
    }
    
    // Send calibration directly without modal confirmation
    sendCalibration(height);
}

// ===========================================
// UI Update Functions
// ===========================================

/**
 * Update preset button states and labels
 */
function updatePresetButtons() {
    const buttons = elements.presetButtons.querySelectorAll('.btn-preset');
    
    buttons.forEach(btn => {
        const slot = parseInt(btn.dataset.slot, 10);
        const preset = presets.find(p => p.slot === slot);
        
        const labelEl = btn.querySelector('.preset-label');
        const heightEl = btn.querySelector('.preset-height');
        
        if (preset && preset.height_cm > 0) {
            btn.disabled = false;
            labelEl.textContent = preset.name || `Preset ${slot}`;
            heightEl.textContent = `${preset.height_cm} cm`;
        } else {
            btn.disabled = true;
            labelEl.textContent = `Preset ${slot}`;
            heightEl.textContent = '-- cm';
        }
    });
}

/**
 * Update preset configuration form
 */
function updatePresetConfigForm() {
    const container = elements.presetConfigForm;
    container.innerHTML = '';
    
    for (let slot = 1; slot <= 5; slot++) {
        const preset = presets.find(p => p.slot === slot) || { slot, name: '', height_cm: 0 };
        
        const item = document.createElement('div');
        item.className = 'preset-config-item';
        item.innerHTML = `
            <span>Slot ${slot}:</span>
            <input type="text" placeholder="Name" value="${preset.name || ''}" data-slot="${slot}" data-field="name">
            <input type="number" placeholder="Height" value="${preset.height_cm || ''}" min="50" max="125" data-slot="${slot}" data-field="height">
            <span>cm</span>
            <button type="button" class="btn btn-primary btn-save-preset" data-slot="${slot}">Save</button>
        `;
        container.appendChild(item);
    }
    
    // Add save button listeners
    container.querySelectorAll('.btn-save-preset').forEach(btn => {
        btn.addEventListener('click', () => {
            const slot = parseInt(btn.dataset.slot, 10);
            const nameInput = container.querySelector(`input[data-slot="${slot}"][data-field="name"]`);
            const heightInput = container.querySelector(`input[data-slot="${slot}"][data-field="height"]`);
            
            const name = nameInput.value.trim() || `Preset ${slot}`;
            const height = parseFloat(heightInput.value);
            
            if (isNaN(height) || height < 50 || height > 125) {
                showToast('Please enter a valid height (50-125 cm)', 'error');
                return;
            }
            
            savePreset(slot, name, height);
        });
    });
}

/**
 * Update button states based on current movement state
 */
function updateButtonStates() {
    const isMoving = movementState === 'MOVING_UP' || movementState === 'MOVING_DOWN' || movementState === 'STABILIZING';
    
    // Disable move button while moving
    elements.moveBtn.disabled = isMoving;
    elements.moveBtn.textContent = isMoving ? 'Moving...' : 'Move to Height';
    
    // Keep stop button always enabled
    elements.stopBtn.disabled = false;
}

// ===========================================
// Modal Functions
// ===========================================

let modalCallback = null;

/**
 * Show confirmation modal
 */
function showModal(title, message, onConfirm) {
    elements.modalTitle.textContent = title;
    elements.modalMessage.textContent = message;
    modalCallback = onConfirm;
    elements.confirmModal.setAttribute('aria-hidden', 'false');
    elements.modalConfirm.focus();
}

/**
 * Hide confirmation modal
 */
function hideModal() {
    elements.confirmModal.setAttribute('aria-hidden', 'true');
    modalCallback = null;
}

/**
 * Execute modal confirm action
 */
function confirmModalAction() {
    if (modalCallback) {
        modalCallback();
    }
    hideModal();
}

// ===========================================
// Toast Notifications
// ===========================================

/**
 * Show a toast notification
 */
function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    
    elements.toastContainer.appendChild(toast);
    
    // Remove after duration
    setTimeout(() => {
        toast.style.animation = 'slideUp 0.3s ease-out reverse';
        setTimeout(() => toast.remove(), 300);
    }, CONFIG.TOAST_DURATION);
}

// ===========================================
// Utility Functions
// ===========================================

/**
 * Format state name for display
 */
function formatStateName(state) {
    switch (state) {
        case 'IDLE': return 'Idle';
        case 'MOVING_UP': return 'Moving Up';
        case 'MOVING_DOWN': return 'Moving Down';
        case 'STABILIZING': return 'Stabilizing';
        case 'ERROR': return 'Error';
        default: return state;
    }
}

/**
 * Get CSS class for movement state
 */
function getStateClass(state) {
    switch (state) {
        case 'IDLE': return 'idle';
        case 'MOVING_UP': return 'moving-up';
        case 'MOVING_DOWN': return 'moving-down';
        case 'STABILIZING': return 'stabilizing';
        case 'ERROR': return 'error';
        default: return '';
    }
}

/**
 * Format uptime in human-readable format
 */
function formatUptime(ms) {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);
    
    if (days > 0) {
        return `${days}d ${hours % 24}h`;
    } else if (hours > 0) {
        return `${hours}h ${minutes % 60}m`;
    } else if (minutes > 0) {
        return `${minutes}m ${seconds % 60}s`;
    } else {
        return `${seconds}s`;
    }
}
