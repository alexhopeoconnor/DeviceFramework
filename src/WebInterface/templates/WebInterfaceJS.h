#ifndef WEBINTERFACE_JS_H
#define WEBINTERFACE_JS_H

#include <Arduino.h>

// JavaScript for DeviceFramework Web Interface
const char PROGMEM js_scripts[] = R"rawliteral(
// Page-aware JavaScript for the server-rendered interface
let statusUpdateInterval = null;
let statusRequestInFlight = false;
let statusErrorShown = false;
let autoScrollSerial = true;
let webserialSocket = null;
let webserialConnected = false;
let webserialDesired = false;
let webserialReconnectAttempts = 0;
let webserialReconnectTimer = null;
let webserialLastMessageTime = 0;
let webserialHeartbeatInterval = null;
let webserialOpenedAt = 0;
let webserialReceivedMessage = false;
let serialAvailabilityRequestInFlight = false;
let serialAvailabilityRetryTimer = null;
let currentPage = "unknown";

// The server renders one complete page at a time. Keep status polling and
// WebSerial opt-in: a normal status visit must not open a WebSocket.
document.addEventListener("DOMContentLoaded", function() {
    const pageRoot = document.querySelector("[data-df-page]");
    currentPage = pageRoot ? pageRoot.dataset.dfPage : "unknown";
    console.log("DeviceFramework page loaded:", currentPage);
    hidePageLoader();
    setupEventListeners();

    setTimeout(() => {
        if (currentPage === "status") {
            startStatusPage();
        } else if (currentPage === "serial") {
            initializeSerialMonitor();
            loadSavedIntervals();
            initializeSerialPage();
        } else if (currentPage === "controls") {
            loadSavedIntervals();
        }
    }, 100);
});

window.addEventListener("pagehide", function() {
    if (statusUpdateInterval) {
        clearInterval(statusUpdateInterval);
        statusUpdateInterval = null;
    }
    if (serialAvailabilityRetryTimer) {
        clearTimeout(serialAvailabilityRetryTimer);
        serialAvailabilityRetryTimer = null;
    }
    stopWebSerial();
});

function startStatusPage() {
    loadStatus();
    statusUpdateInterval = setInterval(loadStatus, getStatusUpdateInterval());
}

function getStatusUpdateInterval() {
    const value = parseInt(localStorage.getItem("statusInterval"), 10);
    return Number.isFinite(value) && value >= 1000 ? value : 2000;
}

function initializeSerialPage() {
    if (serialAvailabilityRequestInFlight || currentPage !== "serial") {
        return;
    }

    serialAvailabilityRequestInFlight = true;
    fetch("/api/status")
        .then(response => {
            if (!response.ok) throw new Error("Status request failed");
            return response.json();
        })
        .then(data => {
            const logging = data.runtime && data.runtime.logging;
            const serialEnabled = !logging || logging.serial_enabled !== false;
            const webSerialEnabled = logging && logging.web_serial_enabled === true;
            updateSerialAvailability(serialEnabled);
            updateWebSocketConnection(serialEnabled, webSerialEnabled);
        })
        .catch(error => {
            // A bounded ESP8266 can briefly return 503 while another page is
            // streaming. This is not a WebSerial failure: retry the small
            // capability check rather than leaving the serial page inert.
            console.info("WebSerial availability check deferred:", error);
            addSerialLine("Waiting for device status before connecting WebSerial...");
            updateWebSerialStatus("connecting");
            serialAvailabilityRetryTimer = setTimeout(initializeSerialPage, 1500);
        })
        .finally(() => {
            serialAvailabilityRequestInFlight = false;
        });
}

// Setup event listeners
function setupEventListeners() {
    document.querySelectorAll(".nav-link").forEach(link => {
        if (link.dataset.page === currentPage) {
            link.classList.add("active");
            link.setAttribute("aria-current", "page");
        }
        link.addEventListener("click", closeMobileNav);
    });
}

// Toggle mobile navigation
function toggleNav() {
    const navMenu = document.getElementById('nav-menu');
    const navToggle = document.querySelector('.nav-toggle');

    if (navMenu && navToggle) {
        navMenu.classList.toggle('open');
        navToggle.classList.toggle('active');
    }
}

// Close mobile navigation
function closeMobileNav() {
    const navMenu = document.getElementById('nav-menu');
    const navToggle = document.querySelector('.nav-toggle');

    if (navMenu && navToggle) {
        navMenu.classList.remove('open');
        navToggle.classList.remove('active');
    }
}

