#ifndef WEBINTERFACE_CSS_H
#define WEBINTERFACE_CSS_H

#include <Arduino.h>

// CSS Styles for DeviceFramework Web Interface
const char PROGMEM css_styles[] = R"rawliteral(
/* Modern CSS Reset and Base Styles */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

html {
    font-size: 16px;
    scroll-behavior: smooth;
    scroll-padding-top: 120px; /* Offset for sticky header and nav */
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
    line-height: 1.6;
    color: #2d3748;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    min-height: 100vh;
    overflow-x: hidden;
}

/* Header Styles */
.header {
    background: rgba(255, 255, 255, 0.95);
    backdrop-filter: blur(20px);
    box-shadow: 0 4px 32px rgba(0, 0, 0, 0.1);
    padding: 1rem 0;
    position: sticky;
    top: 0;
    z-index: 100;
    border-bottom: 1px solid rgba(255, 255, 255, 0.2);
}

.header-content {
    max-width: 1200px;
    margin: 0 auto;
    display: flex;
    align-items: center;
    gap: 1rem;
    padding: 0 1rem;
}

.logo {
    height: 48px;
    width: auto;
    border-radius: 8px;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.header h1 {
    color: #2d3748;
    font-size: 1.75rem;
    font-weight: 600;
    letter-spacing: -0.025em;
}

/* Navigation Styles */
.nav {
    max-width: 1200px;
    margin: 0 auto;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 1rem;
    background: rgba(255, 255, 255, 0.1);
    border-radius: 12px;
    padding: 0.5rem 1rem;
    margin-top: 0.5rem;
    position: relative;
}

.nav-toggle {
    display: none;
    flex-direction: column;
    background: none;
    border: none;
    cursor: pointer;
    padding: 0.5rem;
    border-radius: 8px;
    transition: background 0.3s ease;
}

.nav-toggle:hover {
    background: rgba(255, 255, 255, 0.1);
}

.nav-toggle span {
    width: 25px;
    height: 3px;
    background: #4a5568;
    margin: 3px 0;
    transition: 0.3s;
    border-radius: 2px;
}

.nav-toggle.active span:nth-child(1) {
    transform: rotate(-45deg) translate(-5px, 6px);
}

.nav-toggle.active span:nth-child(2) {
    opacity: 0;
}

.nav-toggle.active span:nth-child(3) {
    transform: rotate(45deg) translate(-5px, -6px);
}

.nav-menu {
    display: flex;
    gap: 0.5rem;
    flex-wrap: wrap;
}

.nav-link {
    color: #2d3748;
    text-decoration: none;
    padding: 0.75rem 1.25rem;
    border-radius: 8px;
    transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    font-weight: 600;
    font-size: 0.95rem;
    position: relative;
    overflow: hidden;
    background: rgba(255, 255, 255, 0.2);
    border: 1px solid rgba(255, 255, 255, 0.3);
    text-align: center;
    min-width: 120px;
}

.nav-link:hover {
    background: rgba(255, 255, 255, 0.4);
    color: #1a202c;
    transform: translateY(-2px);
    border-color: rgba(255, 255, 255, 0.5);
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
}

.nav-link.active {
    background: rgba(255, 255, 255, 0.95);
    color: #1a202c;
    border-color: rgba(255, 255, 255, 0.6);
    font-weight: 700;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

/* Container and Layout */
.container {
    max-width: 1200px;
    margin: 2rem auto;
    padding: 0 1rem;
    min-height: calc(100vh - 200px);
}

/* Card Styles */
.card {
    background: rgba(255, 255, 255, 0.95);
    backdrop-filter: blur(20px);
    border-radius: 24px;
    padding: 2.5rem;
    margin-bottom: 2rem;
    box-shadow: 0 25px 80px rgba(0, 0, 0, 0.08);
    border: 1px solid rgba(255, 255, 255, 0.2);
    transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
    position: relative;
    overflow: hidden;
    user-select: text !important;
    -webkit-user-select: text !important;
    -moz-user-select: text !important;
    -ms-user-select: text !important;
}

.card::before {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 3px;
    background: linear-gradient(90deg, #4299e1, #3182ce, #2c5282);
    transform: scaleX(0);
    transform-origin: left;
    transition: transform 0.6s ease;
}

.card:hover {
    transform: translateY(-6px);
    box-shadow: 0 35px 100px rgba(0, 0, 0, 0.12);
}

.card:hover::before {
    transform: scaleX(1);
}

.card h2 {
    color: #2d3748;
    margin-bottom: 1.5rem;
    font-size: 1.75rem;
    font-weight: 600;
    letter-spacing: -0.025em;
}

/* Status Grid */
.status-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 1.5rem;
    margin-bottom: 2rem;
    user-select: text !important;
    -webkit-user-select: text !important;
    -moz-user-select: text !important;
    -ms-user-select: text !important;
}

.status-item {
    background: rgba(255, 255, 255, 0.9);
    padding: 1.5rem;
    border-radius: 16px;
    border-left: 4px solid #4299e1;
    transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
    position: relative;
    overflow: hidden;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.06);
    user-select: text !important;
    -webkit-user-select: text !important;
    -moz-user-select: text !important;
    -ms-user-select: text !important;
}

.status-item * {
    user-select: text !important;
    -webkit-user-select: text !important;
    -moz-user-select: text !important;
    -ms-user-select: text !important;
}

.status-details {
    user-select: text !important;
    -webkit-user-select: text !important;
    -moz-user-select: text !important;
    -ms-user-select: text !important;
    position: relative;
    z-index: 2;
}

.status-details * {
    user-select: text !important;
    -webkit-user-select: text !important;
    -moz-user-select: text !important;
    -ms-user-select: text !important;
    position: relative;
    z-index: 2;
}

/* Copy button for status items */
.status-item-copy {
    position: absolute;
    top: 0.75rem;
    right: 0.75rem;
    background: rgba(66, 153, 225, 0.1);
    border: 1px solid rgba(66, 153, 225, 0.2);
    color: #4299e1;
    padding: 0.375rem 0.75rem;
    border-radius: 6px;
    font-size: 0.75rem;
    font-weight: 500;
    cursor: pointer;
    transition: all 0.2s ease;
    opacity: 0;
    pointer-events: none;
    z-index: 10;
    user-select: none !important;
    -webkit-user-select: none !important;
    -moz-user-select: none !important;
    -ms-user-select: none !important;
}

.status-item:hover .status-item-copy {
    opacity: 1;
    pointer-events: auto;
}

.status-item-copy:hover {
    background: rgba(66, 153, 225, 0.2);
    border-color: rgba(66, 153, 225, 0.4);
    transform: translateY(-1px);
}

.status-item-copy:active {
    transform: translateY(0);
    background: rgba(66, 153, 225, 0.3);
}

.status-item-copy.copied {
    background: rgba(34, 197, 94, 0.2);
    border-color: rgba(34, 197, 94, 0.4);
    color: #22c55e;
}

.status-item-copy.copied::after {
    content: ' ✓';
}

.status-item::before {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 3px;
    background: linear-gradient(90deg, #4299e1, #3182ce, #2c5282);
    transform: scaleX(0);
    transform-origin: left;
    transition: transform 0.5s ease;
    pointer-events: none;
    z-index: 1;
}

.status-item::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: linear-gradient(135deg, rgba(66, 153, 225, 0.02), rgba(49, 130, 206, 0.02));
    opacity: 0;
    transition: opacity 0.3s ease;
    pointer-events: none;
    z-index: 1;
}

.status-item:hover::before {
    transform: scaleX(1);
}

.status-item:hover::after {
    opacity: 1;
}

.status-item:hover {
    transform: translateY(-4px);
    box-shadow: 0 20px 60px rgba(66, 153, 225, 0.12);
    border-left-color: #3182ce;
}

.status-item h3 {
    color: #2d3748;
    margin-bottom: 1rem;
    font-size: 1.25rem;
    font-weight: 600;
    position: relative;
    z-index: 2;
}

.status-item p {
    margin-bottom: 0.75rem;
    color: #4a5568;
    font-size: 0.95rem;
}

.status-item strong {
    color: #2d3748;
    font-weight: 600;
}

/* Button Styles */
.btn {
    background: linear-gradient(135deg, #4299e1, #3182ce);
    color: white;
    border: none;
    padding: 0.875rem 1.75rem;
    border-radius: 12px;
    cursor: pointer;
    font-size: 0.95rem;
    font-weight: 500;
    transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    margin: 0.25rem;
    text-decoration: none;
    display: inline-block;
    position: relative;
    overflow: hidden;
}

.btn::before {
    content: '';
    position: absolute;
    top: 0;
    left: -100%;
    width: 100%;
    height: 100%;
    background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.2), transparent);
    transition: left 0.5s;
}

