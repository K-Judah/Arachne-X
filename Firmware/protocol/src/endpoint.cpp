#include "arachne/protocol/endpoint.hpp"

#include <limits>

namespace arachne::protocol {
static_assert(static_cast<unsigned>(ActuatorState::Disabled) == 0 &&
              static_cast<unsigned>(ActuatorState::SimulationEnabled) == 1 &&
              static_cast<unsigned>(ActuatorState::Fault) == 2, "Protocol v1 state values must remain stable");
namespace {
Error command_error(CommandResult result) {
    switch (result) {
    case CommandResult::Accepted: return Error::Ok;
    case CommandResult::Disabled: return Error::Disabled;
    case CommandResult::FaultLatched: return Error::Stopped;
    case CommandResult::PhysicalOutputLocked: return Error::PhysicalLocked;
    case CommandResult::InvalidConfiguration: return Error::InvalidConfiguration;
    case CommandResult::InvalidJoint: return Error::InvalidJoint;
    case CommandResult::InvalidTarget: return Error::InvalidValue;
    case CommandResult::OutOfRange: return Error::OutOfRange;
    case CommandResult::Uncalibrated: return Error::Uncalibrated;
    default: return Error::ActuatorFailure;
    }
}
bool elapsed(std::uint64_t now, std::uint64_t since, std::uint64_t duration) {
    return now < since || now - since >= duration;
}
} // namespace

Endpoint::Endpoint(ActuatorSystem& actuators, const Clock& clock, Settings settings)
    : actuators_(actuators), clock_(clock), settings_(settings),
      next_session_(settings.session_seed.value_or(0)) {
    actuators_.disable(); // acquiring a protocol owner never inherits enablement
}
bool Endpoint::configured() const {
    return settings_.heartbeat_timeout_us && *settings_.heartbeat_timeout_us > 0 &&
           settings_.command_timeout_us && *settings_.command_timeout_us > 0 &&
           settings_.frame_timeout_us && *settings_.frame_timeout_us > 0 &&
           settings_.session_seed && *settings_.session_seed > 0 && settings_.configuration_tag &&
           *settings_.configuration_tag > 0;
}
Fault Endpoint::fault() const {
    if (fault_ == Fault::EmergencyStop) return fault_;
    return actuators_.state() == ActuatorState::Fault ? Fault::Actuator : fault_;
}
void Endpoint::record_error() {
    if (malformed_ != std::numeric_limits<std::uint32_t>::max()) ++malformed_;
}
void Endpoint::invalidate(Fault reason) {
    actuators_.disable();
    active_ = false;
    session_ = 0;
    if (fault_ != Fault::EmergencyStop) fault_ = reason;
    if (parser_.finish() == DecodeError::Truncated) record_error();
    partial_since_.reset();
}
void Endpoint::disconnect() { invalidate(Fault::Transport); }
void Endpoint::tick() {
    const auto now = clock_.now_us();
    if (!now && !last_clock_ && !active_) {
        // Once clock loss has disarmed the session, keep parsing bounded frames
        // so status, DISABLE and E-stop remain available without a clock.
        actuators_.disable();
        if (fault_ != Fault::EmergencyStop) fault_ = Fault::Clock;
        return;
    }
    if (!now || (last_clock_ && *now < *last_clock_)) {
        invalidate(Fault::Clock);
        last_clock_ = now;
        return;
    }
    last_clock_ = now;
    if (partial_since_ && settings_.frame_timeout_us &&
        elapsed(*now, *partial_since_, *settings_.frame_timeout_us)) {
        invalidate(Fault::Transport);
        return;
    }
    if (!active_) return;
    if (elapsed(*now, heartbeat_at_, *settings_.heartbeat_timeout_us)) {
        invalidate(Fault::HeartbeatTimeout);
    } else if (actuators_.state() == ActuatorState::SimulationEnabled &&
               elapsed(*now, command_at_, *settings_.command_timeout_us)) {
        invalidate(Fault::CommandTimeout);
    }
}
void Endpoint::poll(Transport& transport) {
    tick();
    for (std::size_t i = 0; i < kMaxWire; ++i) {
        std::uint8_t byte = 0;
        const auto read = transport.read(byte);
        if (read == ReadResult::Empty) break;
        if (read == ReadResult::Closed) { disconnect(); break; }
        // Enforce deadlines before consuming even the terminating delimiter.
        // A slow owner/adapter must not hide frame expiry by completing a frame.
        tick();
        if (byte != 0 && !partial_since_) partial_since_ = clock_.now_us();
        Frame request;
        const auto decoded = parser_.push(byte, request);
        if (byte == 0) partial_since_.reset();
        if (decoded == DecodeError::Incomplete) continue;
        if (decoded != DecodeError::None) { record_error(); continue; }
        tick();
        EncodedFrame output;
        if (!encode(dispatch(request), output) || !transport.write(output.bytes.data(), output.size)) {
            invalidate(Fault::Transport);
            break;
        }
    }
    tick();
}
Frame Endpoint::reply(const Frame& request, Type type) const {
    Frame result;
    result.type = type;
    result.request = request.request;
    result.session = session_;
    result.token = active_ ? token_ : 0;
    return result;
}
Frame Endpoint::outcome(const Frame& request, Error error, Type success) const {
    auto result = reply(request, error == Error::Ok ? success : Type::Nack);
    result.length = 5;
    result.payload[0] = static_cast<std::uint8_t>(request.type);
    result.payload[1] = static_cast<std::uint8_t>(error);
    result.payload[2] = static_cast<std::uint8_t>(actuators_.state());
    result.payload[3] = static_cast<std::uint8_t>(fault());
    result.payload[4] = 0; // Physical permission is unconditionally false.
    return result;
}
Frame Endpoint::status(const Frame& request) const {
    auto result = reply(request, Type::Status);
    result.length = 249;
    auto* p = result.payload.data();
    p[0] = static_cast<std::uint8_t>(actuators_.state());
    p[1] = static_cast<std::uint8_t>(fault());
    p[2] = 0;
    p[3] = actuators_.validation().ok() ? 1 : 0;
    p[4] = active_ ? 1 : 0;
    p[5] = 0; // Configured controller mask, never hardware health.
    p[6] = static_cast<std::uint8_t>(kJointCount);
    put_uint(p + 7, settings_.configuration_tag.value_or(0), 4);
    put_uint(p + 11, malformed_, 4);
    for (std::size_t i = 0; i < kJointCount; ++i) {
        auto* record = p + 15 + i * 13;
        record[0] = static_cast<std::uint8_t>(i);
        record[1] = 255;
        record[2] = 255;
        record[3] = 0;
        record[4] = commanded_[i] ? 1 : 0;
        put_double(record + 5, commanded_[i].value_or(0)); // value valid only when flag=1
        for (const auto& slot : actuators_.configuration().joints) {
            if (!slot || slot->joint != kAllJoints[i]) continue;
            if (slot->wiring.controller && (*slot->wiring.controller == ControllerId::A ||
                                           *slot->wiring.controller == ControllerId::B)) {
                record[1] = static_cast<std::uint8_t>(*slot->wiring.controller);
                p[5] |= static_cast<std::uint8_t>(1u << record[1]);
            }
            if (slot->wiring.channel && *slot->wiring.channel >= 0 && *slot->wiring.channel < 16)
                record[2] = static_cast<std::uint8_t>(*slot->wiring.channel);
            record[3] = slot->calibration.state == CalibrationState::Calibrated ? 1 : 0;
            break;
        }
    }
    return result;
}
Frame Endpoint::dispatch(const Frame& request) {
    // All syntax/length checks precede even stop/disarm operations.
    const auto type = request.type;
    if (static_cast<std::uint8_t>(type) >= 128) return outcome(request, Error::WrongDirection);
    std::size_t expected = 0;
    switch (type) {
    case Type::Hello: expected = 2; break;
    case Type::EnableRequest: expected = 4; break;
    case Type::SetJointTarget: expected = 9; break;
    case Type::SetMultiJointTarget:
        if (request.length < 1 || request.payload[0] == 0 || request.payload[0] > kJointCount)
            return outcome(request, Error::BadPayload);
        expected = 1 + 9 * static_cast<std::size_t>(request.payload[0]);
        break;
    default: break;
    }
    if (request.length != expected) return outcome(request, Error::BadPayload);
    if (type == Type::EmergencyStop) {
        actuators_.emergency_stop();
        fault_ = Fault::EmergencyStop;
        invalidate(Fault::EmergencyStop);
        return outcome(request, Error::Ok);
    }
    if (type == Type::Disable) {
        invalidate(fault());
        return outcome(request, Error::Ok);
    }
    if (type == Type::GetStatus) return status(request);
    if (actuators_.state() == ActuatorState::Fault) return outcome(request, Error::Stopped);
    const auto now = clock_.now_us();
    if (!configured() || !now) return outcome(request, Error::NotConfigured);
    if (type == Type::Hello) {
        // HELLO always disarms; it cannot clear the actuator's latched fault.
        invalidate(Fault::None);
        if (request.payload[0] > kVersion || request.payload[1] < kVersion ||
            request.payload[0] > request.payload[1]) return outcome(request, Error::UnsupportedVersion);
        if (next_session_ == 0 || next_session_ == std::numeric_limits<std::uint64_t>::max() ||
            token_ == std::numeric_limits<std::uint32_t>::max()) return outcome(request, Error::NotConfigured);
        session_ = next_session_++;
        ++token_;
        sequence_ = request.request;
        heartbeat_at_ = token_at_ = command_at_ = *now;
        active_ = true;
        auto result = reply(request, Type::HelloAck);
        result.length = 22;
        result.payload[0] = kVersion;
        put_uint(result.payload.data() + 1, *settings_.configuration_tag, 4);
        put_uint(result.payload.data() + 5, *settings_.heartbeat_timeout_us, 8);
        put_uint(result.payload.data() + 13, *settings_.command_timeout_us, 8);
        result.payload[21] = 0;
        return result;
    }
    if (!active_) return outcome(request, Error::NoSession);
    if (request.session != session_) return outcome(request, Error::BadSession);
    if (request.request <= sequence_) return outcome(request, Error::BadSequence);
    if (request.token != token_) return outcome(request, Error::StaleToken);
    if (type != Type::Heartbeat && elapsed(*now, token_at_, *settings_.command_timeout_us))
        return outcome(request, Error::StaleToken);
    sequence_ = request.request; // semantic rejections also consume a request ID
    if (type == Type::Heartbeat) {
        if (token_ == std::numeric_limits<std::uint32_t>::max()) {
            invalidate(Fault::Transport);
            return outcome(request, Error::NotConfigured);
        }
        ++token_;
        heartbeat_at_ = token_at_ = *now;
        return outcome(request, Error::Ok, Type::HeartbeatReply);
    }
    if (type == Type::EnableRequest) {
        if (get_uint(request.payload.data(), 4) != *settings_.configuration_tag)
            return outcome(request, Error::ConfigMismatch);
        const auto result = actuators_.enable_simulation(); // existing lock remains authoritative
        if (result == CommandResult::Accepted) command_at_ = *now;
        return outcome(request, command_error(result));
    }
    const auto count = type == Type::SetJointTarget ? std::size_t{1} : request.payload[0];
    const auto offset = type == Type::SetJointTarget ? std::size_t{0} : std::size_t{1};
    std::array<JointId, kJointCount> joints{};
    std::array<double, kJointCount> targets{};
    std::array<bool, kJointCount> seen{};
    for (std::size_t i = 0; i < count; ++i) {
        joints[i] = static_cast<JointId>(request.payload[offset + i * 9]);
        targets[i] = get_double(request.payload.data() + offset + i * 9 + 1);
        const auto checked = actuators_.validate_target(joints[i], targets[i]);
        if (checked != CommandResult::Accepted) return outcome(request, command_error(checked));
        const auto index = static_cast<std::size_t>(joints[i]);
        if (seen[index]) return outcome(request, Error::DuplicateJoint);
        seen[index] = true;
    }
    if (actuators_.state() != ActuatorState::SimulationEnabled) return outcome(request, Error::Disabled);
    // Every target has passed preflight. Output failure during application is a
    // latched fault, not a promise of transactional hardware rollback.
    for (std::size_t i = 0; i < count; ++i) {
        const auto result = actuators_.command(joints[i], targets[i]);
        if (result != CommandResult::Accepted) {
            invalidate(Fault::Actuator);
            return outcome(request, command_error(result));
        }
        commanded_[static_cast<std::size_t>(joints[i])] = targets[i];
    }
    command_at_ = *now;
    return outcome(request, Error::Ok);
}
} // namespace arachne::protocol
