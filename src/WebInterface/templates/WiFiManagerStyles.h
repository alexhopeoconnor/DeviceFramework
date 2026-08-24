#ifndef WIFIMANAGER_STYLES_H
#define WIFIMANAGER_STYLES_H

#include <Arduino.h>

// Custom CSS styles for WiFiManager config portal
// Themed to match the DeviceFramework web interface with purple gradient and glassmorphism
// Note: Not using PROGMEM because WiFiManager's String concatenation expects RAM pointers
const char wifimanager_custom_css[] = R"rawliteral(
<style>
/* Override WiFiManager defaults with DeviceFramework theme */

/* Background - purple gradient matching web interface */
body {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%) !important;
    min-height: 100vh;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif !important;
}

/* Main container - glassmorphism card effect */
.wrap {
    background: rgba(255, 255, 255, 0.95) !important;
    backdrop-filter: blur(20px);
    border-radius: 24px !important;
    padding: 2.5rem !important;
    box-shadow: 0 25px 80px rgba(0, 0, 0, 0.08) !important;
    border: 1px solid rgba(255, 255, 255, 0.2);
    margin: 2rem auto !important;
    max-width: 500px !important;
}

/* Headings */
h1 {
    color: #2d3748 !important;
    font-size: 1.75rem !important;
    font-weight: 600 !important;
    letter-spacing: -0.025em;
    margin-bottom: 1.5rem !important;
}

h3 {
    color: #4a5568 !important;
    font-size: 1.25rem !important;
    font-weight: 600 !important;
    margin-bottom: 1rem !important;
}

/* Input fields - modern styling */
input:not([type='checkbox']):not([type='radio']), select {
    background: rgba(255, 255, 255, 0.9) !important;
    border: 1px solid #e2e8f0 !important;
    border-radius: 12px !important;
    padding: 0.875rem 1.25rem !important;
    font-size: 0.95rem !important;
    transition: all 0.3s ease !important;
    color: #2d3748 !important;
}

input:not([type='checkbox']):not([type='radio']):focus, select:focus {
    outline: none !important;
    border-color: #4299e1 !important;
    box-shadow: 0 0 0 3px rgba(66, 153, 225, 0.1) !important;
    background: rgba(255, 255, 255, 1) !important;
}

/* Labels */
label {
    color: #4a5568 !important;
    font-weight: 600 !important;
    font-size: 0.9rem !important;
    margin-bottom: 0.5rem !important;
    display: block !important;
}

