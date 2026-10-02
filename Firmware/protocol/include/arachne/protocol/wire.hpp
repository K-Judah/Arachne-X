#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arachne::protocol {
inline constexpr std::uint8_t kVersion = 1;
inline constexpr std::size_t kHeaderSize = 20;
inline constexpr std::size_t kMaxPayload = 256;
inline constexpr std::size_t kMaxRaw = kHeaderSize + kMaxPayload + 2;
inline constexpr std::size_t kMaxEncoded = kMaxRaw + kMaxRaw / 254 + 1;
inline constexpr std::size_t kMaxWire = kMaxEncoded + 1;

enum class Type : std::uint8_t {
    Hello = 1, Heartbeat = 2, GetStatus = 3, EnableRequest = 4, Disable = 5,
    SetJointTarget = 6, SetMultiJointTarget = 7, EmergencyStop = 8,
    HelloAck = 128, Ack = 129, Nack = 130, Status = 131, HeartbeatReply = 132
};
enum class DecodeError { None, Incomplete, Malformed, TooLarge, Truncated, Length, Crc, Version, Type };
struct Frame {
    std::uint8_t version{kVersion};
    Type type{Type::GetStatus};
    std::uint32_t request{0};
    std::uint64_t session{0};
    std::uint32_t token{0};
    std::uint16_t length{0};
    std::array<std::uint8_t, kMaxPayload> payload{};
};
struct EncodedFrame {
    std::array<std::uint8_t, kMaxWire> bytes{};
    std::size_t size{0};
};
bool known_type(Type type);
std::uint16_t crc16(const std::uint8_t* data, std::size_t size);
void put_uint(std::uint8_t* output, std::uint64_t value, std::size_t size);
std::uint64_t get_uint(const std::uint8_t* data, std::size_t size);
void put_double(std::uint8_t* output, double value);
double get_double(const std::uint8_t* data);
bool encode(const Frame& frame, EncodedFrame& output);
DecodeError decode(const std::uint8_t* data, std::size_t size, Frame& output);

// One byte per bounded step. Output is changed only for a complete valid frame.
class Parser {
public:
    DecodeError push(std::uint8_t byte, Frame& output);
    DecodeError finish(); // explicit end-of-stream / partial-frame deadline
private:
    std::array<std::uint8_t, kMaxWire> buffer_{};
    std::size_t used_{0};
    bool dropping_{false};
};
} // namespace arachne::protocol