// Load status from API
async function loadStatus() {
    if (statusRequestInFlight) return;

    statusRequestInFlight = true;
    try {
        const response = await fetch('/api/status');
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();
        updateStatusDisplay(data);
        clearErrorState();
    } catch (error) {
        console.error('Error loading status:', error);
        if (!statusErrorShown) {
            showError('Failed to load device status: ' + error.message);
            statusErrorShown = true;
        }
        setErrorState();
    } finally {
        statusRequestInFlight = false;
    }
}

// Set error state for status elements
function setErrorState() {
    const statusElements = document.querySelectorAll('.status-value');
    statusElements.forEach(element => {
        if (!element.classList.contains('error')) {
            element.classList.add('error');
            // Only update textContent if it's a simple text element
            if (element.textContent && !element.innerHTML.includes('<')) {
                element.textContent = 'Error loading...';
            }
        }
    });
}

// Clear error state
function clearErrorState() {
    statusErrorShown = false;
    const errorElements = document.querySelectorAll('.status-value.error');
    errorElements.forEach(element => {
        element.classList.remove('error');
    });
}

// Update status display - enhanced version with comprehensive device information
function updateStatusDisplay(data) {
    try {
        // Update Hardware Information
        updateElement('device-name', data.hardware.version || 'Unknown');
        updateElement('device-version', data.hardware.version || 'Unknown');
        updateElement('device-cpu-freq', data.hardware.cpu_freq || 'Unknown');
        updateElement('device-flash-size', formatBytes(data.hardware.flash_size));
        updateElement('device-sketch-size', formatBytes(data.hardware.sketch_size));
        updateElement('free-sketch-space', formatBytes(data.hardware.free_sketch_space));

        // Update WiFi stability information
        if (data.runtime && data.runtime.wifi) {
            const wifi = data.runtime.wifi;
            updateElement('wifi-status', wifi.connected ? 'Connected' : 'Disconnected', wifi.connected ? 'connected' : 'disconnected');
            updateElement('wifi-ssid', wifi.ssid || 'Unknown');
            updateElement('wifi-ip', wifi.ip || 'Unknown');
            updateElement('wifi-gateway', wifi.gateway || 'Unknown');
            updateElement('wifi-mac', wifi.mac || 'Unknown');
            updateElement('wifi-rssi', wifi.rssi || -100);
            updateElement('wifi-channel', wifi.channel || 'Unknown');

            // Update WiFi status indicator
            updateStatusIndicator('wifi-indicator', wifi.connected);

            // Update WiFi signal bars
            const rssi = parseInt(wifi.rssi) || -100;
            updateWifiSignalBars(rssi);

        }

        // Update MQTT stability information
        if (data.runtime && data.runtime.mqtt) {
            const mqtt = data.runtime.mqtt;
            updateElement('mqtt-status', mqtt.connected ? 'Connected' : 'Disconnected', mqtt.connected ? 'connected' : 'disconnected');
            updateElement('mqtt-broker', mqtt.broker || 'Unknown');
            updateElement('mqtt-port', mqtt.port || 'Unknown');
            updateElement('mqtt-user', mqtt.user || 'None');

            // Update MQTT status indicator
            updateStatusIndicator('mqtt-indicator', mqtt.connected);

        }

        // Update Device Information
        if (data.runtime && data.runtime.device) {
            const device = data.runtime.device;
            updateElement('device-name', device.name || 'Unknown');
            updateElement('device-uptime', formatUptime(device.uptime));
        }

        // Update Enhanced Memory Information
        if (data.runtime && data.runtime.memory) {
            const memory = data.runtime.memory;
            updateElement('memory-free', formatBytes(memory.free));
            updateElement('memory-total', formatBytes(data.hardware.max_memory));

            const memoryUsage = Math.round(((data.hardware.max_memory - memory.free) / data.hardware.max_memory) * 100);
            updateElement('memory-usage', memoryUsage);
            updateMemoryBar(memoryUsage);

        }

        // Update Flash Information
        updateElement('flash-used', formatBytes(data.hardware.sketch_size));
        updateElement('flash-total', formatBytes(data.hardware.flash_size));

        const flashUsage = Math.round((data.hardware.sketch_size / data.hardware.flash_size) * 100);
        updateElement('flash-usage', flashUsage);
        updateFlashBar(flashUsage);

        // Update Logging Information
        if (data.runtime && data.runtime.logging) {
            const logging = data.runtime.logging;
            updateElement('log-level', logging.log_level || 'INFO');

            // Update serial availability based on device configuration
            updateSerialAvailability(logging.serial_enabled !== false);

        }

    } catch (error) {
        console.error('Error updating status display:', error);
    }
}

// Helper function to update individual elements
function updateElement(elementId, value, className = null) {
    const element = document.getElementById(elementId);
    if (element && element.textContent !== undefined) {
        element.textContent = value;
        if (className) {
            element.className = 'status-value ' + className;
        }
    } else if (!element) {
        console.warn('Element not found:', elementId);
    }
}

