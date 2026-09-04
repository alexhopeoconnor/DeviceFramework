#ifdef ENABLE_WEB_INTERFACE
#include "DeviceFrameworkWebSerial.h"
#include "../Utils/TimeUtils.h"
#include "../DeviceFrameworkDebug.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include "DeviceFrameworkWebAdmissionControl.h"
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


size_t DeviceFrameworkCircularBuffer::getDataLength() const {
    return _dataLength;
}
const char* DeviceFrameworkCircularBuffer::getContiguousData() const {
    return _dataLength == 0 || _buffer == nullptr ? nullptr : _buffer + _readPos;
}

size_t DeviceFrameworkCircularBuffer::getContiguousDataLength() const {
    return min(getContiguousReadSpace(), _dataLength);
}

void DeviceFrameworkCircularBuffer::consume(size_t length) {
    const size_t consumed = min(length, _dataLength);
    if (consumed == 0 || _bufferSize == 0) {
        return;
    }
    _readPos = (_readPos + consumed) % _bufferSize;
    _dataLength -= consumed;
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
ClientState DeviceFrameworkWebSerial::_clientStates[
    DeviceFrameworkWebAdmissionControl::kMaximumTrackedWebSerialClients] = {};
uint8_t DeviceFrameworkWebSerial::_clientStateCount = 0;
unsigned long DeviceFrameworkWebSerial::_lastFlushTime = 0;
unsigned long DeviceFrameworkWebSerial::_lastClientCheckTime = 0;
unsigned long DeviceFrameworkWebSerial::_lastCleanupTime = 0;

bool DeviceFrameworkWebSerial::isExplicitTakeoverRequest(AsyncWebServerRequest* request) {
    return request != nullptr && request->hasParam("takeover") &&
           request->getParam("takeover")->value() == "1";
}


void DeviceFrameworkWebSerial::handleWebSocketEvent(AsyncWebSocket* server,
                                                     AsyncWebSocketClient* client,
                                                     AwsEventType type, void* arg,
                                                     uint8_t*, size_t) {
    if (client == nullptr) {
        return;
    }

    if (type == WS_EVT_CONNECT) {
        AsyncWebServerRequest* request = static_cast<AsyncWebServerRequest*>(arg);
        uint32_t evictedClientId = 0;
        const WebSerialAdmissionResult admission =
            DeviceFrameworkWebAdmissionControl::admitWebSerial(
                client->id(), isExplicitTakeoverRequest(request),
                &evictedClientId);
        if (admission == WebSerialAdmissionResult::RejectedCapacity ||
            admission == WebSerialAdmissionResult::RejectedMemory) {
            client->close(1013, "WebSerial busy");
            return;
        }

        // Serial output is diagnostic, so dropping a line is preferable to a
        // full queue closing the socket and making the browser reconnect.
        client->setCloseClientOnQueueFull(false);
        if (evictedClientId != 0 && server != nullptr) {
            server->close(evictedClientId, 4001, "WebSerial replaced");
        }
        return;
    }

    if (type == WS_EVT_DISCONNECT) {
        DeviceFrameworkWebAdmissionControl::releaseWebSerial(client->id());
    }
}
void DeviceFrameworkWebSerial::begin(AsyncWebServer* server, const char* url) {
    if (!server) {
        LOG_ERRORLN(F("WebSerialTransport: Server is null"));
        return;
    }

    _ws = new (std::nothrow) AsyncWebSocket(url ? url : "/webserial");
    if (_ws == nullptr) {
        LOG_ERRORLN(F("WebSerialTransport: Failed to allocate WebSocket"));
        return;
    }
    _ws->onEvent(DeviceFrameworkWebSerial::handleWebSocketEvent);
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
    return _ws && DeviceFrameworkWebAdmissionControl::activeWebSerialClients() > 0;
}

void DeviceFrameworkWebSerial::loop() {
    if (!_enabled || !_ws) {
        return;
    }

    unsigned long now = millis();

    // AsyncWebServer can retain a disconnected client object until its later
    // housekeeping pass. Refresh our independent admission records promptly:
    // an already-closed diagnostic tab must not consume a WebSerial slot.
    if (DeviceFrameworkWebAdmissionControl::activeWebSerialClients() > 0 &&
        TimeUtils::hasTimeElapsed(now, _lastClientCheckTime,
                                  getConfigWSClientCheckInterval())) {
        updateClientStates();
        _lastClientCheckTime = now;
    }

    // Periodic buffer flush
    if (_buffer && TimeUtils::hasTimeElapsed(now, _lastFlushTime, getConfigWSSendInterval())) {
        if (_buffer->getDataLength() > 0) {
            flushBuffer();
        }
    }

    // Keep the bounded client view current and shed one low-priority serial
    // client only if the configured critical-memory watermark is crossed.
    if (now - _lastCleanupTime >= getConfigWSCleanupInterval()) {
        updateClientStates();
        uint32_t evictedClientId = 0;
        if (DeviceFrameworkWebAdmissionControl::shouldShedWebSerialClient(&evictedClientId) &&
            evictedClientId != 0) {
            _ws->close(evictedClientId, 1013, "Device memory pressure");
        }
        // Admission owns the policy. Passing the upstream default here would
        // silently evict the oldest client above its unrelated limit.
        _ws->cleanupClients(0xFFFFU);
        if (!hasConnections() && _buffer && _buffer->getDataLength() == 0) {
            delete _buffer;
            _buffer = nullptr;
            _clientStateCount = 0;
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
    _ws->cleanupClients(0xFFFFU);

    // Clean up buffer
    if (_buffer) {
        delete _buffer;
        _buffer = nullptr;
    }

    // Clear client states
    _clientStateCount = 0;

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

    _clientStateCount = 0;
    const unsigned long checkedAt = millis();
    bool releasedDisconnectedClient = false;

    // Refresh fixed-capacity state without allocating a vector on a busy web server.
    auto& clients = _ws->getClients();
    for (auto& client : clients) {
        if (client.status() != WS_CONNECTED) {
            DeviceFrameworkWebAdmissionControl::releaseWebSerial(client.id());
            releasedDisconnectedClient = true;
            continue;
        }
        if (_clientStateCount >= DeviceFrameworkWebAdmissionControl::kMaximumTrackedWebSerialClients) {
            client.close(1013, "WebSerial capacity");
            continue;
        }

        ClientState& state = _clientStates[_clientStateCount++];
        state.id = client.id();
        state.queueIsFull = client.queueIsFull();
        state.canSend = client.canSend();
        state.queueLen = client.queueLen();
        state.lastCheck = checkedAt;
        DeviceFrameworkWebAdmissionControl::updateWebSerialClient(
            state.id, state.queueIsFull, state.canSend, state.queueLen);
    }
    if (releasedDisconnectedClient) {
        // Remove only already-disconnected transport objects. A large limit
        // avoids AsyncWebServer's unrelated oldest-client eviction policy.
        _ws->cleanupClients(0xFFFFU);
    }
}

bool DeviceFrameworkWebSerial::canAnyClientAccept() {
    // Check client states (throttled)
    if (TimeUtils::hasTimeElapsed(millis(), _lastClientCheckTime, getConfigWSClientCheckInterval())) {
        updateClientStates();
        _lastClientCheckTime = millis();
    }

    // Return true if any client can accept
    for (uint8_t index = 0; index < _clientStateCount; ++index) {
        const ClientState& client = _clientStates[index];
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

    if (!hasConnections()) {
        DeviceFrameworkWebAdmissionControl::recordDroppedWebSerialBytes(_buffer->getDataLength());
        _buffer->clear();
        return;
    }

    if (DeviceFrameworkWebAdmissionControl::isCriticalMemoryPressure()) {
        DeviceFrameworkWebAdmissionControl::recordDroppedWebSerialBytes(_buffer->getDataLength());
        _buffer->clear();
        _lastFlushTime = millis() + getConfigWSBackoffDelay();
        return;
    }

    const char* chunk = _buffer->getContiguousData();
    const size_t chunkLength = _buffer->getContiguousDataLength();
    if (chunk == nullptr || chunkLength == 0) return;

    const AsyncWebSocket::SendStatus status = _ws->textAll(chunk, chunkLength);
    _buffer->consume(chunkLength);
    if (status == AsyncWebSocket::DISCARDED) {
        DeviceFrameworkWebAdmissionControl::recordDroppedWebSerialBytes(chunkLength);
        _lastFlushTime = millis() + getConfigWSBackoffDelay();
        return;
    }

    _lastFlushTime = millis();
}

void DeviceFrameworkWebSerial::addToBuffer(const char* message, size_t length) {
    if (!_buffer) return;

    // Try to add to buffer
    if (!_buffer->add(message, length)) {
        // Buffer full - force flush what we have
        flushBuffer();

        if (!_buffer->add(message, length)) {
            DeviceFrameworkWebAdmissionControl::recordDroppedWebSerialBytes(length);
        }
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