/* Buttons - gradient style matching web interface */
button, input[type='submit'], input[type='button'] {
    background: linear-gradient(135deg, #4299e1, #3182ce) !important;
    border: none !important;
    border-radius: 12px !important;
    padding: 0.875rem 1.75rem !important;
    font-weight: 600 !important;
    font-size: 0.95rem !important;
    transition: all 0.3s ease !important;
    box-shadow: 0 4px 12px rgba(66, 153, 225, 0.2) !important;
    color: white !important;
    cursor: pointer !important;
    margin: 0.5rem 0.25rem !important;
}

button:hover, input[type='submit']:hover, input[type='button']:hover {
    background: linear-gradient(135deg, #3182ce, #2c5282) !important;
    transform: translateY(-2px);
    box-shadow: 0 8px 25px rgba(66, 153, 225, 0.3) !important;
}

button:active, input[type='submit']:active, input[type='button']:active {
    transform: translateY(0);
}

/* Danger button (Erase) */
button.D {
    background: linear-gradient(135deg, #e53e3e, #c53030) !important;
}

button.D:hover {
    background: linear-gradient(135deg, #c53030, #9c2626) !important;
    box-shadow: 0 8px 25px rgba(229, 62, 62, 0.3) !important;
}

/* Password show/hide checkbox styling */
input[type='checkbox'] {
    width: auto !important;
    margin: 0 0.5rem 0 0 !important;
    cursor: pointer;
    accent-color: #4299e1;
}

label[for='showpass'], label.password-toggle-label {
    display: inline !important;
    color: #4a5568 !important;
    font-weight: 500 !important;
    cursor: pointer;
    font-size: 0.875rem !important;
}

.password-toggle-checkbox {
    width: auto !important;
    margin: 0 0.5rem 0 0 !important;
    cursor: pointer;
    accent-color: #4299e1;
}

/* WiFi signal quality indicators */
.q {
    color: #4299e1 !important;
    font-weight: 600 !important;
}

/* WiFi SSID list items */
div > a {
    color: #2d3748 !important;
    font-weight: 600 !important;
    text-decoration: none !important;
    transition: color 0.3s ease !important;
}

div > a:hover {
    color: #4299e1 !important;
}

/* Links */
a {
    color: #4299e1 !important;
    transition: color 0.3s ease !important;
}

a:hover {
    color: #3182ce !important;
}

/* Message boxes */
.msg {
    background: rgba(255, 255, 255, 0.9) !important;
    border-radius: 12px !important;
    border-left-width: 4px !important;
    padding: 1.5rem !important;
    margin: 1.5rem 0 !important;
}

.msg.S {
    border-left-color: #38a169 !important;
    background: rgba(56, 161, 105, 0.05) !important;
}

.msg.S h4 {
    color: #38a169 !important;
}

.msg.P {
    border-left-color: #4299e1 !important;
    background: rgba(66, 153, 225, 0.05) !important;
}

.msg.P h4 {
    color: #4299e1 !important;
}

.msg.D {
    border-left-color: #e53e3e !important;
    background: rgba(229, 62, 62, 0.05) !important;
}

.msg.D h4 {
    color: #e53e3e !important;
}

/* Horizontal rule */
hr {
    border: none !important;
    border-top: 1px solid rgba(226, 232, 240, 0.5) !important;
    margin: 1.5rem 0 !important;
}

/* Form elements spacing */
form {
    margin: 0 !important;
}

br {
    display: block !important;
    content: "" !important;
    margin-top: 0.5rem !important;
}

/* List items (info page) */
dt {
    font-weight: 600 !important;
    color: #2d3748 !important;
    margin-top: 0.75rem !important;
}

dd {
    color: #4a5568 !important;
    margin: 0.25rem 0 0.75rem 0 !important;
    padding-left: 0 !important;
}

/* Tables */
table {
    width: 100% !important;
    border-collapse: collapse !important;
}

th {
    background: rgba(66, 153, 225, 0.1) !important;
    color: #2d3748 !important;
    font-weight: 600 !important;
    padding: 0.75rem !important;
    text-align: left !important;
    border-radius: 8px 8px 0 0 !important;
}

td {
    padding: 0.75rem !important;
    border-bottom: 1px solid rgba(226, 232, 240, 0.5) !important;
    color: #4a5568 !important;
}

/* Progress bars */
progress {
    width: 100% !important;
    height: 8px !important;
    border-radius: 4px !important;
    overflow: hidden !important;
    background: #e2e8f0 !important;
    border: none !important;
}

progress::-webkit-progress-bar {
    background: #e2e8f0 !important;
    border-radius: 4px !important;
}

progress::-webkit-progress-value {
    background: linear-gradient(90deg, #4299e1, #3182ce) !important;
    border-radius: 4px !important;
}

progress::-moz-progress-bar {
    background: linear-gradient(90deg, #4299e1, #3182ce) !important;
    border-radius: 4px !important;
}

/* Mobile responsiveness */
@media (max-width: 768px) {
    .wrap {
        margin: 1rem !important;
        padding: 1.5rem !important;
        border-radius: 16px !important;
    }

    h1 {
        font-size: 1.5rem !important;
    }

    button, input[type='submit'], input[type='button'] {
        padding: 1rem 1.5rem !important;
        font-size: 1rem !important;
    }
}
</style>

<script>
// Add password toggles for custom password fields
// WiFiManager already provides a toggle for WiFi password (id='p'),
// but we need to add toggles for other password fields (like MQTT password)
window.addEventListener('DOMContentLoaded', function() {
    // Find all password inputs
    var passwordInputs = document.querySelectorAll('input[type="password"]');

    passwordInputs.forEach(function(input) {
        // Skip the WiFi password field - it already has WiFiManager's built-in toggle
        if (input.id === 'p') return;

        // Create unique toggle ID
        var toggleId = 'toggle_' + input.id;

        // Create toggle checkbox
        var checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.id = toggleId;
        checkbox.className = 'password-toggle-checkbox';
        checkbox.onclick = function() {
            input.type = input.type === 'password' ? 'text' : 'password';
        };

        // Create label
        var label = document.createElement('label');
        label.htmlFor = toggleId;
        label.className = 'password-toggle-label';
        label.textContent = 'Show Password';

        // Insert toggle after the password input
        var nextElement = input.nextSibling;
        var parent = input.parentNode;

        // Add a line break first
        var br = document.createElement('br');
        parent.insertBefore(br, nextElement);
        parent.insertBefore(checkbox, nextElement);
        parent.insertBefore(label, nextElement);
    });
});
</script>
)rawliteral";

#endif // WIFIMANAGER_STYLES_H