// Update memory usage bar
function updateMemoryBar(percentage) {
    try {
        const memoryBarFill = document.getElementById('memory-bar-fill');
        if (memoryBarFill && memoryBarFill.style) {
            memoryBarFill.style.width = percentage + '%';

            // Change color based on usage level
            if (percentage > 80) {
                memoryBarFill.style.background = 'linear-gradient(90deg, #e53e3e, #c53030)';
            } else if (percentage > 60) {
                memoryBarFill.style.background = 'linear-gradient(90deg, #ed8936, #dd6b20)';
            } else {
                memoryBarFill.style.background = 'linear-gradient(90deg, #38a169, #2f855a)';
            }
        }
    } catch (error) {
        console.error('Error updating memory bar:', error);
    }
}

// Update flash bar
function updateFlashBar(percentage) {
    try {
        const flashBarFill = document.getElementById('flash-bar-fill');
        if (flashBarFill && flashBarFill.style) {
            flashBarFill.style.width = percentage + '%';

            // Change color based on usage level
            if (percentage > 80) {
                flashBarFill.style.background = 'linear-gradient(90deg, #e53e3e, #c53030)';
            } else if (percentage > 60) {
                flashBarFill.style.background = 'linear-gradient(90deg, #ed8936, #dd6b20)';
            } else {
                flashBarFill.style.background = 'linear-gradient(90deg, #3182ce, #2c5282)';
            }
        }
    } catch (error) {
        console.error('Error updating flash bar:', error);
    }
}

// Update status indicator
function updateStatusIndicator(elementId, isConnected) {
    try {
        const indicator = document.getElementById(elementId);
        if (indicator) {
            if (isConnected) {
                indicator.className = 'status-indicator connected';
            } else {
                indicator.className = 'status-indicator disconnected';
            }
        }
    } catch (error) {
        console.error('Error updating status indicator:', error);
    }
}

// Update WiFi signal strength bars based on RSSI
function updateWifiSignalBars(rssi) {
    try {
        const signal = parseInt(rssi) || -100;

        // Find the bars - they should exist now
        const bars = document.querySelectorAll('#wifi-signal-bars .wifi-bar');

        if (bars.length === 0) {
            // Retry if bars not found yet, but limit retries
            setTimeout(() => {
                const retryBars = document.querySelectorAll('#wifi-signal-bars .wifi-bar');
                if (retryBars.length > 0) {
                    updateWifiSignalBars(rssi);
                }
            }, 100);
            return;
        }

        // Calculate how many bars should be active
        let activeBars = 0;
        if (signal >= -50) activeBars = 4;      // Excellent signal
        else if (signal >= -60) activeBars = 3;  // Good signal
        else if (signal >= -70) activeBars = 2; // Fair signal
        else if (signal >= -80) activeBars = 1; // Poor signal
        else activeBars = 0;                     // Very poor signal

        // Update each bar
        bars.forEach((bar, index) => {
            if (bar && bar.style) {
                // Remove all classes first
                bar.classList.remove('active', 'medium', 'weak');

                if (index < activeBars) {
                    // Active bars - color based on signal strength
                    if (signal >= -60) {
                        bar.classList.add('active');
                    } else if (signal >= -70) {
                        bar.classList.add('medium');
                    } else {
                        bar.classList.add('weak');
                    }
                } else {
                    // Inactive bars - ensure they're gray
                    bar.style.backgroundColor = '#e2e8f0';
                    bar.style.boxShadow = 'none';
                }
            }
        });
    } catch (error) {
        console.error('Error updating WiFi signal bars:', error);
    }
}

// Format number with commas
function formatNumber(num) {
    if (num === null || num === undefined) return 'Unknown';
    return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
}


// Page loader functions
function hidePageLoader() {
    const pageLoader = document.getElementById('page-loader');
    if (pageLoader) {
        // Add a small delay to ensure smooth transition
        setTimeout(() => {
            pageLoader.classList.add('hidden');
            // Remove from DOM after transition
            setTimeout(() => {
                if (pageLoader.parentNode) {
                    pageLoader.parentNode.removeChild(pageLoader);
                }
            }, 500);
        }, 100);
    }
}


// Update serial navigation visibility from a status or serial-page availability check.
let serialAvailable = true;

function updateSerialAvailability(serialEnabled) {
    serialAvailable = serialEnabled !== false;
    const serialSection = document.getElementById("serial-output");
    const serialNavLink = document.querySelector("a[data-page=\"serial\"]");

    if (serialSection) {
        serialSection.style.display = serialAvailable ? "block" : "none";
    }
    if (serialNavLink) {
        serialNavLink.style.display = serialAvailable ? "block" : "none";
    }

    if (!serialAvailable && currentPage === "serial") {
        updateWebSocketConnection(false, false);
    }
}

