#include "arachne/protocol/wire.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace arachne::protocol {
bool known_type(Type type) {
    const auto v = static_cast<std::uint8_t>(type);
    return (v >= 1 && v <= 8) || (v >= 128 && v <= 132);
}
std::uint16_t crc16(const std::uint8_t* data, std::size_t size) {
    std::uint16_t crc = 0xffff;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[i]) << 8);
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
        }
    }
    return crc;
}
void put_uint(std::uint8_t* output, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) output[i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint64_t get_uint(const std::uint8_t* data, std::size_t size) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < size; ++i) value |= static_cast<std::uint64_t>(data[i]) << (i * 8);
    return value;
}
void put_double(std::uint8_t* output, double value) {
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    put_uint(output, bits, 8);
}
double get_double(const std::uint8_t* data) {
    const auto bits = get_uint(data, 8);
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
bool encode(const Frame& frame, EncodedFrame& output) {
    output.size = 0;
    if (frame.length > kMaxPayload || frame.version != kVersion || !known_type(frame.type)) return false;
    std::array<std::uint8_t, kMaxRaw> raw{};
    raw[0] = frame.version;
    raw[1] = static_cast<std::uint8_t>(frame.type);
    put_uint(raw.data() + 2, frame.request, 4);
    put_uint(raw.data() + 6, frame.session, 8);
    put_uint(raw.data() + 14, frame.token, 4);
    put_uint(raw.data() + 18, frame.length, 2);
    std::copy_n(frame.payload.begin(), frame.length, raw.begin() + kHeaderSize);
    const auto body_size = kHeaderSize + frame.length;
    put_uint(raw.data() + body_size, crc16(raw.data(), body_size), 2);
    std::size_t code_index = 0, write = 1;
    std::uint8_t code = 1;
    for (std::size_t i = 0; i < body_size + 2; ++i) {
        if (raw[i] == 0) {
            output.bytes[code_index] = code;
            code_index = write++;
            code = 1;
        } else {
            output.bytes[write++] = raw[i];
            if (++code == 0xff) {
                output.bytes[code_index] = code;
                code_index = write++;
                code = 1;
            }
        }
    }
    output.bytes[code_index] = code;
    output.bytes[write++] = 0;
    output.size = write;
    return true;
}
DecodeError decode(const std::uint8_t* data, std::size_t size, Frame& output) {
    if (size == 0 || data[size - 1] != 0) return DecodeError::Truncated;
    if (size > kMaxWire) return DecodeError::TooLarge;
    std::array<std::uint8_t, kMaxRaw> raw{};
    std::size_t read = 0, write = 0;
    const auto end = size - 1;
    while (read < end) {
        const auto code = data[read++];
        if (code == 0 || static_cast<std::size_t>(code - 1) > end - read) return DecodeError::Malformed;
        for (unsigned i = 1; i < code; ++i) {
            if (write >= raw.size()) return DecodeError::TooLarge;
            if (data[read] == 0) return DecodeError::Malformed;
            raw[write++] = data[read++];
        }
        if (code != 0xff && read < end) {
            if (write >= raw.size()) return DecodeError::TooLarge;
            raw[write++] = 0;
        }
    }
    if (write < kHeaderSize + 2) return DecodeError::Truncated;
    const auto length = get_uint(raw.data() + 18, 2);
    if (length > kMaxPayload || write != kHeaderSize + length + 2) return DecodeError::Length;
    if (crc16(raw.data(), write - 2) != get_uint(raw.data() + write - 2, 2)) return DecodeError::Crc;
    if (raw[0] != kVersion) return DecodeError::Version;
    if (!known_type(static_cast<Type>(raw[1]))) return DecodeError::Type;
    Frame result;
    result.type = static_cast<Type>(raw[1]);
    result.request = static_cast<std::uint32_t>(get_uint(raw.data() + 2, 4));
    result.session = get_uint(raw.data() + 6, 8);
    result.token = static_cast<std::uint32_t>(get_uint(raw.data() + 14, 4));
    result.length = static_cast<std::uint16_t>(length);
    std::copy_n(raw.begin() + kHeaderSize, result.length, result.payload.begin());
    output = result;
    return DecodeError::None;
}
DecodeError Parser::push(std::uint8_t byte, Frame& output) {
    if (dropping_) {
        if (byte == 0) dropping_ = false;
        return DecodeError::Incomplete;
    }
    if (byte == 0 && used_ == 0) return DecodeError::Incomplete;
    if (used_ == kMaxEncoded && byte != 0) {
        used_ = 0;
        dropping_ = true;
        return DecodeError::TooLarge;
    }
    buffer_[used_++] = byte;
    if (byte != 0) return DecodeError::Incomplete;
    const auto result = decode(buffer_.data(), used_, output);
    used_ = 0;
    return result;
}
DecodeError Parser::finish() {
    const bool partial = used_ != 0 || dropping_;
    used_ = 0;
    dropping_ = false;
    return partial ? DecodeError::Truncated : DecodeError::Incomplete;
}
} // namespace arachne::protocol
