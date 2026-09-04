#ifndef DEVICEFRAMEWORK_WEB_SERIAL_H
#define DEVICEFRAMEWORK_WEB_SERIAL_H

#ifdef ENABLE_WEB_INTERFACE
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>

// Circular buffer for WebSocket message buffering
class DeviceFrameworkCircularBuffer {
public:
    DeviceFrameworkCircularBuffer(size_t size);
    ~DeviceFrameworkCircularBuffer();

    // Add data to buffer
    bool add(const char* data, size_t length);


    // Access and consume the next contiguous slice without materialising a
    // second heap String. The caller must consume exactly what it sent.
    const char* getContiguousData() const;
    size_t getContiguousDataLength() const;
    void consume(size_t length);

    // Get available space
    size_t getAvailableSpace() const;

    // Get current data length
    size_t getDataLength() const;

    // Check if buffer is getting full
    bool isNearFull() const;

    // True when backing storage was allocated successfully.
    bool isReady() const;

    // Clear buffer
    void clear();

private:
    char* _buffer;
    size_t _bufferSize;
    size_t _writePos;
    size_t _readPos;
    size_t _dataLength;

    // Helper methods
    size_t getContiguousWriteSpace() const;
    size_t getContiguousReadSpace() const;
};

// Client state tracking
struct ClientState {
    uint32_t id;
    bool queueIsFull;
    bool canSend;
    size_t queueLen;
    unsigned long lastCheck;

    ClientState() : id(0), queueIsFull(false), canSend(true), queueLen(0), lastCheck(0) {}
};

class DeviceFrameworkWebSerial {
public:
    // Initialization
    static void begin(AsyncWebServer* server, const char* url = "/webserial");
    static void setAuthentication(const char* username, const char* password);

    // Send methods
    static void send(const String& message);
    static void send(const char* message);
    static void send(const char* message, size_t length);

    // Status
    static bool isEnabled();
    static bool hasConnections();
    static void loop(); // Call in main loop for maintenance

    // Cleanup
    static void end(); // Clean up resources

    // Message handling
    static void setMessageCallback(std::function<void(const String&)> callback);

private:
    static AsyncWebSocket* _ws;
    static bool _enabled;
    static std::function<void(const String&)> _messageCallback;

    // Buffering system
    static DeviceFrameworkCircularBuffer* _buffer;
    static ClientState _clientStates[];
    static uint8_t _clientStateCount;

    // Timing
    static unsigned long _lastFlushTime;
    static unsigned long _lastClientCheckTime;
    static unsigned long _lastCleanupTime;

    // Helper methods
    static void updateClientStates();
    static void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                     AwsEventType type, void* arg, uint8_t* data, size_t len);
    static bool isExplicitTakeoverRequest(AsyncWebServerRequest* request);
    static bool ensureBuffer();
    static bool canAnyClientAccept();
    static bool shouldFlushImmediately();
    static void flushBuffer();
    static void addToBuffer(const char* message, size_t length);

};

#endif // ENABLE_WEB_INTERFACE
#endif // DEVICEFRAMEWORK_WEB_SERIAL_H