function updateWebSocketConnection(serialEnabled, webSerialEnabled) {
    const shouldConnect = serialEnabled === true && webSerialEnabled === true;
    if (webserialDesired === shouldConnect) {
        return;
    }

    webserialDesired = shouldConnect;
    if (webserialDesired) {
        initializeWebSerial();
    } else {
        stopWebSerial();
    }
}

// Remove whitespace-only text nodes from serial output
function cleanSerialOutputWhitespace() {
    const serialOutput = document.getElementById('serial-output-content');
    if (!serialOutput) return;

    // Remove all text nodes that are only whitespace
    const walker = document.createTreeWalker(
        serialOutput,
        NodeFilter.SHOW_TEXT,
        null,
        false
    );

    const nodesToRemove = [];
    let node;
    while (node = walker.nextNode()) {
        if (/^\s+$/.test(node.textContent)) {
            nodesToRemove.push(node);
        }
    }

    nodesToRemove.forEach(n => n.remove());
}

// Initialize serial monitor
function initializeSerialMonitor() {
    const serialOutput = document.getElementById('serial-output-content');
    if (serialOutput) {
        // Remove any whitespace text nodes from HTML template
        cleanSerialOutputWhitespace();
        // Add initial message
        addSerialLine('Serial monitor initialized...');

        // Start periodic stats update
        setInterval(updateSerialStats, 1000);
    }
}

