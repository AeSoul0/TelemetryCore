/**
 * platform/include/frame.h
 * * Architecture Layer: Platform / Link-layer framing
 * Responsibility: Encodes and decodes the NMEA-like frames used on the serial link:
 *
 *     $<payload>*<XOR8 checksum, 2 uppercase hex digits>\r\n
 *
 * The checksum is the XOR of every payload byte (the characters between '$' and '*').
 * '$', '*', CR and LF are reserved; encode() replaces them in the payload with '_'.
 * The full protocol is documented in docs/protocol.md.
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace frame {

// Longest payload accepted by decode() and produced by the system.
constexpr size_t kMaxPayloadLength = 160;
// '$' + payload + '*' + 2 hex + "\r\n" + NUL
constexpr size_t kMaxFrameLength = kMaxPayloadLength + 7;

uint8_t checksum(const char* payload, size_t length);

// Writes a NUL-terminated frame into out. Returns the frame length (without NUL),
// or 0 if out is too small.
size_t encode(const char* payload, char* out, size_t outSize);

// Validates a received line (trailing CR/LF optional) and copies the payload into payloadOut.
// Returns false on a missing '$', missing '*', bad checksum or an undersized output buffer.
bool decode(const char* line, char* payloadOut, size_t payloadOutSize);

} // namespace frame
