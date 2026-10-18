/**
 * drivers/host/src/stdout_serial.cpp
 * * Architecture Layer: Driver / Host implementation
 * Responsibility: Writes raw bytes to stdout and flushes, like a UART would transmit them.
 */
#include "stdout_serial.h"

#include <cstdio>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

ErrorCode StdoutSerial::init() {
#if defined(_WIN32)
    // Frames already end with "\r\n": stop the C runtime from turning '\n' into "\r\n" again.
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    return ErrorCode::OK;
}

ErrorCode StdoutSerial::write(const uint8_t* data, size_t length, uint32_t /*timeoutMs*/) {
    const size_t written = std::fwrite(data, 1, length, stdout);
    // Pipes are block-buffered: flush so every frame reaches the reader immediately.
    (void)std::fflush(stdout);
    return (written == length) ? ErrorCode::OK : ErrorCode::COMMUNICATION_ERROR;
}
