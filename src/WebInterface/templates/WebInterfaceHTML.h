#ifndef WEBINTERFACE_HTML_H
#define WEBINTERFACE_HTML_H

#include <Arduino.h>

// HTML Templates for DeviceFramework Web Interface

// ============================================================================
// COMMON TEMPLATES (Header, Nav, Footer)
// ============================================================================

// Header template
const char PROGMEM header_template[] = R"rawliteral(
<header class="header">
    <div class="header-content">
        <img src="/assets/deviceframework-logo" alt="%LOGO_ALT_TEXT%" class="logo">
        <div class="header-titles">
            <p class="brand-name">%BRAND_NAME%</p>
            <h1>%PAGE_TITLE%</h1>
        </div>
    </div>
    %NAV%
</header>
)rawliteral";

// Header template for 404 page (without navigation)
const char PROGMEM header_404_template[] = R"rawliteral(
<header class="header">
    <div class="header-content">
        <img src="/assets/deviceframework-logo" alt="%LOGO_ALT_TEXT%" class="logo">
        <div class="header-titles">
            <p class="brand-name">%BRAND_NAME%</p>
            <h1>%PAGE_TITLE_404%</h1>
        </div>
    </div>
</header>
)rawliteral";

// Navigation template
const char PROGMEM nav_template[] = R"rawliteral(
<nav class="nav">
    <button class="nav-toggle" onclick="toggleNav()" aria-label="Toggle navigation">
        <span></span>
        <span></span>
        <span></span>
    </button>
    <div class="nav-menu" id="nav-menu">
        <a href="#device-status" class="nav-link active">Device Status</a>
        <a href="#serial-output" class="nav-link">Serial Output</a>
        <a href="#controls" class="nav-link">Controls</a>
        %ABOUT_NAV%
    </div>
</nav>
)rawliteral";

// Footer template
const char PROGMEM footer_template[] = R"rawliteral(
<footer class="footer">
    <p>Powered by DeviceFramework.</p>
</footer>
)rawliteral";

// ============================================================================
// CONTENT TEMPLATES
// ============================================================================