.btn:hover::before {
    left: 100%;
}

.btn:hover {
    background: linear-gradient(135deg, #3182ce, #2c5282);
    transform: translateY(-2px);
    box-shadow: 0 8px 25px rgba(66, 153, 225, 0.3);
}

.btn:active {
    transform: translateY(0);
}

.btn-danger {
    background: linear-gradient(135deg, #e53e3e, #c53030);
}

.btn-danger:hover {
    background: linear-gradient(135deg, #c53030, #9c2626);
    box-shadow: 0 8px 25px rgba(229, 62, 62, 0.3);
}

.btn-warning {
    background: linear-gradient(135deg, #ed8936, #dd6b20);
}

.btn-warning:hover {
    background: linear-gradient(135deg, #dd6b20, #c05621);
    box-shadow: 0 8px 25px rgba(237, 137, 54, 0.3);
}

.btn-success {
    background: linear-gradient(135deg, #38a169, #2f855a);
}

.btn-success:hover {
    background: linear-gradient(135deg, #2f855a, #276749);
    box-shadow: 0 8px 25px rgba(56, 161, 105, 0.3);
}

/* Control Panel */
.control-panel {
    display: block;
    margin-top: 0;
}

.control-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
}

.control-group {
    background: rgba(255, 255, 255, 0.8);
    padding: 1.75rem;
    border-radius: 16px;
    border-left: 4px solid #ed8936;
}

.control-group h3 {
    color: #2d3748;
    margin-bottom: 1rem;
    font-size: 1.25rem;
    font-weight: 600;
}

.control-group .btn {
    width: 100%;
    margin: 0.5rem 0;
    text-align: center;
}

/* Professional Terminal Serial Monitor */
.serial-monitor {
    background: #0d1117;
    border-radius: 0 0 8px 8px;
    overflow: hidden;
    border: 1px solid #30363d;
    border-top: none;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
    font-family: 'SF Mono', 'Monaco', 'Inconsolata', 'Roboto Mono', 'Courier New', monospace;
}

/* Terminal Header */
.terminal-header {
    background: #0d1117;
    border-bottom: 1px solid #21262d;
    padding: 0.75rem 1rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 0.5rem;
    border-radius: 8px 8px 0 0;
    position: relative;
}

.terminal-header::after {
    content: '';
    position: absolute;
    bottom: 0;
    left: 0;
    right: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent 0%, #30363d 20%, #30363d 80%, transparent 100%);
}

.terminal-title {
    display: flex;
    align-items: center;
    gap: 0.75rem;
}

.terminal-label {
    color: #f0f6fc;
    font-weight: 600;
    font-size: 0.9rem;
    letter-spacing: 0.025em;
}

.terminal-stats {
    display: flex;
    gap: 1rem;
    align-items: center;
}

.terminal-stat {
    color: #8b949e;
    font-size: 0.8rem;
    font-weight: 500;
    background: rgba(110, 118, 129, 0.08);
    padding: 0.25rem 0.5rem;
    border-radius: 4px;
    border: 1px solid rgba(110, 118, 129, 0.15);
    font-family: 'SF Mono', 'Monaco', 'Inconsolata', 'Roboto Mono', 'Courier New', monospace;
}

/* Terminal Output Area */
.serial-output {
    background: #0d1117;
    color: #e6edf3;
    font-size: 0.9rem;
    line-height: 1.6;
    padding: 0 1rem 1rem 1rem;
    height: 350px;
    overflow-y: auto;
    white-space: pre-wrap;
    word-wrap: break-word;
    position: relative;
}

/* Add padding to first actual element instead of container */
.serial-output > .serial-line:first-of-type {
    margin-top: 0.5rem;
}


.serial-output::-webkit-scrollbar {
    width: 8px;
}

.serial-output::-webkit-scrollbar-track {
    background: #161b22;
    border-radius: 4px;
}

.serial-output::-webkit-scrollbar-thumb {
    background: #30363d;
    border-radius: 4px;
    border: 1px solid #21262d;
}

.serial-output::-webkit-scrollbar-thumb:hover {
    background: #484f58;
}

/* Terminal Lines */
.serial-line {
    margin-bottom: 0.125rem;
    padding: 0.125rem 0;
    display: flex;
    align-items: flex-start;
    gap: 0.5rem;
    transition: all 0.2s ease;
    border-left: 2px solid transparent;
    padding-left: 0.25rem;
}

.serial-line:last-child {
    margin-bottom: 0;
}

.serial-timestamp {
    color: #8b949e;
    font-weight: 500;
    flex-shrink: 0;
    min-width: 80px;
    font-size: 0.8rem;
    opacity: 0.8;
}

.serial-content {
    color: #e6edf3;
    flex: 1;
    word-break: break-word;
    font-weight: 400;
}

/* Terminal Line Types */
.serial-line[data-level="ERROR"] {
    border-left-color: #f85149;
    background: rgba(248, 81, 73, 0.05);
}

.serial-line[data-level="ERROR"] .serial-content {
    color: #f85149;
}

.serial-line[data-level="WARN"] {
    border-left-color: #d29922;
    background: rgba(210, 153, 34, 0.05);
}

.serial-line[data-level="WARN"] .serial-content {
    color: #d29922;
}

.serial-line[data-level="INFO"] {
    border-left-color: #58a6ff;
    background: rgba(88, 166, 255, 0.05);
}

.serial-line[data-level="INFO"] .serial-content {
    color: #58a6ff;
}

.serial-line[data-level="DEBUG"] {
    border-left-color: #8b949e;
    background: rgba(139, 148, 158, 0.05);
}

.serial-line[data-level="DEBUG"] .serial-content {
    color: #8b949e;
}

/* Terminal Animation for new serial lines */
.serial-line-new {
    background: rgba(88, 166, 255, 0.1) !important;
    border-left-color: #58a6ff !important;
    animation: terminalLineSlide 0.8s cubic-bezier(0.25, 0.46, 0.45, 0.94);
    will-change: transform, opacity;
    transform: translateZ(0);
}

@keyframes terminalLineSlide {
    0% {
        transform: translateZ(0) translateX(-12px);
        opacity: 0.6;
        background: rgba(88, 166, 255, 0.2) !important;
        border-left-color: #7c3aed !important;
        box-shadow: 0 0 12px rgba(88, 166, 255, 0.3);
    }
    70% {
        transform: translateZ(0) translateX(0);
        opacity: 1;
        background: rgba(88, 166, 255, 0.15) !important;
        border-left-color: #58a6ff !important;
        box-shadow: 0 0 6px rgba(88, 166, 255, 0.2);
    }
    100% {
        transform: translateZ(0) translateX(0);
        opacity: 1;
        background: rgba(88, 166, 255, 0.1) !important;
        border-left-color: #58a6ff !important;
        box-shadow: none;
    }
}

/* WebSocket status indicator */
.webserial-status {
    font-size: 0.8rem;
    font-weight: 500;
    margin-left: 0.5rem;
    padding: 0.25rem 0.5rem;
    border-radius: 4px;
    transition: all 0.3s ease;
    font-family: 'SF Mono', 'Monaco', 'Inconsolata', 'Roboto Mono', 'Courier New', monospace;
}

.webserial-connected {
    color: #7c3aed;
    background: rgba(124, 58, 237, 0.1);
    border: 1px solid rgba(124, 58, 237, 0.2);
    animation: statusPulse 2s infinite;
}

.webserial-disconnected {
    color: #f85149;
    background: rgba(248, 81, 73, 0.1);
    border: 1px solid rgba(248, 81, 73, 0.2);
}

@keyframes statusPulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.7; }
}

/* Terminal Controls */
.serial-controls {
    background: linear-gradient(135deg, #161b22 0%, #21262d 100%);
    padding: 1rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 1rem;
    border-top: 1px solid #30363d;
    flex-wrap: wrap;
}

.serial-control-group {
    display: flex;
    gap: 0.5rem;
    align-items: center;
}

.terminal-btn {
    background: linear-gradient(135deg, #21262d 0%, #30363d 100%);
    border: 1px solid #30363d;
    color: #c9d1d9;
    padding: 0.5rem 0.75rem;
    font-size: 0.8rem;
    border-radius: 6px;
    cursor: pointer;
    transition: all 0.2s ease;
    display: flex;
    align-items: center;
    gap: 0.375rem;
    font-weight: 500;
    min-height: 32px;
}

.terminal-btn:hover {
    background: linear-gradient(135deg, #30363d 0%, #484f58 100%);
    border-color: #484f58;
    color: #f0f6fc;
    transform: translateY(-1px);
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
}

.terminal-btn:active {
    transform: translateY(0);
    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.2);
}

.terminal-btn.active {
    background: linear-gradient(135deg, #7c3aed 0%, #8b5cf6 100%);
    border-color: #7c3aed;
    color: #ffffff;
}

.btn-icon {
    font-size: 0.9rem;
    line-height: 1;
}

.btn-sm {
    padding: 0.5rem 1rem;
    font-size: 0.85rem;
    border-radius: 8px;
}

/* Status Grid */
.status-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 1.5rem;
    margin-bottom: 1.5rem;
}

.status-item {
    background: rgba(255, 255, 255, 0.8);
    padding: 1.5rem;
    border-radius: 16px;
    border-left: 4px solid #4299e1;
    transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
}

.status-item:hover {
    transform: translateY(-2px);
    box-shadow: 0 12px 40px rgba(66, 153, 225, 0.15);
}

.status-item h3 {
    color: #2d3748;
    margin-bottom: 1rem;
    font-size: 1.1rem;
    font-weight: 600;
}

.status-item p {
    margin-bottom: 0.5rem;
    color: #4a5568;
    font-size: 0.9rem;
}

.status-item strong {
    color: #2d3748;
    font-weight: 600;
}

/* Status Indicators */
.status-indicator {
    display: inline-block;
    width: 12px;
    height: 12px;
    border-radius: 50%;
    margin-right: 0.5rem;
}

.status-indicator.connected {
    background: #38a169;
    box-shadow: 0 0 8px rgba(56, 161, 105, 0.5);
}

.status-indicator.disconnected {
    background: #e53e3e;
    box-shadow: 0 0 8px rgba(229, 62, 62, 0.5);
}

/* WiFi Signal Strength Bars */
.wifi-signal {
    display: inline-flex;
    align-items: flex-end;
    height: 16px;
    gap: 2px;
    margin-left: 8px;
    vertical-align: middle;
    position: relative;
}

.wifi-bar {
    width: 3px;
    background-color: #e2e8f0;
    transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    display: block;
    border-radius: 1px;
    position: relative;
    min-height: 2px;
}

.wifi-bar:nth-child(1) { height: 3px; }
.wifi-bar:nth-child(2) { height: 5px; }
.wifi-bar:nth-child(3) { height: 7px; }
.wifi-bar:nth-child(4) { height: 9px; }

.wifi-bar.active {
    background: linear-gradient(135deg, #38a169, #2f855a) !important;
    box-shadow: 0 0 4px rgba(56, 161, 105, 0.4);
}

.wifi-bar.medium {
    background: linear-gradient(135deg, #ed8936, #dd6b20) !important;
    box-shadow: 0 0 4px rgba(237, 137, 54, 0.4);
}

.wifi-bar.weak {
    background: linear-gradient(135deg, #e53e3e, #c53030) !important;
    box-shadow: 0 0 4px rgba(229, 62, 62, 0.4);
}

/* Memory Usage Bar */
.memory-bar {
    width: 100%;
    height: 6px;
    background: #e2e8f0;
    border-radius: 3px;
    overflow: hidden;
    margin: 0.25rem 0;
    position: relative;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.1);
}

.memory-bar-fill {
    height: 100%;
    background: linear-gradient(90deg, #38a169, #2f855a);
    border-radius: 4px;
    transition: width 0.5s ease;
    position: relative;
}

.memory-bar-fill::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: linear-gradient(90deg, transparent, rgba(255,255,255,0.3), transparent);
    animation: shimmer 2s infinite;
}

/* Flash Usage Bar */
.flash-bar {
    width: 100%;
    height: 6px;
    background: #e2e8f0;
    border-radius: 3px;
    overflow: hidden;
    margin: 0.25rem 0;
    position: relative;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.1);
}

.flash-bar-fill {
    height: 100%;
    background: linear-gradient(90deg, #3182ce, #2c5282);
    border-radius: 4px;
    transition: width 0.5s ease;
    position: relative;
}

.flash-bar-fill::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: linear-gradient(90deg, transparent, rgba(255,255,255,0.3), transparent);
    animation: shimmer 2s infinite;
}

@keyframes shimmer {
    0% { transform: translateX(-100%); }
    100% { transform: translateX(100%); }
}

/* Status Value Styling */
.status-value {
    font-weight: 500;
    color: #2d3748;
    transition: color 0.3s ease;
}

.status-value.connected {
    color: #38a169;
}

.status-value.disconnected {
    color: #e53e3e;
}

.status-value.loading {
    color: #a0aec0;
    font-style: italic;
}

.status-value.error {
    color: #e53e3e;
    font-style: italic;
}

/* Status Details */
.status-details {
    margin-top: 0.5rem;
}

.status-details p {
    margin-bottom: 0.5rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.status-details strong {
    min-width: 120px;
    color: #4a5568;
}

/* Page Loader */
.page-loader {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 9999;
    transition: opacity 0.5s ease, visibility 0.5s ease;
}

.page-loader.hidden {
    opacity: 0;
    visibility: hidden;
}

.page-loader-content {
    text-align: center;
    color: white;
}

.page-loader-spinner {
    width: 48px;
    height: 48px;
    border: 4px solid rgba(255, 255, 255, 0.3);
    border-radius: 50%;
    border-top-color: white;
    border-right-color: white;
    animation: spin 1s linear infinite;
    margin: 0 auto 1.5rem;
}

.page-loader-text {
    font-size: 1.1rem;
    font-weight: 500;
    letter-spacing: 0.025em;
}

/* Content Loading Animation */
.loading {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 2rem;
    color: #4a5568;
    font-size: 0.95rem;
    font-weight: 500;
}

.loading::before {
    content: '';
    display: inline-block;
    width: 24px;
    height: 24px;
    border: 3px solid rgba(66, 153, 225, 0.2);
    border-radius: 50%;
    border-top-color: #4299e1;
    border-right-color: #4299e1;
    animation: spin 0.8s linear infinite;
    margin-right: 0.75rem;
}

@keyframes spin {
    0% { transform: rotate(0deg); }
    100% { transform: rotate(360deg); }
}

@keyframes fadeInUp {
    from {
        opacity: 0;
        transform: translateY(30px);
    }
    to {
        opacity: 1;
        transform: translateY(0);
    }
}

@keyframes slideInFromLeft {
    from {
        opacity: 0;
        transform: translateX(-30px);
    }
    to {
        opacity: 1;
        transform: translateX(0);
    }
}

@keyframes slideInFromRight {
    from {
        opacity: 0;
        transform: translateX(30px);
    }
    to {
        opacity: 1;
        transform: translateX(0);
    }
}

@keyframes pulse {
    0%, 100% {
        opacity: 1;
    }
    50% {
        opacity: 0.7;
    }
}

@keyframes glow {
    0%, 100% {
        box-shadow: 0 0 20px rgba(66, 153, 225, 0.3);
    }
    50% {
        box-shadow: 0 0 30px rgba(66, 153, 225, 0.5);
    }
}

/* Add entrance animations to cards */
.card {
    animation: fadeInUp 0.6s ease-out;
}

.status-item {
    animation: slideInFromLeft 0.6s ease-out;
}

.status-item:nth-child(even) {
    animation: slideInFromRight 0.6s ease-out;
}

/* Loading state animations */
.status-value.loading {
    animation: pulse 1.5s infinite;
}

/* Connection status glow */
.status-value.connected {
    animation: glow 2s infinite;
}

/* Built-in product attribution. Content is fixed framework markup assembled
 * from setup-time escaped text; consumers cannot supply arbitrary HTML. */
.about-card {
    scroll-margin-top: 120px;
}

.about-summary {
    margin: 0;
    color: var(--df-muted);
    line-height: 1.6;
}

.about-links {
    display: flex;
    flex-wrap: wrap;
    gap: 0.7rem;
    margin: 1rem 0 0;
    font-weight: 600;
}

.about-links a {
    color: var(--df-accent);
}

/* Footer */
.footer {
    background: rgba(255, 255, 255, 0.9);
    backdrop-filter: blur(20px);
    text-align: center;
    padding: 2rem;
    margin-top: 4rem;
    color: #718096;
    border-top: 1px solid rgba(255, 255, 255, 0.2);
}

/* Mobile Responsive Design */
@media (max-width: 768px) {
    .header-content {
        flex-direction: column;
        text-align: center;
        gap: 0.75rem;
    }

    .header h1 {
        font-size: 1.5rem;
    }

    .nav {
        flex-direction: column;
        align-items: stretch;
        padding: 0.75rem 1rem;
        margin-top: 0.75rem;
    }

    .nav-toggle {
        display: flex;
        align-self: flex-end;
        margin-bottom: 0.5rem;
    }

    .nav-menu {
        position: absolute;
        top: 100%;
        right: 0;
        flex-direction: column;
        background: white;
        border: 1px solid #e2e8f0;
        border-radius: 8px;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        padding: 0.5rem;
        min-width: 150px;
        transform: translateY(-10px);
        opacity: 0;
        visibility: hidden;
        transition: all 0.3s ease;
        z-index: 1000;
        max-height: none;
        overflow: visible;
    }

    .nav-menu.open {
        transform: translateY(0);
        opacity: 1;
        visibility: visible;
    }

    .nav-link {
        text-align: center;
        padding: 0.75rem 1rem;
        font-size: 0.95rem;
        min-width: auto;
        width: 100%;
        margin: 0.125rem 0;
        border-radius: 4px;
    }

    .container {
        padding: 0 0.75rem;
        margin: 1rem auto;
    }

    .card {
        padding: 1.5rem;
        border-radius: 16px;
    }

    .status-grid {
        grid-template-columns: 1fr;
        gap: 1rem;
    }

    .status-item {
        padding: 1.5rem;
    }

    .control-grid {
        grid-template-columns: 1fr;
    }

    .btn {
        padding: 1rem 1.5rem;
        font-size: 1rem;
    }
}

@media (max-width: 480px) {
    .header h1 {
        font-size: 1.25rem;
    }

    .card h2 {
        font-size: 1.5rem;
    }

    .status-item h3 {
        font-size: 1.1rem;
    }

    .container {
        padding: 0 0.5rem;
    }

    .card {
        padding: 1.25rem;
    }

    .status-item {
        padding: 1.25rem;
    }
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
    body {
        background: linear-gradient(135deg, #1a202c 0%, #2d3748 100%);
    }

    .header, .card, .status-item, .control-group {
        background: rgba(26, 32, 44, 0.95);
        color: #e2e8f0;
    }

    .nav {
        background: rgba(26, 32, 44, 0.8);
        border: 1px solid rgba(255, 255, 255, 0.1);
    }

    .header h1, .card h2, .status-item h3 {
        color: #f7fafc;
    }

    .status-item p {
        color: #cbd5e0;
    }

    .nav-link {
        color: #cbd5e0;
        background: rgba(255, 255, 255, 0.05);
        border: 1px solid rgba(255, 255, 255, 0.1);
    }

    .nav-link:hover {
        background: rgba(255, 255, 255, 0.1);
        color: #f7fafc;
        border-color: rgba(255, 255, 255, 0.2);
    }

    .nav-link.active {
        background: rgba(255, 255, 255, 0.15);
        color: #f7fafc;
        border-color: rgba(255, 255, 255, 0.3);
    }

    .footer {
        background: rgba(26, 32, 44, 0.9);
        color: #a0aec0;
    }
}


/* Control Items */
.control-item {
    margin-bottom: 1rem;
}

.control-item:last-child {
    margin-bottom: 0;
}

.control-item label {
    display: block;
    font-weight: 600;
    color: #374151;
    margin-bottom: 0.5rem;
    font-size: 0.9rem;
}

.control-item select, .control-item input {
    width: 100%;
    padding: 0.5rem 0.75rem;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    background: white;
    font-size: 0.9rem;
    transition: border-color 0.2s ease;
}

.control-item select:focus, .control-item input:focus {
    outline: none;
    border-color: #3b82f6;
    box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.1);
}

/* Product theme layer. DeviceFrameworkUI emits only these semantic tokens. */
:root {
    --df-page-start: #667eea;
    --df-page-end: #764ba2;
    --df-surface: rgba(255, 255, 255, 0.95);
    --df-text: #2d3748;
    --df-muted: #4a5568;
    --df-border: rgba(255, 255, 255, 0.2);
    --df-accent: #4299e1;
    --df-accent-hover: #3182ce;
    --df-accent-text: #ffffff;
    --df-success: #38a169;
    --df-danger: #e53e3e;
    --df-radius: 12px;
    --df-radius-sm: 8px;
    --df-card-radius: 24px;
}

body {
    color: var(--df-text);
    background: linear-gradient(135deg, var(--df-page-start) 0%, var(--df-page-end) 100%);
}

.header, .card {
    background: var(--df-surface);
    border-color: var(--df-border);
}

.card {
    border-radius: var(--df-card-radius);
}

.logo, .nav-toggle, .nav-link {
    border-radius: var(--df-radius-sm);
}

.nav, .btn {
    border-radius: var(--df-radius);
}

.header h1, .nav-link {
    color: var(--df-text);
}

.brand-name:empty {
    display: none;
}

.brand-name {
    margin: 0 0 0.15rem;
    color: var(--df-muted);
    font-size: 0.78rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
}

.nav-toggle span {
    background: var(--df-muted);
}

.btn {
    background: linear-gradient(135deg, var(--df-accent), var(--df-accent-hover));
    color: var(--df-accent-text);
}

.btn-success {
    background: linear-gradient(135deg, var(--df-success), var(--df-success));
}

.btn-danger {
    background: linear-gradient(135deg, var(--df-danger), var(--df-danger));
}
)rawliteral";

#endif // WEBINTERFACE_CSS_H
