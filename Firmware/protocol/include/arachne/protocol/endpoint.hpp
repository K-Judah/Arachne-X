#pragma once
#include "arachne/actuator.hpp"
#include "arachne/protocol/transport.hpp"

namespace arachne::protocol {
enum class Error : std::uint8_t {
    Ok = 0, BadPayload = 1, UnsupportedVersion = 2, WrongDirection = 3, NoSession = 4, BadSession = 5,
    BadSequence = 6, StaleToken = 7, NotConfigured = 8, ConfigMismatch = 9, Disabled = 10, PhysicalLocked = 11,
    InvalidJoint = 12, InvalidValue = 13, OutOfRange = 14, Uncalibrated = 15, InvalidConfiguration = 16,
    DuplicateJoint = 17, Stopped = 18, ActuatorFailure = 19
};
enum class Fault : std::uint8_t {
    None = 0, EmergencyStop = 1, HeartbeatTimeout = 2, CommandTimeout = 3, Clock = 4, Transport = 5, Actuator = 6
};
struct Settings {
    std::optional<std::uint64_t> heartbeat_timeout_us;
    std::optional<std::uint64_t> command_timeout_us;
    std::optional<std::uint64_t> frame_timeout_us;
    // Deployment must supply a fresh boot/session namespace and a revision tied
    // to the immutable actuator configuration. No production defaults exist.
    std::optional<std::uint64_t> session_seed;
    std::optional<std::uint32_t> configuration_tag;
};

class Endpoint {
public:
    Endpoint(ActuatorSystem& actuators, const Clock& clock, Settings settings);
    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;
    void poll(Transport& transport); // bounded: at most kMaxWire input bytes
    void tick(); // must also be called when there is no traffic
    void disconnect();
    Fault fault() const;
    bool session_active() const { return active_; }
    std::uint32_t malformed_count() const { return malformed_; }
private:
    Frame dispatch(const Frame& request);
    Frame reply(const Frame& request, Type type) const;
    Frame outcome(const Frame& request, Error error, Type success = Type::Ack) const;
    Frame status(const Frame& request) const;
    void invalidate(Fault reason);
    bool configured() const;
    void record_error();
    ActuatorSystem& actuators_;
    const Clock& clock_;
    const Settings settings_;
    Parser parser_;
    bool active_{false};
    Fault fault_{Fault::None};
    std::uint64_t session_{0}, next_session_{0};
    std::uint32_t sequence_{0}, token_{0}, malformed_{0};
    std::optional<std::uint64_t> last_clock_, partial_since_;
    std::uint64_t heartbeat_at_{0}, command_at_{0}, token_at_{0};
    std::array<std::optional<double>, kJointCount> commanded_{};
};
} // namespace arachne::protocol