// Initialize WebSocket connection for real-time serial output
function initializeWebSerial() {
    if (!webserialDesired ||
        (webserialSocket && (webserialSocket.readyState === WebSocket.CONNECTING ||
            webserialSocket.readyState === WebSocket.OPEN))) {
        return;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/webserial`;

    try {
        const socket = new WebSocket(wsUrl);
        webserialSocket = socket;

        socket.onopen = function(event) {
            if (!webserialDesired || webserialSocket !== socket) {
                socket.close();
                return;
            }
            webserialConnected = true;
            webserialReconnectAttempts = 0;
            webserialLastMessageTime = Date.now();
            webserialOpenedAt = webserialLastMessageTime;
            webserialReceivedMessage = false;
            webserialConnectionTime = Date.now();
            addSerialLine('WebSerial connected - real-time output enabled');
            console.log('WebSerial WebSocket connected');

            // Update connection status indicator
            updateWebSerialStatus('connected');

            // Start heartbeat monitoring
            startWebSerialHeartbeat();
        };

        socket.onmessage = function(event) {
            // Update last message time for health monitoring
            webserialLastMessageTime = Date.now();
            webserialReceivedMessage = true;

            // Handle incoming serial data with buffering
            processWebSocketMessage(event.data);
        };

        socket.onclose = function(event) {
            if (webserialSocket !== socket) return;

            webserialSocket = null;
            webserialConnected = false;
            webserialConnectionTime = null;
            stopWebSerialHeartbeat();

            // ESPAsyncWebServer on ESP8266 may abort a close issued from its
            // connect callback before the 1013 frame reaches the browser. A
            // socket that opens then closes without a message is still a
            // capacity rejection in that path; throttle it just like 1013.
            const elapsedSinceOpen = webserialOpenedAt ? Date.now() - webserialOpenedAt : 0;
            const immediateRejection = elapsedSinceOpen > 0 && elapsedSinceOpen < 1000 &&
                !webserialReceivedMessage;
            webserialOpenedAt = 0;

            if (!webserialDesired) {
                updateWebSerialStatus('disconnected');
                return;
            }

            // The server uses 1013 when its bounded WebSerial capacity is full
            // or diagnostic work is unsafe. onopen can fire before that close,
            // so do not reset into a one-second reconnect loop under pressure.
            updateWebSerialStatus('disconnected');
            if (event.code === 1013 || immediateRejection) {
                addSerialLine('WebSerial is temporarily unavailable - retrying in 10 seconds...');
                console.info('WebSerial WebSocket busy; retrying later');
                webserialReconnectTimer = setTimeout(() => {
                    if (webserialDesired && !webserialConnected) {
                        initializeWebSerial();
                    }
                }, 10000);
                return;
            }

            addSerialLine('WebSerial disconnected - attempting reconnection...');
            console.log('WebSerial WebSocket disconnected');

            // Exponential backoff for an unexpected connection loss.
            const delay = Math.min(1000 * Math.pow(2, webserialReconnectAttempts), 30000);
            webserialReconnectAttempts++;

            webserialReconnectTimer = setTimeout(() => {
                if (webserialDesired && !webserialConnected) {
                    initializeWebSerial();
                }
            }, delay);
        };

        socket.onerror = function(error) {
            // A close event follows transport errors and carries the policy code.
            // Keep the console clean for expected 1013 capacity rejection.
            console.debug('WebSerial WebSocket transport error:', error);
        };

    } catch (error) {
        console.error('Failed to initialize WebSerial WebSocket:', error);
        addSerialLine('WebSerial initialization failed - using polling only');
    }
}

function stopWebSerial() {
    if (webserialReconnectTimer) {
        clearTimeout(webserialReconnectTimer);
        webserialReconnectTimer = null;
    }
    stopWebSerialHeartbeat();
    webserialConnected = false;
    webserialConnectionTime = null;

    if (webserialSocket) {
        const socket = webserialSocket;
        webserialSocket = null;
        socket.onclose = null;
        if (socket.readyState !== WebSocket.CLOSED) {
            socket.close();
        }
    }

    updateWebSerialStatus('disconnected');
}

// WebSocket health monitoring with keepalive pings
function startWebSerialHeartbeat() {
    if (webserialHeartbeatInterval) {
        clearInterval(webserialHeartbeatInterval);
    }

    webserialHeartbeatInterval = setInterval(() => {
        if (webserialConnected && webserialSocket && webserialSocket.readyState === WebSocket.OPEN) {
            // Send ping to keep connection alive
            // This prevents the connection from being closed due to inactivity
            try {
                webserialSocket.send(JSON.stringify({ type: 'ping' }));
            } catch (error) {
                console.error('Error sending WebSocket ping:', error);
                // Connection might be dead, close it
                webserialSocket.close();
            }
        }
    }, 30000); // Send ping every 30 seconds to keep connection alive
}

function stopWebSerialHeartbeat() {
    if (webserialHeartbeatInterval) {
        clearInterval(webserialHeartbeatInterval);
        webserialHeartbeatInterval = null;
    }
}

// Update WebSocket connection status indicator
function updateWebSerialStatus(status) {
    const statusIndicator = document.getElementById('webserial-status');
    if (statusIndicator) {
        statusIndicator.className = `webserial-status webserial-${status}`;
        statusIndicator.textContent = status === 'connected' ? '● Connected' : '● Disconnected';
    }
}

// HTTP polling removed - WebSocket provides real-time updates

// Terminal state variables
let serialLineCount = 0;
let serialPaused = false;
let serialWrap = true;
let webserialConnectionTime = null;

// Client-side message buffering
let messageBuffer = '';
const MAX_BUFFER_SIZE = 2048;

// Batch DOM operations using DocumentFragment
let pendingLines = [];
let isUpdating = false;
const BATCH_SIZE = 10;
const ANIMATION_DURATION = 300;

// Process WebSocket messages with buffering
function processWebSocketMessage(data) {
    // Add to buffer
    messageBuffer += data;

    // Process complete lines
    processCompleteLines();

    // Prevent buffer overflow
    if (messageBuffer.length > MAX_BUFFER_SIZE) {
        // Force process what we have
        addSerialLine(messageBuffer, true);
        messageBuffer = '';
    }
}

function processCompleteLines() {
    let newlineIndex;
    while ((newlineIndex = messageBuffer.indexOf('\n')) !== -1) {
        const line = messageBuffer.substring(0, newlineIndex).trim();
        if (line.length > 0) {
            addSerialLine(line, true);
        }
        messageBuffer = messageBuffer.substring(newlineIndex + 1);
    }
}

// Add line to serial output with batched DOM operations
function addSerialLine(line, isNew = false) {
    if (serialPaused) return;

    pendingLines.push({
        line: line,
        isNew: isNew,
        timestamp: new Date().toLocaleTimeString()
    });

    // Trigger batched update
    scheduleUpdate();
}

function scheduleUpdate() {
    if (isUpdating) return;

    isUpdating = true;
    requestAnimationFrame(() => {
        processPendingLines();
        isUpdating = false;
    });
}

function processPendingLines() {
    if (pendingLines.length === 0) return;

    const serialOutput = document.getElementById('serial-output-content');
    if (!serialOutput) {
        pendingLines = [];
        return;
    }

    // Process in batches
    const batch = pendingLines.splice(0, BATCH_SIZE);
    const fragment = document.createDocumentFragment();

    batch.forEach(({line, isNew, timestamp}) => {
        const lineElement = createLineElement(line, isNew, timestamp);
        fragment.appendChild(lineElement);
        serialLineCount++;

        // Limit DOM size
        const lines = serialOutput.querySelectorAll('.serial-line');
        if (lines.length > 200) {
            lines[0].remove();
            serialLineCount--;
        }
    });

    // Single DOM append
    serialOutput.appendChild(fragment);

    // Update stats (throttled)
    updateSerialStatsThrottled();

    // Auto-scroll if enabled - always scroll to bottom when new content is added
    if (autoScrollSerial) {
        requestAnimationFrame(() => {
            // Scroll the serial output container to bottom (doesn't affect page scroll)
            serialOutput.scrollTop = serialOutput.scrollHeight;
        });
    }

    // Process next batch if more pending
    if (pendingLines.length > 0) {
        requestAnimationFrame(processPendingLines);
    }
}

function createLineElement(line, isNew, timestamp) {
    const lineElement = document.createElement('div');
    lineElement.className = 'serial-line';

    const logLevel = parseLogLevel(line);
    if (logLevel) {
        lineElement.setAttribute('data-level', logLevel);
    }

    const timestampSpan = document.createElement('span');
    timestampSpan.className = 'serial-timestamp';
    timestampSpan.textContent = `[${timestamp}]`;

    const contentSpan = document.createElement('span');
    contentSpan.className = 'serial-content';
    contentSpan.textContent = line;

    lineElement.appendChild(timestampSpan);
    lineElement.appendChild(contentSpan);

    if (isNew) {
        lineElement.classList.add('serial-line-new');
        // Reduced animation duration
        setTimeout(() => {
            lineElement.classList.remove('serial-line-new');
        }, ANIMATION_DURATION);
    }

    return lineElement;
}

// Throttled stats update
let statsUpdateScheduled = false;
function updateSerialStatsThrottled() {
    if (statsUpdateScheduled) return;

    statsUpdateScheduled = true;
    requestAnimationFrame(() => {
        updateSerialStats();
        statsUpdateScheduled = false;
    });
}

// Parse log level from line content
function parseLogLevel(line) {
    const upperLine = line.toUpperCase();
    if (upperLine.includes('[ERROR]') || upperLine.includes('ERROR:')) return 'ERROR';
    if (upperLine.includes('[WARN]') || upperLine.includes('WARNING:')) return 'WARN';
    if (upperLine.includes('[INFO]') || upperLine.includes('INFO:')) return 'INFO';
    if (upperLine.includes('[DEBUG]') || upperLine.includes('DEBUG:')) return 'DEBUG';
    return null;
}

// Update terminal statistics
function updateSerialStats() {
    const lineCountElement = document.getElementById('serial-line-count');
    const uptimeElement = document.getElementById('serial-uptime');

    if (lineCountElement) {
        lineCountElement.textContent = `Lines: ${serialLineCount}`;
    }

    if (uptimeElement) {
        if (webserialConnectionTime) {
            const uptime = Math.floor((Date.now() - webserialConnectionTime) / 1000);
            uptimeElement.textContent = `Connected: ${formatUptime(uptime)}`;
        } else {
            uptimeElement.textContent = `Connected: 0s`;
        }
    }
}

// Clear serial output
function clearSerial() {
    const serialOutput = document.getElementById('serial-output-content');
    if (serialOutput) {
        serialOutput.innerHTML = '';
        serialLineCount = 0;
        updateSerialStats();
        addSerialLine('Serial output cleared...');
    }
}

// Toggle auto-scroll
function toggleSerialAutoScroll() {
    autoScrollSerial = !autoScrollSerial;
    const btn = document.getElementById('auto-scroll-btn');
    if (btn) {
        btn.textContent = `Auto Scroll: ${autoScrollSerial ? 'ON' : 'OFF'}`;
        btn.classList.toggle('active', autoScrollSerial);
    }
    localStorage.setItem('autoScrollSerial', autoScrollSerial);
}

// Toggle text wrapping
function toggleSerialWrap() {
    serialWrap = !serialWrap;
    const btn = document.getElementById('wrap-btn');
    const output = document.getElementById('serial-output-content');

    if (btn) {
        btn.textContent = `Wrap: ${serialWrap ? 'ON' : 'OFF'}`;
        btn.classList.toggle('active', serialWrap);
    }

    if (output) {
        output.style.whiteSpace = serialWrap ? 'pre-wrap' : 'pre';
        output.style.wordWrap = serialWrap ? 'break-word' : 'normal';
    }

    localStorage.setItem('serialWrap', serialWrap);
}

// Pause/resume serial output
function pauseSerial() {
    serialPaused = !serialPaused;
    const btn = document.getElementById('pause-btn');

    if (btn) {
        btn.innerHTML = serialPaused ?
            '<span class="btn-icon">▶️</span> Resume' :
            '<span class="btn-icon">⏸️</span> Pause';
        btn.classList.toggle('active', serialPaused);
    }

    if (serialPaused) {
        addSerialLine('Serial output paused...');
    } else {
        addSerialLine('Serial output resumed...');
    }
}

// Export serial output
function exportSerial() {
    const serialOutput = document.getElementById('serial-output-content');
    if (!serialOutput) return;

    const lines = serialOutput.querySelectorAll('.serial-line');
    let exportText = `Serial Monitor Export - ${new Date().toLocaleString()}\n`;
    exportText += `Lines: ${serialLineCount}\n`;
    if (webserialConnectionTime) {
        const uptime = Math.floor((Date.now() - webserialConnectionTime) / 1000);
        exportText += `WebSocket Connected: ${formatUptime(uptime)}\n`;
    } else {
        exportText += `WebSocket Connected: 0s\n`;
    }
    exportText += '='.repeat(50) + '\n\n';

    lines.forEach(line => {
        const timestamp = line.querySelector('.serial-timestamp')?.textContent || '';
        const content = line.querySelector('.serial-content')?.textContent || '';
        exportText += `${timestamp} ${content}\n`;
    });

    // Create and download file
    const blob = new Blob([exportText], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `serial-monitor-${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);

    addSerialLine('Serial output exported to file...');
}

// Copy status section to clipboard
async function copyStatusSection(button) {
    const statusItem = button.closest('.status-item');
    if (!statusItem) return;

    // Clone the status item and remove the copy button
    const clone = statusItem.cloneNode(true);
    const copyBtn = clone.querySelector('.status-item-copy');
    if (copyBtn) {
        copyBtn.remove();
    }

    // Extract text content, preserving structure
    const title = clone.querySelector('h3')?.textContent || '';
    const details = clone.querySelector('.status-details');

    let text = title + '\n';
    if (details) {
        const paragraphs = details.querySelectorAll('p');
        paragraphs.forEach(p => {
            // Extract text, handling nested spans and indicators
            const textContent = p.textContent.trim();
            if (textContent) {
                text += textContent.replace(/\s+/g, ' ') + '\n';
            }
        });
    }

    // Remove trailing newline
    text = text.trim();

    try {
        await navigator.clipboard.writeText(text);

        // Visual feedback
        button.classList.add('copied');
        const originalText = button.textContent;
        button.textContent = 'Copied!';

        setTimeout(() => {
            button.classList.remove('copied');
            button.textContent = originalText;
        }, 2000);
    } catch (err) {
        console.error('Failed to copy:', err);
        // Fallback for older browsers
        const textArea = document.createElement('textarea');
        textArea.value = text;
        textArea.style.position = 'fixed';
        textArea.style.opacity = '0';
        document.body.appendChild(textArea);
        textArea.select();
        try {
            document.execCommand('copy');
            button.textContent = 'Copied!';
            setTimeout(() => {
                button.textContent = 'Copy';
            }, 2000);
        } catch (fallbackErr) {
            console.error('Fallback copy failed:', fallbackErr);
            button.textContent = 'Failed';
            setTimeout(() => {
                button.textContent = 'Copy';
            }, 2000);
        }
        document.body.removeChild(textArea);
    }
}

// Format uptime
function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (days > 0) {
        return `${days}d ${hours}h ${minutes}m`;
    } else if (hours > 0) {
        return `${hours}h ${minutes}m ${secs}s`;
    } else if (minutes > 0) {
        return `${minutes}m ${secs}s`;
    } else {
        return `${secs}s`;
    }
}