// SPA Content template (main page content)
const char PROGMEM spa_content_template[] = R"rawliteral(
<div class="card" id="device-status">
    <h2>Device Status</h2>
    <div id="status-content">
        <div class="status-grid">
            <!-- WiFi & Network -->
            <div class="status-item">
                <button class="status-item-copy" onclick="copyStatusSection(this)" title="Copy section to clipboard">Copy</button>
                <h3>Network</h3>
                <div class="status-details">
                    <p><strong>WiFi:</strong> <span class="status-indicator" id="wifi-indicator"></span> <span id="wifi-status" class="status-value">Loading...</span></p>
                    <p><strong>SSID:</strong> <span id="wifi-ssid" class="status-value">Loading...</span></p>
                    <p><strong>IP Address:</strong> <span id="wifi-ip" class="status-value">Loading...</span></p>
                    <p><strong>Gateway:</strong> <span id="wifi-gateway" class="status-value">Loading...</span></p>
                    <p><strong>MAC:</strong> <span id="wifi-mac" class="status-value">Loading...</span></p>
                    <p><strong>Signal:</strong> <span id="wifi-rssi" class="status-value">Loading...</span> dBm
                        <span class="wifi-signal" id="wifi-signal-bars">
                            <span class="wifi-bar"></span>
                            <span class="wifi-bar"></span>
                            <span class="wifi-bar"></span>
                            <span class="wifi-bar"></span>
                        </span>
                    </p>
                    <p><strong>Channel:</strong> <span id="wifi-channel" class="status-value">Loading...</span></p>
                </div>
            </div>

            <!-- Device & System Status -->
            <div class="status-item">
                <button class="status-item-copy" onclick="copyStatusSection(this)" title="Copy section to clipboard">Copy</button>
                <h3>Device & System</h3>
                <div class="status-details">
                    <p><strong>Device:</strong> <span id="device-name" class="status-value">Loading...</span></p>
                    <p><strong>Version:</strong> <span id="device-version" class="status-value">Loading...</span></p>
                    <p><strong>Uptime:</strong> <span id="device-uptime" class="status-value">Loading...</span></p>
                    <p><strong>CPU:</strong> <span id="device-cpu-freq" class="status-value">Loading...</span> MHz</p>
                    <p><strong>Flash:</strong> <span id="device-flash-size" class="status-value">Loading...</span></p>
                    <p><strong>Config Mode:</strong> <span id="config-mode" class="status-value">Loading...</span></p>
                    <p><strong>Log Level:</strong> <span id="log-level" class="status-value">Loading...</span></p>
                </div>
            </div>

            <!-- System Resources -->
            <div class="status-item">
                <button class="status-item-copy" onclick="copyStatusSection(this)" title="Copy section to clipboard">Copy</button>
                <h3>Resources</h3>
                <div class="status-details">
                    <p><strong>Memory:</strong> <span id="memory-free" class="status-value">Loading...</span> / <span id="memory-total" class="status-value">Loading...</span></p>
                    <p><strong>Usage:</strong> <span id="memory-usage" class="status-value">Loading...</span>&#37;</p>
                    <div class="memory-bar">
                        <div class="memory-bar-fill" id="memory-bar-fill"></div>
                    </div>
                    <p><strong>Flash:</strong> <span id="flash-used" class="status-value">Loading...</span> / <span id="flash-total" class="status-value">Loading...</span></p>
                    <p><strong>Flash Usage:</strong> <span id="flash-usage" class="status-value">Loading...</span>&#37;</p>
                    <div class="flash-bar">
                        <div class="flash-bar-fill" id="flash-bar-fill"></div>
                    </div>
                    <p><strong>Sketch:</strong> <span id="device-sketch-size" class="status-value">Loading...</span></p>
                    <p><strong>Free Sketch Space:</strong> <span id="free-sketch-space" class="status-value">Loading...</span></p>
                </div>
            </div>

            <!-- MQTT Connection -->
            <div class="status-item">
                <button class="status-item-copy" onclick="copyStatusSection(this)" title="Copy section to clipboard">Copy</button>
                <h3>MQTT</h3>
                <div class="status-details">
                    <p><strong>Status:</strong> <span class="status-indicator" id="mqtt-indicator"></span> <span id="mqtt-status" class="status-value">Loading...</span></p>
                    <p><strong>Broker:</strong> <span id="mqtt-broker" class="status-value">Loading...</span></p>
                    <p><strong>Port:</strong> <span id="mqtt-port" class="status-value">Loading...</span></p>
                    <p><strong>User:</strong> <span id="mqtt-user" class="status-value">Loading...</span></p>
                </div>
            </div>
        </div>
    </div>
</div>

<div class="card" id="serial-output">
    <div class="terminal-header">
        <div class="terminal-title">
            <span class="terminal-label">Serial Monitor</span>
            <span id="webserial-status" class="webserial-status webserial-disconnected">● Disconnected</span>
        </div>
        <div class="terminal-stats">
            <span id="serial-line-count" class="terminal-stat">Lines: 0</span>
            <span id="serial-uptime" class="terminal-stat">Uptime: 0s</span>
        </div>
    </div>
    <div id="serial-monitor" class="serial-monitor">
        <div class="serial-output" id="serial-output-content">
        </div>
        <div class="serial-controls">
            <div class="serial-control-group">
                <button onclick="clearSerial()" class="btn btn-sm terminal-btn">
                    <span class="btn-icon">🗑️</span> Clear
                </button>
                <button onclick="toggleSerialAutoScroll()" class="btn btn-sm terminal-btn" id="auto-scroll-btn">
                    <span class="btn-icon">📜</span> Auto Scroll: ON
                </button>
                <button onclick="toggleSerialWrap()" class="btn btn-sm terminal-btn" id="wrap-btn">
                    <span class="btn-icon">↩️</span> Wrap: ON
                </button>
            </div>
            <div class="serial-control-group">
                <button onclick="exportSerial()" class="btn btn-sm terminal-btn">
                    <span class="btn-icon">💾</span> Export
                </button>
                <button onclick="pauseSerial()" class="btn btn-sm terminal-btn" id="pause-btn">
                    <span class="btn-icon">⏸️</span> Pause
                </button>
            </div>
        </div>
    </div>
