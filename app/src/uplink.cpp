/**
 * app/src/uplink.cpp
 * * Architecture Layer: Application / Communication Implementation
 * Responsibility: Frames payloads and writes them to the serial port under a mutex.
 */
#include "uplink.h"

ErrorCode Uplink::init(SerialPort& port) {
    const ErrorCode mutexResult = mutex_.create();
    if (mutexResult != ErrorCode::OK) {
        return mutexResult;
    }
    const ErrorCode portResult = port.init();
    if (portResult != ErrorCode::OK) {
        return portResult;
    }
    port_ = &port;
    return ErrorCode::OK;
}

ErrorCode Uplink::sendPayload(const char* payload) {
    if (port_ == nullptr) {
        return ErrorCode::NOT_INITIALIZED;
    }
    osal::LockGuard lock(mutex_);
    const size_t length = frame::encode(payload, frameBuffer_, sizeof(frameBuffer_));
    if (length == 0) {
        errors_.fetch_add(1);
        return ErrorCode::BUFFER_TOO_SMALL;
    }
    // Do not log from here: the logger itself writes through this function.
    const ErrorCode result = port_->write(reinterpret_cast<const uint8_t*>(frameBuffer_), length, kWriteTimeoutMs);
    if (result != ErrorCode::OK) {
        errors_.fetch_add(1);
    }
    return result;
}