// Format bytes
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// Refresh status manually
function refreshStatus() {
    loadStatus();
}

// Update status interval
function updateStatusInterval(interval) {
    const selectedInterval = parseInt(interval, 10);
    const validInterval = Number.isFinite(selectedInterval) && selectedInterval >= 1000
        ? selectedInterval : 2000;
    localStorage.setItem("statusInterval", String(validInterval));

    // The setting is edited on Controls but only starts polling on Status.
    // This prevents a background API loop on a page with no status elements.
    if (currentPage !== "status") return;
    if (statusUpdateInterval) clearInterval(statusUpdateInterval);
    statusUpdateInterval = setInterval(loadStatus, validInterval);
}

// Serial interval removed - WebSocket provides real-time updates

// Load saved intervals from localStorage
function loadSavedIntervals() {
    const savedStatusInterval = localStorage.getItem('statusInterval');

    const statusIntervalSelect = document.getElementById("status-interval");
    if (savedStatusInterval && statusIntervalSelect) {
        statusIntervalSelect.value = savedStatusInterval;
    }

    // Load serial monitor settings
    const savedAutoScroll = localStorage.getItem('autoScrollSerial');
    if (savedAutoScroll !== null) {
        autoScrollSerial = savedAutoScroll === 'true';
        const btn = document.getElementById('auto-scroll-btn');
        if (btn) {
            btn.textContent = `Auto Scroll: ${autoScrollSerial ? 'ON' : 'OFF'}`;
            btn.classList.toggle('active', autoScrollSerial);
        }
    }

    const savedWrap = localStorage.getItem('serialWrap');
    if (savedWrap !== null) {
        serialWrap = savedWrap === 'true';
        const btn = document.getElementById('wrap-btn');
        const output = document.getElementById('serial-output-content');

        if (btn) {
            btn.textContent = `Wrap: ${serialWrap ? 'ON' : 'OFF'}`;
            btn.classList.toggle('active', serialWrap);
        }

        if (output) {
            output.style.whiteSpace = serialWrap ? 'pre-wrap' : 'pre';
            output.style.wordWrap = serialWrap ? 'break-word' : 'normal';
        }
    }
}