</div>

<div class="card" id="controls">
    <h2>System Controls</h2>
    <div id="control-panel">
        <div class="control-grid">
            <div class="control-group">
                <h3>System Controls</h3>
                <button onclick="restartDevice()" class="btn btn-danger">Restart Device</button>
                <button onclick="resetToDefaults()" class="btn btn-warning">Reset Configuration</button>
                <button onclick="factoryReset()" class="btn btn-danger">Factory Reset</button>
            </div>

            <div class="control-group">
                <h3>Update Settings</h3>
                <div class="control-item">
                    <label for="status-interval">Status Update:</label>
                    <select id="status-interval" onchange="updateStatusInterval(this.value)">
                        <option value="1000">1 second</option>
                        <option value="2000" selected>2 seconds</option>
                        <option value="5000">5 seconds</option>
                        <option value="10000">10 seconds</option>
                        <option value="30000">30 seconds</option>
                    </select>
                </div>
                <!-- Serial interval removed - WebSocket provides real-time updates -->
            </div>

            <div class="control-group">
                <h3>Device Password</h3>
                <p>One optional password protects the provisioning AP, OTA, this web interface, and WebSerial.</p>
                <form id="device-password-form" onsubmit="updateDevicePassword(event)">
                    <input type="text" name="username" value="admin" autocomplete="username" hidden>
                    <div class="control-item">
                        <label for="device-password">New password (8–31 characters):</label>
                        <input id="device-password" type="password" autocomplete="new-password" maxlength="31">
                    </div>
                    <div class="control-item">
                        <label for="device-password-confirm">Confirm new password:</label>
                        <input id="device-password-confirm" type="password" autocomplete="new-password" maxlength="31">
                    </div>
                    <button type="submit" class="btn">Save Password and Restart</button>
                </form>
            </div>

            <div class="control-group">
                <h3>WiFi Controls</h3>
                <button onclick="enterConfigMode()" class="btn">Enter Config Mode</button>
                <button onclick="disconnectWiFi()" class="btn">Disconnect WiFi</button>
            </div>
        </div>
    </div>
</div>

%ABOUT_SECTION%
)rawliteral";

// 404 Content template
const char PROGMEM error404_content_template[] = R"rawliteral(
<div class="card">
    <div style="text-align: center; padding: 3rem 0;">
        <h1 style="font-size: 4rem; color: #e53e3e; margin-bottom: 1rem;">404</h1>
        <h2 style="color: #2d3748; margin-bottom: 1rem;">Page Not Found</h2>
        <p style="color: #4a5568; margin-bottom: 2rem; font-size: 1.1rem;">
            The page you're looking for doesn't exist or has been moved.
        </p>
        <a href="/" class="btn" style="display: inline-block; text-decoration: none;">
            Return to Home
        </a>
    </div>
</div>
)rawliteral";

// ============================================================================
// PAGE TEMPLATES
// ============================================================================

// Base template (used by all pages)
const char PROGMEM base_template[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%PAGE_TITLE%</title>
    <link rel="icon" type="image/png" href="data:image/x-icon;base64,%FAVICON_BASE64%">
    <link rel="stylesheet" href="/assets/deviceframework.css">
    %UI_THEME%
</head>
<body>
    <div id="page-loader" class="page-loader">
        <div class="page-loader-content">
            <div class="page-loader-spinner"></div>
            <div class="page-loader-text">Loading device interface...</div>
        </div>
    </div>
    %HEADER%
    <main class="container">
        %CONTENT%
    </main>
    %FOOTER%
    <script src="/assets/deviceframework.js" defer></script>
</body>
</html>
)rawliteral";

// 404 page template (simplified without navbar)
const char PROGMEM error404_template[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%PAGE_TITLE_404%</title>
    <link rel="icon" type="image/png" href="data:image/x-icon;base64,%FAVICON_BASE64%">
    <link rel="stylesheet" href="/assets/deviceframework.css">
    %UI_THEME%
</head>
<body>
    %HEADER_404%
    <main class="container">
        %404_CONTENT%
    </main>
    %FOOTER%
</body>
</html>
)rawliteral";

#endif // WEBINTERFACE_HTML_H
