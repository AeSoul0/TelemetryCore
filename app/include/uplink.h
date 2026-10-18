/**
 * app/include/uplink.h
 * * Architecture Layer: Application / Communication
 * Responsibility: The single owner of the serial link. Frames payloads ($...*CS\r\n) and
 *                 serialises access with a mutex, so telemetry and log frames from different
 *                 tasks never interleave on the wire.
 */
#pragma once
#include <atomic>
#include <cstdint>

#include "error_codes.h"
#include "frame.h"
#include "osal/osal.h"
#include "serial_port.h"

class Uplink {
  public:
    static constexpr uint32_t kWriteTimeoutMs = 50;

    ErrorCode init(SerialPort& port);
    // Thread-safe. Must not be called from an ISR.
    ErrorCode sendPayload(const char* payload);

    uint32_t errorCount() const { return errors_.load(); }

  private:
    SerialPort* port_ = nullptr;
    osal::Mutex mutex_;
    char frameBuffer_[frame::kMaxFrameLength]{}; // guarded by mutex_
    std::atomic<uint32_t> errors_{0};
};