// Control functions
async function updateDevicePassword(event) {
    if (event) {
        event.preventDefault();
    }

    const passwordInput = document.getElementById('device-password');
    const confirmInput = document.getElementById('device-password-confirm');
    const password = passwordInput.value;
    const confirmation = confirmInput.value;

    if (password !== confirmation) {
        showError('The password confirmation does not match.');
        return;
    }
    if (password.length > 0 && (password.length < 8 || password.length > 31)) {
        showError('A device password must be 8–31 characters, or empty to disable it.');
        return;
    }
    if (password.length === 0 && !confirm('Remove the device password? Local protected services will be open after restart.')) {
        return;
    }

    try {
        const body = new URLSearchParams({
            new_password: password,
            confirm_password: confirmation
        });
        if (password.length === 0) body.set('allow_empty', 'true');
        const response = await fetch('/api/device-password', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body.toString()
        });
        if (!response.ok) {
            const result = await response.json().catch(() => ({}));
            throw new Error(result.message || 'Unable to update the device password');
        }
        passwordInput.value = '';
        confirmInput.value = '';
        showSuccess('Device password saved. The device is restarting. Update your local profile before the next OTA upload.');
    } catch (error) {
        console.error('Error updating device password:', error);
        showError(error.message || 'Unable to update the device password');
    }
}

