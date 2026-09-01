#ifdef ENABLE_WEB_INTERFACE
#include "DeviceFrameworkWebSerial.h"
#include "../Utils/TimeUtils.h"
#include "../DeviceFrameworkDebug.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include <new>

// DeviceFrameworkCircularBuffer implementation
DeviceFrameworkCircularBuffer::DeviceFrameworkCircularBuffer(size_t size)
    : _bufferSize(size), _writePos(0), _readPos(0), _dataLength(0) {
    _buffer = new (std::nothrow) char[_bufferSize];
    if (!_buffer) {
        LOG_ERRORLN(F("CircularBuffer: Failed to allocate buffer"));
        _bufferSize = 0;
    }
}

DeviceFrameworkCircularBuffer::~DeviceFrameworkCircularBuffer() {
    delete[] _buffer;
}

bool DeviceFrameworkCircularBuffer::add(const char* data, size_t length) {
    if (!_buffer || length == 0) return false;

    // Check if we have enough space
    if (length > getAvailableSpace()) {
        return false; // Buffer full
    }

    // Add data in chunks if needed (circular write)
    size_t remaining = length;
    const char* src = data;

    while (remaining > 0) {
        size_t contiguousSpace = getContiguousWriteSpace();
        size_t toWrite = min(remaining, contiguousSpace);

        memcpy(_buffer + _writePos, src, toWrite);

        _writePos = (_writePos + toWrite) % _bufferSize;
        _dataLength += toWrite;
        remaining -= toWrite;
        src += toWrite;
    }

    return true;
}

String DeviceFrameworkCircularBuffer::extractAll() {
    if (_dataLength == 0) return "";

    String result = "";
    result.reserve(_dataLength);

    while (_dataLength > 0) {
        size_t contiguousRead = getContiguousReadSpace();
        size_t toRead = min(contiguousRead, _dataLength);

        // Extract chunk
        for (size_t i = 0; i < toRead; i++) {
            result += _buffer[(_readPos + i) % _bufferSize];
        }

        _readPos = (_readPos + toRead) % _bufferSize;
        _dataLength -= toRead;
    }

    return result;
}

size_t DeviceFrameworkCircularBuffer::getDataLength() const {
    return _dataLength;
}

bool DeviceFrameworkCircularBuffer::isNearFull() const {
    return _dataLength >= (_bufferSize * 0.8); // 80% full
}

bool DeviceFrameworkCircularBuffer::isReady() const {
    return _buffer != nullptr && _bufferSize > 0;
}

void DeviceFrameworkCircularBuffer::clear() {
    _writePos = 0;
    _readPos = 0;
    _dataLength = 0;
}

size_t DeviceFrameworkCircularBuffer::getAvailableSpace() const {
    return _bufferSize - _dataLength;
}

size_t DeviceFrameworkCircularBuffer::getContiguousWriteSpace() const {
    if (_writePos >= _readPos) {
        return _bufferSize - _writePos;
    } else {
        return _readPos - _writePos;
    }
}

size_t DeviceFrameworkCircularBuffer::getContiguousReadSpace() const {
    if (_dataLength == 0) {
        return 0;
    }
    if (_readPos < _writePos) {
        return _writePos - _readPos;
    }
    return _bufferSize - _readPos;
}

// Static member definitions
AsyncWebSocket* DeviceFrameworkWebSerial::_ws = nullptr;
bool DeviceFrameworkWebSerial::_enabled = false;
std::function<void(const String&)> DeviceFrameworkWebSerial::_messageCallback = nullptr;
DeviceFrameworkCircularBuffer* DeviceFrameworkWebSerial::_buffer = nullptr;
std::vector<ClientState> DeviceFrameworkWebSerial::_clientStates;
unsigned long DeviceFrameworkWebSerial::_lastFlushTime = 0;
unsigned long DeviceFrameworkWebSerial::_lastClientCheckTime = 0;
unsigned long DeviceFrameworkWebSerial::_lastCleanupTime = 0;

void DeviceFrameworkWebSerial::begin(AsyncWebServer* server, const char* url) {
    if (!server) {
        LOG_ERRORLN(F("WebSerialTransport: Server is null"));
        return;
    }

    _ws = new AsyncWebSocket(url ? url : "/webserial");
    server->addHandler(_ws);

    // Initialize timing
    _lastFlushTime = millis();
    _lastClientCheckTime = millis();
    _lastCleanupTime = millis();

    _enabled = true;

    LOG_INFOLN(F("WebSerialTransport: WebSocket initialized with on-demand buffering"));
}

void DeviceFrameworkWebSerial::setAuthentication(const char* username, const char* password) {
    if (_ws) {
        _ws->setAuthentication(username, password);
    }
}

void DeviceFrameworkWebSerial::send(const String& message) {
    send(message.c_str(), message.length());
}

void DeviceFrameworkWebSerial::send(const char* message) {
    send(message, strlen(message));
}

void DeviceFrameworkWebSerial::send(const char* message, size_t length) {
    if (!_enabled || !_ws || !hasConnections() || length == 0) {
        return;
    }

    if (!ensureBuffer()) {
        return;
    }

    // Add to buffer instead of sending directly
    addToBuffer(message, length);

    // Check if we should flush immediately
    if (shouldFlushImmediately()) {
        flushBuffer();
    }
}

bool DeviceFrameworkWebSerial::isEnabled() {
    return _enabled;
}

bool DeviceFrameworkWebSerial::hasConnections() {
    return _ws && _ws->count() > 0;
}

