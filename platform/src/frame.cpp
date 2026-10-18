/**
 * platform/src/frame.cpp
 * * Architecture Layer: Platform / Link-layer framing
 * Responsibility: Implements frame encoding, decoding and checksum.
 */
#include "frame.h"

#include <cstring>

namespace {

constexpr char kHexDigits[] = "0123456789ABCDEF";

bool isReserved(char c) {
    return c == '$' || c == '*' || c == '\r' || c == '\n';
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

} // namespace

namespace frame {

uint8_t checksum(const char* payload, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum = static_cast<uint8_t>(sum ^ static_cast<uint8_t>(payload[i]));
    }
    return sum;
}

size_t encode(const char* payload, char* out, size_t outSize) {
    const size_t payloadLength = std::strlen(payload);
    const size_t frameLength = payloadLength + 6; // '$' '*' H H '\r' '\n'
    if (out == nullptr || frameLength + 1 > outSize) {
        return 0;
    }

    out[0] = '$';
    for (size_t i = 0; i < payloadLength; ++i) {
        out[1 + i] = isReserved(payload[i]) ? '_' : payload[i];
    }
    const uint8_t sum = checksum(out + 1, payloadLength);
    char* tail = out + 1 + payloadLength;
    tail[0] = '*';
    tail[1] = kHexDigits[(sum >> 4U) & 0x0FU];
    tail[2] = kHexDigits[sum & 0x0FU];
    tail[3] = '\r';
    tail[4] = '\n';
    tail[5] = '\0';
    return frameLength;
}

bool decode(const char* line, char* payloadOut, size_t payloadOutSize) {
    if (line == nullptr || line[0] != '$' || payloadOut == nullptr) {
        return false;
    }
    size_t length = std::strlen(line);
    while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n')) {
        --length;
    }
    // Minimum frame: "$*HH"
    if (length < 4 || line[length - 3] != '*') {
        return false;
    }
    const int high = hexValue(line[length - 2]);
    const int low = hexValue(line[length - 1]);
    if (high < 0 || low < 0) {
        return false;
    }
    const size_t payloadLength = length - 4;
    if (payloadLength + 1 > payloadOutSize) {
        return false;
    }
    const auto expected = static_cast<uint8_t>((high << 4) | low);
    if (checksum(line + 1, payloadLength) != expected) {
        return false;
    }
    std::memcpy(payloadOut, line + 1, payloadLength);
    payloadOut[payloadLength] = '\0';
    return true;
}

} // namespace frame