async function restartDevice() {
    if (confirm('Are you sure you want to restart the device?')) {
        try {
            const response = await fetch('/api/control', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'restart' })
            });

            if (response.ok) {
                showSuccess('Restart command sent!');
            } else {
                throw new Error('Failed to send restart command');
            }
        } catch (error) {
            console.error('Error restarting device:', error);
            showError('Failed to send restart command');
        }
    }
}

async function resetToDefaults() {
    if (confirm('Are you sure you want to reset all settings to defaults?')) {
        try {
            const response = await fetch('/api/control', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'reset' })
            });

            if (response.ok) {
                showSuccess('Reset command sent!');
            } else {
                throw new Error('Failed to send reset command');
            }
        } catch (error) {
            console.error('Error resetting device:', error);
            showError('Failed to send reset command');
        }
    }
}

async function factoryReset() {
    if (confirm('Factory reset WiFi and all framework configuration?')) {
        try {
            const response = await fetch('/api/control', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'factory_reset' })
            });

            if (response.ok) {
                showSuccess('Factory reset started; device is restarting.');
            } else {
                throw new Error('Failed to send factory reset command');
            }
        } catch (error) {
            console.error('Error during factory reset:', error);
            showError('Failed to send factory reset command');
        }
    }
}

// Utility functions
function showSuccess(message) {
    showNotification(message, 'success');
}

function showError(message) {
    showNotification(message, 'error');
}

function showNotification(message, type) {
    // Create notification element
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.textContent = message;

    // Style the notification
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 1rem 1.5rem;
        border-radius: 8px;
        color: white;
        font-weight: 500;
        z-index: 1000;
        animation: slideInRight 0.3s ease-out;
        max-width: 300px;
        word-wrap: break-word;
    `;

    if (type === 'success') {
        notification.style.background = 'linear-gradient(135deg, #38a169, #2f855a)';
    } else {
        notification.style.background = 'linear-gradient(135deg, #e53e3e, #c53030)';
    }

    // Add to page
    document.body.appendChild(notification);

    // Remove after 3 seconds
    setTimeout(() => {
        notification.style.animation = 'slideOutRight 0.3s ease-in';
        setTimeout(() => {
            if (notification.parentNode) {
                notification.parentNode.removeChild(notification);
            }
        }, 300);
    }, 3000);
}

// Add CSS animations for notifications
const style = document.createElement('style');
style.textContent = `
    @keyframes slideInRight {
        from {
            transform: translateX(100%);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }

    @keyframes slideOutRight {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(100%);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);
)rawliteral";

#endif // WEBINTERFACE_JS_H