void DeviceFrameworkWebSerial::loop() {
    if (!_enabled || !_ws) {
        return;
    }

    unsigned long now = millis();

    // Periodic buffer flush
    if (_buffer && TimeUtils::hasTimeElapsed(now, _lastFlushTime, getConfigWSSendInterval())) {
        if (_buffer->getDataLength() > 0) {
            flushBuffer();
        }
    }

    // Client cleanup (less frequent now with better buffering)
    if (now - _lastCleanupTime >= getConfigWSCleanupInterval()) {
        _ws->cleanupClients();
        if (!hasConnections() && _buffer && _buffer->getDataLength() == 0) {
            delete _buffer;
            _buffer = nullptr;
            _clientStates.clear();
        }
        _lastCleanupTime = now;
    }
}

void DeviceFrameworkWebSerial::setMessageCallback(std::function<void(const String&)> callback) {
    _messageCallback = callback;
}

void DeviceFrameworkWebSerial::end() {
    if (!_enabled || !_ws) {
        return;
    }

    LOG_DEBUGLN(F("WebSerialTransport: Cleaning up resources..."));

    // Flush any remaining buffered messages before cleanup
    if (_buffer && _buffer->getDataLength() > 0) {
        flushBuffer();
    }

    // Close all WebSocket connections first
    _ws->closeAll();

    // Clean up any remaining clients
    _ws->cleanupClients();

    // Clean up buffer
    if (_buffer) {
        delete _buffer;
        _buffer = nullptr;
    }

    // Clear client states
    _clientStates.clear();

    // Reset state
    _enabled = false;
    _messageCallback = nullptr;

    // The server owns the WebSocket handler. Clear our non-owning pointer
    // before the server is deleted so a later setup cannot use stale memory.
    _ws = nullptr;

    LOG_DEBUGLN(F("WebSerialTransport: Cleaned up resources"));
}

// Helper method implementations
bool DeviceFrameworkWebSerial::ensureBuffer() {
    if (_buffer) {
        return _buffer->isReady();
    }

    DeviceFrameworkCircularBuffer* buffer =
        new (std::nothrow) DeviceFrameworkCircularBuffer(getConfigWSBufferSize());
    if (!buffer || !buffer->isReady()) {
        delete buffer;
        LOG_ERRORLN(F("WebSerialTransport: Failed to allocate circular buffer"));
        return false;
    }

    _buffer = buffer;
    return true;
}

void DeviceFrameworkWebSerial::updateClientStates() {
    if (!_ws) return;

    _clientStates.clear();

    // Get all connected clients
    auto& clients = _ws->getClients();
    for (auto& client : clients) {
        if (client.status() == WS_CONNECTED) {
            ClientState state;
            state.id = client.id();
            state.queueIsFull = client.queueIsFull();
            state.canSend = client.canSend();
            state.queueLen = client.queueLen();
            state.lastCheck = millis();

            _clientStates.push_back(state);
        }
    }
}

bool DeviceFrameworkWebSerial::canAnyClientAccept() {
    // Check client states (throttled)
    if (TimeUtils::hasTimeElapsed(millis(), _lastClientCheckTime, getConfigWSClientCheckInterval())) {
        updateClientStates();
        _lastClientCheckTime = millis();
    }

    // Return true if any client can accept
    for (const auto& client : _clientStates) {
        if (client.canSend && !client.queueIsFull) {
            return true;
        }
    }
    return false;
}

bool DeviceFrameworkWebSerial::shouldFlushImmediately() {
    if (!_buffer) return false;

    // Flush if clients can accept (client handles newlines)
    if (canAnyClientAccept() && _buffer->getDataLength() > 0) {
        // Flush more aggressively if buffer is getting full
        if (_buffer->isNearFull()) {
            return true;
        }
    }

    return false;
}

void DeviceFrameworkWebSerial::flushBuffer() {
    if (!_buffer || !_ws || _buffer->getDataLength() == 0) return;

    // Extract all buffered data (client will handle newlines)
    String bufferedData = _buffer->extractAll();

    if (bufferedData.length() > 0) {
        // Send with status checking
        AsyncWebSocket::SendStatus status = _ws->textAll(bufferedData);

        // Handle different statuses
        switch (status) {
            case AsyncWebSocket::DISCARDED:
                // All clients rejected - back off
                _lastFlushTime = millis() + getConfigWSBackoffDelay();
                break;
            case AsyncWebSocket::PARTIALLY_ENQUEUED:
                // Some clients accepted - normal timing
                _lastFlushTime = millis();
                break;
            case AsyncWebSocket::ENQUEUED:
                // All clients accepted - can flush more aggressively
                _lastFlushTime = millis();
                break;
        }
    }
}

void DeviceFrameworkWebSerial::addToBuffer(const char* message, size_t length) {
    if (!_buffer) return;

    // Try to add to buffer
    if (!_buffer->add(message, length)) {
        // Buffer full - force flush what we have
        flushBuffer();

        // Try again
        _buffer->add(message, length);
    }
}

// Function to send debug output to WebSocket (called from WebInterface)
bool shouldSendDebugToWebSocket() {
    return DeviceFrameworkWebSerial::isEnabled() && DeviceFrameworkWebSerial::hasConnections();
}

void sendDebugToWebSocket(const String& message) {
    if (!shouldSendDebugToWebSocket()) {
        return;
    }

    // Send all messages - let the WebSocket transport handle rate limiting
    DeviceFrameworkWebSerial::send(message);
}

#endif // ENABLE_WEB_INTERFACE